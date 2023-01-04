#include "wireless_main.h"

#include "camera.h"
#include "data_common.h"
#include "debug_tools_conf.h"
#include "wireless_conf.h"

//
#include <sdkconfig.h>
//
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>
//
#include <esp_attr.h>
#include <esp_mesh_internal.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
//
#include <aes/esp_aes.h>
#include <esp_private/periph_ctrl.h>
#include <hal/aes_hal.h>
#include <hal/aes_ll.h>
#include <soc/dport_access.h>
#include <soc/hwcrypto_periph.h>
#include <soc/hwcrypto_reg.h>
#include <soc/periph_defs.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration

// Amount of packets in Queue what could be sent
#define WIFI_TX_PACKETS_NUM      (32)
#define WIFI_TX_PACKETS_NUM_MASK (WIFI_TX_PACKETS_NUM - 1)


#if WIRELESS_USE_RAW_80211_PACKET
typedef struct
{
	wifi_espnow_packet_t magic_packet;
	uint8_t ucData[250];
} wifi_espnow_raw_packet_t;
#endif // WIRELESS_USE_RAW_80211_PACKET

// ----------------------------------------------------------------------
// FreeRTOS Variables

#define STACK_WORDS_SIZE_FOR_TASK_DATA_TX (2048)
#define PRIORITY_LEVEL_FOR_TASK_DATA_TX   (1)
#define PINNED_CORE_FOR_TASK_DATA_TX      (0)
const char* assigned_name_for_task_data_tx = "data_tx";
TaskHandle_t xDataTransmitterTaskHandler = NULL;
StaticTask_t xDataTransmitterTaskControlBlock;
StackType_t xDataTransmitterStack[STACK_WORDS_SIZE_FOR_TASK_DATA_TX];

//
#define FRAME_PACKETS_QUEUE_SIZE (WIFI_TX_PACKETS_NUM)
QueueHandle_t xFramePacketQueueHandler = NULL;
StaticQueue_t xFramePacketQueueControlBlock;
uint32_t xFramePacketQueueStorage[WIFI_TX_PACKETS_NUM];

// #if(WIRELESS_USE_RAW_80211_PACKET == 0)
// //
// SemaphoreHandle_t xDataTransmitterTxLockHandler = NULL;
// StaticSemaphore_t xDataTransmitterTxLockControlBlock;
// #endif

// ----------------------------------------------------------------------
// Variables

// ----------------------------
// clang-format off
esp_now_peer_info_t xPeerNode = {
	{
		// rx node MAC in STA mode
		0
	},
	{
		// LMK
		0
	},
	DEFAULT_WIFI_CHANNEL,
	WIFI_IF_STA, // ifidx
	false,       // encrypt. If encryption is enabled it wouldn't be possible to parse raw 802.11 frames
	NULL
};
// clang-format on

#if WIRELESS_USE_RAW_80211_PACKET
// clang-format off
uint8_t ucDataBlob[] = {
	// .hdr
	0xd0,0x00, // .frame_ctrl
	0x00,0x00, // .duration_id  (from actual frame: 0x3a,0x01)
	0xcc,0x50,0xe3,0x96,0x57,0xdc, // .addr2[]
	0x7c,0xdf,0xa1,0xe7,0xe0,0x00, // .addr1[]
	0xff,0xff,0xff,0xff,0xff,0xff, // .addr3[]
	0xc0,0x44, // .sequence_ctrl
	0x7f, // .category_code
	0x18,0xfe,0x34, // .oui[]
	0xa9,0x8c,0xe3,0xfd, // .random

	// ->content.body:
	0xdd,  // element_id
	0xff, // length
	0x18,0xfe,0x34, // oui
	0x04, // type
	0x01 // version
};
// clang-format on

wifi_espnow_raw_packet_t wifi_espnow_raw_packet;
#endif

uint32_t ulFramePacketOffset = 0UL;
PacketFrame_t xPackets[WIFI_TX_PACKETS_NUM];

uint8_t ucEncryptedData[2][256];

// ----------------------------------------------------------------------
// Static functions declaration

#if WIFI_RX_DATA_CB_DBG_PRINTOUT
static void wifi_espnow_dump_playload(uint8_t* pucPayloadBuff);

static void wifi_espnow_dump_mac(const uint8_t* mac_addr);
#endif // WIFI_RX_DATA_CB_DBG_PRINTOUT


static void wifi_set_tx_power(int8_t ic_new_tx_power);


/**
 * @brief Send packet with ESP_NOW or as raw 802.11 blob
 * 
 * @param pxPacketFrame byte array with data need to be sent
 * 
 * @retval See @ref ''esp_err_t''
 */
static esp_err_t send_new_packet(const PacketFrame_t* pxPacketFrame);

/**
 * @brief This function decrypt data from @ref ''wifi_espnow_packet_rx_cb'' 
 *        or from @ref ''wifi_raw_packet_rx_cb'' callback and go through state machine.
 * 
 * @param data raw blob of data (which normally should be encrypted)
 */
static void wifi_espnow_parse_new_data(const uint8_t* data);

#if(WIRELESS_USE_RAW_80211_PACKET == 1)
/**
 * @brief Callback function from WiFi driver in promiscuous mode.
 * 
 * @param buf Raw blob of received data from WiFi interface
 * @param type Description of the data located in @ref ''buf''
 */
static void wifi_raw_packet_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type);
#endif // WIRELESS_USE_RAW_80211_PACKET

#if(WIRELESS_USE_RAW_80211_PACKET == 0)
/**
 * @brief Callback function from ESP-NOW when ned bunch of data is received.
 * 
 * @param mac_addr
 * @param data
 * @param data_len
 * 
 * @note This function is called ONLY when @ref ''WIRELESS_USE_RAW_80211_PACKET'' is disabled
 */
static void wifi_espnow_packet_rx_cb(const uint8_t* mac_addr, const uint8_t* data, int data_len);

/**
 * @brief Callback function from ESP-NOW when ned bunch of data is received.
 * 
 * @param mac_addr
 * @param status see @ref 'esp_now_send_status_t'
 * 
 * @note This function is called ONLY when @ref ''WIRELESS_USE_RAW_80211_PACKET'' is disabled
 */
// static void wifi_espnow_packet_tx_cb(const uint8_t* mac_addr, esp_now_send_status_t status);
#endif // !WIRELESS_USE_RAW_80211_PACKET

/**
 * @brief Creates FreeRTOS objects what need to maintain WiFi.
 */
static void init_wifi_rtos(void);

/**
 * @brief Send everything from @ref ''xFramePacketQueueHandler'' over wifi.
 * 
 * @param pvArg Argument for the task. (Not used and NULL is passed)
 * 
 * @note API what is used to send data depends on @ref ''WIRELESS_USE_RAW_80211_PACKET'' define
 */
static void vDataTransmitterTask(void* pvArg);

// ----------------------------------------------------------------------
// Static functions

#if WIFI_RX_DATA_CB_DBG_PRINTOUT
static void
wifi_espnow_dump_playload(uint8_t* pucPayloadBuff)
{

	static char cBuff[256];
	size_t offset = 0;

	for(size_t i = 0; i < 32; i++)
	{
		offset += sprintf(&cBuff[offset], "%02X ", pucPayloadBuff[i]);
	}

	ASYNC_PRINTF(1, async_print_type_str, "cBuff:\n", 0);
	ASYNC_PRINTF(1, async_print_type_str, cBuff, 0);
	ASYNC_PRINTF(1, async_print_type_str, "\n\n", 0);
}

static void
wifi_espnow_dump_mac(const uint8_t* mac_addr)
{
	static char macStr[20];
	snprintf(macStr,
	         sizeof(macStr),
	         "%02x:%02x:%02x:%02x:%02x:%02x\n",
	         mac_addr[0],
	         mac_addr[1],
	         mac_addr[2],
	         mac_addr[3],
	         mac_addr[4],
	         mac_addr[5]);

	ASYNC_PRINTF(1, async_print_type_str, "Last Packet Recv from: ", 0);
	ASYNC_PRINTF(1, async_print_type_str, macStr, 0);
}
#endif // WIFI_RX_DATA_CB_DBG_PRINTOUT

static void
wifi_set_tx_power(int8_t ic_new_tx_power)
{
	// Convert range
	// [WIFI_MIN_TX_POWER_PERCENTAGE : WIFI_MAX_TX_POWER_PERCENTAGE]
	// to
	// [WIFI_MIN_TX_POWER, WIFI_MAX_TX_POWER]
	int8_t ic_tx_Power = ul_map_val(ic_new_tx_power,
	                                WIFI_MIN_TX_POWER_PERCENTAGE,
	                                WIFI_MAX_TX_POWER_PERCENTAGE,
	                                WIFI_MIN_TX_POWER,
	                                WIFI_MAX_TX_POWER);
	ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(ic_tx_Power));
}


static esp_err_t IRAM_ATTR
send_new_packet(const PacketFrame_t* pxPacketFrame)
{
	PROFILE_POINT(ESP_NOW_TASK_PACKET_SEND_DBG_PROFILER, profile_point_start);

	const PacketFrame_t* pxPacketFrameToSend = NULL;
	uint32_t ulTxDataLen = sizeof(PacketHeader_t) + pxPacketFrame->xHeader.ucDataSize;

	if((BaseType_t)pxPacketFrame->xHeader.ucEncrypted == pdTRUE)
	{
		PacketFrame_t* pxPacketEncrypted = (PacketFrame_t*)&ucEncryptedData[0][0];
		pxPacketEncrypted->xHeader.ulValue = pxPacketFrame->xHeader.ulValue;

		wifi_crypt_packet(&pxPacketFrame->ucFrameData[0],
		                  &pxPacketEncrypted->ucFrameData[0],
		                  pxPacketEncrypted->xHeader.ucDataSize,
		                  ESP_AES_ENCRYPT);
		pxPacketFrameToSend = (const PacketFrame_t*)pxPacketEncrypted;
	}
	else
	{
		pxPacketFrameToSend = pxPacketFrame;
	}

#if(WIRELESS_USE_RAW_80211_PACKET == 1)
	// TODO: add random stuff
	// wifi_espnow_raw_packet.magic_packet.random = (uint32_t)
	wifi_espnow_raw_packet.magic_packet.content.length = ulTxDataLen;
	memcpy(wifi_espnow_raw_packet.magic_packet.content.body, pxPacketFrameToSend, ulTxDataLen);
	esp_err_t xRes =
	    esp_wifi_80211_tx(WIFI_IF_STA, &wifi_espnow_raw_packet, sizeof(wifi_espnow_packet_t) + ulTxDataLen, true);
#else
	// xSemaphoreTake(xDataTransmitterTxLockHandler, portMAX_DELAY);
	esp_err_t xRes = esp_now_send(NULL, (const uint8_t*)pxPacketFrameToSend, ulTxDataLen);
#endif

	PROFILE_POINT(ESP_NOW_TASK_PACKET_SEND_DBG_PROFILER, profile_point_end);

	if(ESP_OK != xRes)
	{
		// do something...
		ASYNC_PRINTF(
		    ESP_NOW_SEND_PACKET_FAIL_DBG_PRINTOUT, async_print_type_u32, "esp_now_send failed with %u\n", (uint32_t)xRes);
	}

	return xRes;
}

static void IRAM_ATTR
wifi_espnow_parse_new_data(const uint8_t* data)
{
	const PacketFrame_t* pxPacketFrame = (const PacketFrame_t*)data;

	PROFILE_POINT(ESP_NOW_RX_DATA_DBG_PROFILER, profile_point_start);

	if((BaseType_t)pxPacketFrame->xHeader.ucEncrypted == pdTRUE)
	{
		PacketFrame_t* pxPacketEncrypted = (PacketFrame_t*)&ucEncryptedData[1][0];
		pxPacketEncrypted->xHeader.ulValue = pxPacketFrame->xHeader.ulValue;

		wifi_crypt_packet(&pxPacketFrame->ucFrameData[0],
		                  &pxPacketEncrypted->ucFrameData[0],
		                  pxPacketEncrypted->xHeader.ucDataSize,
		                  ESP_AES_DECRYPT);
		pxPacketFrame = (const PacketFrame_t*)pxPacketEncrypted;
	}

	switch(pxPacketFrame->xHeader.ucType)
	{
	case PACKET_TYPE_ACK: {
		vResetForcedFrameUpdate();
		vStartNewFrame();
		break;
	}

	case PACKET_TYPE_PING: {
		send_new_packet((const PacketFrame_t*)pxPacketFrame);
		break;
	}

	case PACKET_TYPE_SWITCH_CHANNEL: {
		uint8_t ucNewChannel = pxPacketFrame->ucFrameData[0];
		ESP_ERROR_CHECK(esp_wifi_set_channel(ucNewChannel, WIFI_SECOND_CHAN_NONE));

#if(WIRELESS_USE_RAW_80211_PACKET == 0)
		esp_now_peer_info_t xNewNodeSettings;
		memcpy(&xNewNodeSettings, &xPeerNode, sizeof(esp_now_peer_info_t));

		xNewNodeSettings.channel = ucNewChannel;
		ESP_ERROR_CHECK(esp_now_mod_peer(&xNewNodeSettings));
#endif

		vResetForcedFrameUpdate();
		vEnableForcedFrameUpdate();
		break;
	}

	case PACKET_TYPE_TX_POWER_UPDATE: {
		wifi_set_tx_power((int8_t)pxPacketFrame->ucFrameData[0]);
		break;
	}

	case PACKET_TYPE_ENABLE_LED: {
		vCameraSetLEDState(pxPacketFrame->ucFrameData[0]);
		break;
	}

	default: {
		// TODO: BD-0006 Cover not supported packet type.
		break;
	}
	}
}

#if(WIRELESS_USE_RAW_80211_PACKET == 1)
static void IRAM_ATTR
wifi_raw_packet_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type)
{
	ASYNC_PRINTF(WIFI_RX_PACKET_CB_DBG_PRINTOUT, async_print_type_u32, "wifi_raw_packet_rx_cb %u\n", (uint32_t)type);

	const wifi_promiscuous_pkt_t* px_promiscuous_pkt = (wifi_promiscuous_pkt_t*)buf;
	const wifi_espnow_packet_t* px_espnow_packet = (wifi_espnow_packet_t*)px_promiscuous_pkt->payload;

#if 0
	wifi_espnow_dump_playload((uint8_t*)px_promiscuous_pkt->payload);
#endif

	if((px_espnow_packet->category_code == 0x7f) && (px_espnow_packet->content.element_id == WIFI_VENDOR_IE_ELEMENT_ID))
	{
		// The Type field is set to the value (4) indicating ESP-NOW
		if(px_espnow_packet->content.type == 0x04)
		{
			wifi_espnow_parse_new_data(px_espnow_packet->content.body);
		}
	}
}
#endif // WIRELESS_USE_RAW_80211_PACKET

#if(WIRELESS_USE_RAW_80211_PACKET == 0)
// static void IRAM_ATTR
// wifi_espnow_packet_tx_cb(const uint8_t* mac_addr, esp_now_send_status_t status)
// {
// 	(void)mac_addr;
// 	(void)status;

// 	xSemaphoreGive(xDataTransmitterTxLockHandler);
// }

static void IRAM_ATTR
wifi_espnow_packet_rx_cb(const uint8_t* mac_addr, const uint8_t* data, int data_len)
{
	(void)data_len;

	// wifi_espnow_dump_mac(mac_addr);
	wifi_espnow_parse_new_data(data);
}
#endif

static void
init_wifi_rtos(void)
{
	xFramePacketQueueHandler = xQueueCreateStatic(FRAME_PACKETS_QUEUE_SIZE,
	                                              sizeof(uint32_t),
	                                              (uint8_t*)(&xFramePacketQueueStorage[0]),
	                                              &xFramePacketQueueControlBlock);
	assert(xFramePacketQueueHandler);

	// #if(WIRELESS_USE_RAW_80211_PACKET == 0)
	// 	xDataTransmitterTxLockHandler = xSemaphoreCreateBinaryStatic(&xDataTransmitterTxLockControlBlock);
	// 	assert(xDataTransmitterTxLockHandler);
	// #endif

	xDataTransmitterTaskHandler = xTaskCreateStaticPinnedToCore((TaskFunction_t)(vDataTransmitterTask),
	                                                            assigned_name_for_task_data_tx,
	                                                            STACK_WORDS_SIZE_FOR_TASK_DATA_TX,
	                                                            NULL,
	                                                            PRIORITY_LEVEL_FOR_TASK_DATA_TX,
	                                                            xDataTransmitterStack,
	                                                            &xDataTransmitterTaskControlBlock,
	                                                            (BaseType_t)PINNED_CORE_FOR_TASK_DATA_TX);
	assert(xDataTransmitterTaskHandler);
}

// ----------------------------------------------------------------------
// Accessors functions

void
vWirelessGetOwnMAC(uint8_t* pucMAC)
{
	ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, pucMAC));
}

void
vWirelessSetNodeKeys(pairing_data_t* pxKeysData)
{
	memcpy(&xPeerNode.peer_addr[0], &pxKeysData->ucOtherNodeMac[0], ESP_NOW_ETH_ALEN);
	memcpy(&xPeerNode.lmk[0], &pxKeysData->ucLMK[0], ESP_NOW_KEY_LEN);
}


PacketFrame_t*
get_packet_from_queue(void)
{
	return &xPackets[ulFramePacketOffset];
}

void IRAM_ATTR
set_packet_to_queue(void)
{
	PROFILE_POINT(QUEUE_PACKET_SEND_DBG_PROFILER, profile_point_start);

	xQueueSend(xFramePacketQueueHandler, &ulFramePacketOffset, portMAX_DELAY);
	ulFramePacketOffset = (ulFramePacketOffset + 1) & WIFI_TX_PACKETS_NUM_MASK;

	PROFILE_POINT(QUEUE_PACKET_SEND_DBG_PROFILER, profile_point_end);
}


void IRAM_ATTR
vWirelessSendArray(wifi_packet_type_t xType, uint8_t* pucData, size_t ulDataSize, BaseType_t xUseEncryption)
{
	PROFILE_POINT(NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER, profile_point_start);

	uint32_t ulTotalPackets = 0;

	// Reuse as it have blockId field.
	PacketImageData_t* pxPacket = NULL;
	PacketHeader_t xConfiguredHeader = {.ucType = (uint8_t)xType,
	                                    .ucEncrypted = xUseEncryption,
	                                    .ucFinalBlock = pdFALSE,
	                                    .ucDataSize = PACKET_IMAGE_DATA_MAX_SIZE + 1};

	while(ulDataSize > PACKET_IMAGE_DATA_MAX_SIZE)
	{
		pxPacket = (PacketImageData_t*)get_packet_from_queue();
		pxPacket->xHeader.ulValue = xConfiguredHeader.ulValue;
		pxPacket->usBlockId = ulTotalPackets;

		memcpy(&pxPacket->ucImageData[0], pucData, PACKET_IMAGE_DATA_MAX_SIZE);
		set_packet_to_queue();
		++ulTotalPackets;

		pucData += PACKET_IMAGE_DATA_MAX_SIZE;
		ulDataSize -= PACKET_IMAGE_DATA_MAX_SIZE;
	}

	pxPacket = (PacketImageData_t*)get_packet_from_queue();
	pxPacket->xHeader.ulValue = xConfiguredHeader.ulValue;
	pxPacket->xHeader.ucFinalBlock = pdTRUE;
	pxPacket->xHeader.ucDataSize = ulDataSize + 1;
	pxPacket->usBlockId = ulTotalPackets;

	memcpy(&pxPacket->ucImageData[0], pucData, ulDataSize);
	set_packet_to_queue();
	++ulTotalPackets;

#if TOTAL_PACKETS_SEND_DBG_PRINTOUT
	if(ulTotalPackets)
	{
		ASYNC_PRINTF(1, async_print_type_u32, "Total packets %u\n", ulTotalPackets);
	}
#endif

	PROFILE_POINT(NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER, profile_point_end);
}


// ----------------------------------------------------------------------
// FreeRTOS functions

static void
vDataTransmitterTask(void* pvArg)
{
	(void)pvArg;
	uint32_t ulFramePacketOffset = 0UL;
	PacketFrame_t* pxPacket = NULL;

	task_sync_get_bits(TASK_SYNC_EVENT_BIT_DATA_TX);

#if(WIRELESS_USE_RAW_80211_PACKET == 0)
	// xSemaphoreGive(xDataTransmitterTxLockHandler);
#endif

	ASYNC_PRINTF(ENABLE_TASK_START_EVENT_DBG_PRINTOUT, async_print_type_str, assigned_name_for_task_data_tx, 0);

	for(;;)
	{
		// Wait for data as much as possible, but once anything appear - do not stop!
		while(xQueueReceive(xFramePacketQueueHandler, &ulFramePacketOffset, portMAX_DELAY))
		{
			pxPacket = &xPackets[ulFramePacketOffset];
			send_new_packet((const PacketFrame_t*)pxPacket);
		}
	}
}

// ----------------------------------------------------------------------
// Core functions

void
init_wifi(void)
{
	init_wifi_rtos();

#if(WIRELESS_USE_RAW_80211_PACKET == 1)
	memcpy(&wifi_espnow_raw_packet.magic_packet, ucDataBlob, sizeof(ucDataBlob));
#endif

	// ESP_ERROR_CHECK(esp_netif_init());
	// ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(nvs_flash_init());

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, DEFAULT_WIFI_MODE));
	ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20));

#if(WIRELESS_USE_RAW_80211_PACKET == 1)
	// A bit of trick over ESP-NOW to get RSSI data and connection reliability
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_raw_packet_rx_cb));
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

	wifi_promiscuous_filter_t xFilter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&xFilter));
#endif

	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_ERROR_CHECK(esp_wifi_set_country_code(DEFAULT_WIFI_COUNTRY_CODE, false));
	ESP_ERROR_CHECK(esp_wifi_set_channel(DEFAULT_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
	ESP_ERROR_CHECK(esp_wifi_internal_set_fix_rate(WIFI_IF_STA, true, DEFAULT_WIFI_DATA_RATE));

	wifi_set_tx_power(DEFAULT_WIFI_TX_POWER);
}


void
init_espnow(void)
{
	ESP_ERROR_CHECK(esp_wifi_disconnect());

#if(WIRELESS_USE_RAW_80211_PACKET == 0)
	ESP_ERROR_CHECK(esp_now_init());
	ESP_ERROR_CHECK(esp_wifi_config_espnow_rate(WIFI_IF_STA, DEFAULT_WIFI_DATA_RATE));
	ESP_ERROR_CHECK(esp_now_set_pmk((const uint8_t*)&(xWifiEncryptionGetKeys())->ucPMK[0]));
	ESP_ERROR_CHECK(esp_now_register_recv_cb(wifi_espnow_packet_rx_cb));
	// ESP_ERROR_CHECK(esp_now_register_send_cb(wifi_espnow_packet_tx_cb));
	ESP_ERROR_CHECK(esp_now_add_peer((const esp_now_peer_info_t*)&xPeerNode));
#endif
}


void
init_wireless(void)
{
	init_wifi();
	init_encryption();
	init_espnow();
}