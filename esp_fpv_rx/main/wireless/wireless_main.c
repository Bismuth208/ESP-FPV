/**
 * @file wireless_main.c
 * 
 * Receive image data from ESP-CAM over WiFi.
 */

#include "wireless_main.h"

#include "button_poller.h"
#include "data_common.h"
#include "debug_tools_conf.h"
#include "image_decoder.h"
#include "memory_model/memory_model.h"
#include "pins_definitions.h"
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
#include <aes/esp_aes.h>
#include <esp_private/periph_ctrl.h>
#include <hal/aes_hal.h>
#include <hal/aes_ll.h>
#include <soc/dport_access.h>
#include <soc/hwcrypto_periph.h>
#include <soc/hwcrypto_reg.h>
#include <soc/periph_defs.h>
//
#include <esp_attr.h>
#include <esp_mesh_internal.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>


// ----------------------------------------------------------------------
// Definitions, type & enum declaration

// Amount of system ticks to wait event queue
#define WIRELESS_EVENT_MAX_WAIT_TIMEOUT (5)


#if WIRELESS_USE_RAW_80211_PACKET
typedef struct
{
	wifi_espnow_packet_t magic_packet;
	uint8_t ucData[250];
} wifi_espnow_raw_packet_t;
#endif // WIRELESS_USE_RAW_80211_PACKET


// ----------------------------------------------------------------------
// FreeRTOS Variables

#define STACK_WORDS_SIZE_FOR_TASK_DATA_TX (3072)
#define PRIORITY_LEVEL_FOR_TASK_DATA_TX   (2)
#define PINNED_CORE_FOR_TASK_DATA_TX      (1)
const char* assigned_name_for_task_data_tx = "data_tx";
TaskHandle_t xDataTransmitterTaskHandler = NULL;
StaticTask_t xDataTransmitterTaskControlBlock;
StackType_t xDataTransmitterStack[STACK_WORDS_SIZE_FOR_TASK_DATA_TX];

#define EVENT_QUEUE_SIZE (32)
QueueHandle_t xEventQueueHandler = NULL;
StaticQueue_t xEventQueueControlBlock;
wireless_msg_events_t xEventQueueStorage[EVENT_QUEUE_SIZE];

SemaphoreHandle_t xDataTransmitterTxLockHandler = NULL;
StaticSemaphore_t xDataTransmitterTxLockControlBlock;

// in ms or each 1s
#define NET_STATS_TIMER_TIMEOUT (1000)
TimerHandle_t xNetStatsTimer = NULL;
StaticTimer_t xNetStatsTimerControlBlock;

// ----------------------------------------------------------------------
// Variables

// ----------------------------
// clang-format off
esp_now_peer_info_t xPeerNode = {
  {
    // tx node MAC in STA mode
		0
  },
  {
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
	0x00, 0x00, // .duration_id  (from actual frame: 0x3a,0x01)
	0x7c,0xdf,0xa1,0xe7,0xe0,0x00, // .addr2[]
	0xcc,0x50,0xe3,0x96,0x57,0xdc, // .addr1[]
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

uint8_t ucRxImageBuf[IMG_JPG_FRAMEBUFFERS_MAX_NUM][IMG_JPG_FILE_MAX_SIZE] = {0};

uint32_t ulImgCurBufNum = 0UL;
uint8_t* pucImgCurRxBufPtr = &ucRxImageBuf[0][0];

uint32_t ulReceivedData = 0;
uint32_t ulTotalReceivedData = 0;

int8_t icLinkRSSI = -98;
// TelemetryPacket_t xCamTelemetryPkt;


uint8_t ucEncryptedData[2][256];

uint16_t usDataOffsetExtra = 0;
BaseType_t xFirstFrame = pdTRUE;

PacketFrame_t xPacket;


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
 * @param data_len amount of bytes in ''data'' array
 */
static void wifi_espnow_parse_new_data(const uint8_t* data, int data_len);

/**
 * @brief Callback function from WiFi driver in promiscuous mode.
 * 
 * @param buf Raw blob of received data from WiFi interface
 * @param type Description of the data located in @ref ''buf''
 */
static void wifi_raw_packet_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type);

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
static void wifi_espnow_packet_tx_cb(const uint8_t* mac_addr, esp_now_send_status_t status);
#endif // !WIRELESS_USE_RAW_80211_PACKET

/**
 * @brief Callback function which is called when any value has been updated
 *        in @ref memory_model
 * 
 * @param xDataId Type of variable what was updated
 */
static void vWirelessUpdateCallback(memory_model_types_t xDataId);

/**
 * @brief Initialize memory_model for WiFi.
 */
static void init_wifi_memory_model(void);

/**
 * @brief Creates FreeRTOS objects what need to maintain WiFi.
 */
static void init_wifi_rtos(void);

/**
 * @brief Signals to check ping, RTT and other stuff
 */
static void vNetStatsTimer(TimerHandle_t xTimer);

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
wifi_espnow_dump_playload(const char* text, uint8_t* pucPayloadBuff, size_t size, uint32_t id)
{
	static char cBuff[2][1024];
	size_t offset = 0;

	for(size_t i = 0; i < size; i++)
	{
		offset += sprintf(&cBuff[id][offset], "%02X ", pucPayloadBuff[i]);
	}

	ASYNC_PRINTF(1, async_print_type_str, text, 0);
	ASYNC_PRINTF(1, async_print_type_str, &cBuff[id][0], 0);
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
	// TODO: BD-0005 add random stuff for magic_packet.random
	// wifi_espnow_raw_packet.magic_packet.random = (uint32_t)
	wifi_espnow_raw_packet.magic_packet.content.length = ulTxDataLen;
	memcpy(wifi_espnow_raw_packet.magic_packet.content.body, pxPacketFrameToSend, ulTxDataLen);
	esp_err_t xRes =
	    esp_wifi_80211_tx(WIFI_IF_STA, &wifi_espnow_raw_packet, sizeof(wifi_espnow_packet_t) + ulTxDataLen, true);
#else
	xSemaphoreTake(xDataTransmitterTxLockHandler, portMAX_DELAY);
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
wifi_espnow_parse_new_data(const uint8_t* data, int data_len)
{
	const PacketFrame_t* pxPacketFrame = (const PacketFrame_t*)data;

	PROFILE_POINT(ESP_NOW_RX_DATA_DBG_PROFILER, profile_point_start);

	if(pxPacketFrame->xHeader.ucEncrypted)
	{
		PacketFrame_t* pxPacketEncrypted = (PacketFrame_t*)&ucEncryptedData[1][0];
		pxPacketEncrypted->xHeader.ulValue = pxPacketFrame->xHeader.ulValue;

		wifi_crypt_packet(&pxPacketFrame->ucFrameData[0],
		                  &pxPacketEncrypted->ucFrameData[0],
		                  pxPacketEncrypted->xHeader.ucDataSize,
		                  ESP_AES_DECRYPT);
		pxPacketFrame = (const PacketFrame_t*)pxPacketEncrypted;
	}

	// Count whole amount of received data, not only playload!
	if(data_len)
	{
		ulTotalReceivedData += (uint32_t)data_len;
	}

	switch(pxPacketFrame->xHeader.ucType)
	{
	case PACKET_TYPE_INITIAL_HEADER_DATA: {
		const PacketImageData_t* pxPacketImageData = (const PacketImageData_t*)pxPacketFrame;
		uint16_t usDataOffset = pxPacketImageData->usBlockId * PACKET_IMAGE_DATA_MAX_SIZE;

		memcpy(&pucImgCurRxBufPtr[usDataOffset],
		       &pxPacketImageData->ucImageData[0],
		       pxPacketImageData->xHeader.ucDataSize - 1);

		usDataOffsetExtra += (pxPacketImageData->xHeader.ucDataSize - 1);

		if(pxPacketImageData->xHeader.ucFinalBlock)
		{
			// Apply constant table data to all framebuffers
			for(size_t i = 0; i < IMG_JPG_FRAMEBUFFERS_MAX_NUM; i++)
			{
				if(i != ulImgCurBufNum)
				{
					memcpy(&ucRxImageBuf[i][0], &pucImgCurRxBufPtr[0], usDataOffsetExtra);
				}
			}
		}
		break;
	}

	case PACKET_TYPE_FRAME_DATA: {
		const PacketImageData_t* pxPacketImageData = (const PacketImageData_t*)pxPacketFrame;
		uint16_t usDataOffset = pxPacketImageData->usBlockId * PACKET_IMAGE_DATA_MAX_SIZE + usDataOffsetExtra;

		memcpy(&pucImgCurRxBufPtr[usDataOffset],
		       &pxPacketImageData->ucImageData[0],
		       pxPacketImageData->xHeader.ucDataSize - 1);

		if(pxPacketImageData->xHeader.ucFinalBlock)
		{
			vImageProcessorStartDecode();
		}

		break;
	}

		// case PACKET_TYPE_TELEMETRY: {
		// 	memcpy(&xCamTelemetryPkt, pxPacketFrame, sizeof(TelemetryPacket_t));
		// 	break;
		// }

	case PACKET_TYPE_PING: {
		uint32_t ulRoundTripTime = ((esp_timer_get_time() - ((PacketPing_t*)pxPacketFrame)->ullTimestamp) / 1000);
		vMemoryModelSet(MEMORY_MODEL_WIFI_RTT_VALUE, ulRoundTripTime);
		break;
	}

	default: {
		// TODO: BD-0006 Cover not supported packet type.
		break;
	}
	}

	PROFILE_POINT(ESP_NOW_RX_DATA_DBG_PROFILER, profile_point_end);
}


static void IRAM_ATTR
wifi_raw_packet_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type)
{
	ASYNC_PRINTF(WIFI_RX_PACKET_CB_DBG_PRINTOUT, async_print_type_u32, "wifi_raw_packet_rx_cb %u\n", (uint32_t)type);

	const wifi_promiscuous_pkt_t* px_promiscuous_pkt = (wifi_promiscuous_pkt_t*)buf;
	const wifi_espnow_packet_t* px_espnow_packet = (wifi_espnow_packet_t*)px_promiscuous_pkt->payload;

#if 0
	wifi_espnow_dump_playload("px_promiscuous_pkt:\n", (uint8_t*)px_promiscuous_pkt->payload, 32, 0);
#endif

	// The Category Code field is set to the value(127) indicating the vendor-specific category.
	// The Element ID field is set to the value (221), indicating the vendor-specific element.
	if((px_espnow_packet->category_code == 0x7f) && (px_espnow_packet->content.element_id == WIFI_VENDOR_IE_ELEMENT_ID))
	{
		// The Type field is set to the value (4) indicating ESP-NOW
		if(px_espnow_packet->content.type == 0x04)
		{
			if(px_promiscuous_pkt->rx_ctrl.rssi != icLinkRSSI)
			{
				icLinkRSSI = px_promiscuous_pkt->rx_ctrl.rssi;
				xWirelessSendEvent(W_MSG_EVENT_RSSI_UPDATE);
			}

#if(WIRELESS_USE_RAW_80211_PACKET == 1)
			wifi_espnow_parse_new_data(px_espnow_packet->content.body, px_espnow_packet->content.length);
#endif

#if 0
			wifi_espnow_dump_playload("->content:\n", (uint8_t*)&px_espnow_packet->content, 8, 1);
			// wifi_espnow_dump_playload("->content.body:\n", (uint8_t*)px_espnow_packet->content.body, 32, 1);
#endif
		}
	}
}


#if(WIRELESS_USE_RAW_80211_PACKET == 0)
static void IRAM_ATTR
wifi_espnow_packet_tx_cb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
	(void)mac_addr;
	(void)status;

	xSemaphoreGive(xDataTransmitterTxLockHandler);
}

static void IRAM_ATTR
wifi_espnow_packet_rx_cb(const uint8_t* mac_addr, const uint8_t* data, int data_len)
{
	(void)mac_addr;

	// wifi_espnow_dump_mac(mac_addr);
	wifi_espnow_parse_new_data(data, data_len);
}
#endif

static void IRAM_ATTR
vWirelessUpdateCallback(memory_model_types_t xDataId)
{
	switch(xDataId)
	{
	case MEMORY_MODEL_WIFI_CURRENT_CHANNEL: {
		xWirelessSendEvent(W_MSG_EVENT_SWITCH_CURRENT_CHANNEL);
		break;
	}

	case MEMORY_MODEL_WIFI_TX_POWER_1: {
		xWirelessSendEvent(W_MSG_EVENT_UPDATE_TX_POWER_1);
		break;
	}

	case MEMORY_MODEL_WIFI_TX_POWER_2: {
		xWirelessSendEvent(W_MSG_EVENT_UPDATE_TX_POWER_2);
		break;
	}

	default:
		break;
	}
}


static void
init_wifi_memory_model(void)
{
	assert(xMemoryModelRegisterItem(MEMORY_MODEL_WIFI_SCAN_CHANNEL));
	assert(xMemoryModelRegisterItem(MEMORY_MODEL_WIFI_CURRENT_CHANNEL));
	assert(xMemoryModelRegisterItem(MEMORY_MODEL_WIFI_TX_POWER_1));
	assert(xMemoryModelRegisterItem(MEMORY_MODEL_WIFI_TX_POWER_2));
	assert(xMemoryModelRegisterItem(MEMORY_MODEL_WIFI_RTT_VALUE));
	assert(xMemoryModelRegisterItem(MEMORY_MODEL_DATA_RX_RATE));
	assert(xMemoryModelRegisterItem(MEMORY_MODEL_WIFI_RX_RSSI));

	vMemoryModelSet(MEMORY_MODEL_WIFI_TX_POWER_1, DEFAULT_WIFI_TX_POWER_1);
	vMemoryModelSet(MEMORY_MODEL_WIFI_CURRENT_CHANNEL, DEFAULT_WIFI_CHANNEL);
	vMemoryModelSet(MEMORY_MODEL_WIFI_RTT_VALUE, 0);
	vMemoryModelSet(MEMORY_MODEL_DATA_RX_RATE, 1);
	vMemoryModelSet(MEMORY_MODEL_WIFI_RX_RSSI, -98);

	assert(xMemoryModelRegisterCallback((memory_model_callback_t)vWirelessUpdateCallback));
}


static void
init_wifi_rtos(void)
{
	xNetStatsTimer = xTimerCreateStatic("xNetStatsTimer",
	                                    pdMS_TO_TICKS(NET_STATS_TIMER_TIMEOUT),
	                                    pdTRUE,
	                                    NULL,
	                                    (TimerCallbackFunction_t)(vNetStatsTimer),
	                                    &xNetStatsTimerControlBlock);
	assert(xNetStatsTimer);

	xEventQueueHandler = xQueueCreateStatic(
	    EVENT_QUEUE_SIZE, sizeof(wireless_msg_events_t), (uint8_t*)(&xEventQueueStorage[0]), &xEventQueueControlBlock);
	assert(xEventQueueHandler);

#if(WIRELESS_USE_RAW_80211_PACKET == 0)
	xDataTransmitterTxLockHandler = xSemaphoreCreateBinaryStatic(&xDataTransmitterTxLockControlBlock);
	assert(xDataTransmitterTxLockHandler);
#endif

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


uint8_t* IRAM_ATTR
pucWirelessTakeCurrentRxBuffer(void)
{
	uint32_t ulImgPrevBufNum = ulImgCurBufNum;

	ulImgCurBufNum = (ulImgCurBufNum + 1) & IMG_JPG_FRAMEBUFFERS_MASK;
	pucImgCurRxBufPtr = &ucRxImageBuf[ulImgCurBufNum][0];

	return &ucRxImageBuf[ulImgPrevBufNum][0];
}


BaseType_t
xWirelessSendEvent(wireless_msg_events_t xEvent)
{
	return xQueueSend(xEventQueueHandler, &xEvent, WIRELESS_EVENT_MAX_WAIT_TIMEOUT);
}

// ----------------------------------------------------------------------
// FreeRTOS functions

static void
vNetStatsTimer(TimerHandle_t xTimer)
{
	(void)xTimer;
	xWirelessSendEvent(W_MSG_EVENT_PING);
	xWirelessSendEvent(W_MSG_EVENT_RTT);
}

static void
vDataTransmitterTask(void* pvArg)
{
	(void)pvArg;
	wireless_msg_events_t xEvent = W_MSG_EVENT_TOTAL;

	task_sync_get_bits(TASK_SYNC_EVENT_BIT_DATA_TX);

#if(WIRELESS_USE_RAW_80211_PACKET == 0)
	xSemaphoreGive(xDataTransmitterTxLockHandler);
#endif

	ASYNC_PRINTF(ENABLE_TASK_START_EVENT_DBG_PRINTOUT, async_print_type_str, assigned_name_for_task_data_tx, 0);

	// Force channels scan if user requested so.
	if(xReadButton(BUTTON_1) == BUTTON_STATE_PRESSED)
	{
		vScanAirForBestChannel();
	}
	else
	{
		vMemoryModelSet(MEMORY_MODEL_WIFI_CURRENT_CHANNEL, DEFAULT_WIFI_CHANNEL);
	}

	// Tell the Transmitter to update it's Tx power
	vMemoryModelSet(MEMORY_MODEL_WIFI_TX_POWER_2, DEFAULT_WIFI_TX_POWER_2);

	xTimerStart(xNetStatsTimer, 0UL);

	for(;;)
	{
		while(xQueueReceive(xEventQueueHandler, &xEvent, portMAX_DELAY))
		{
			switch(xEvent)
			{
			case W_MSG_EVENT_FRAME_RECEIVED: {
				PacketHeader_t* pxPacket = (PacketHeader_t*)&xPacket;
				pxPacket->ulValue = 0;
				pxPacket->ucType = PACKET_TYPE_ACK;
				send_new_packet((const PacketFrame_t*)pxPacket);
				break;
			}

			case W_MSG_EVENT_PING: {
				PacketPing_t* pxPacket = (PacketPing_t*)&xPacket;
				pxPacket->xHeader.ulValue = 0;
				pxPacket->xHeader.ucType = PACKET_TYPE_PING;
				pxPacket->xHeader.ucDataSize = sizeof(uint64_t);
				pxPacket->ullTimestamp = esp_timer_get_time();
				send_new_packet((const PacketFrame_t*)pxPacket);
				break;
			}

			case W_MSG_EVENT_RTT: {
				uint32_t ulDataDiff = ulTotalReceivedData - ulReceivedData;
				ulReceivedData = ulTotalReceivedData;
				vMemoryModelSet(MEMORY_MODEL_DATA_RX_RATE, ulDataDiff);
				break;
			}

			case W_MSG_EVENT_RSSI_UPDATE: {
				vMemoryModelSet(MEMORY_MODEL_WIFI_RX_RSSI, icLinkRSSI);
				break;
			}

			case W_MSG_EVENT_SWITCH_CURRENT_CHANNEL: {
				PacketFrame_t* pxPacket = (PacketFrame_t*)&xPacket;
				pxPacket->xHeader.ulValue = 0;
				pxPacket->xHeader.ucType = PACKET_TYPE_SWITCH_CHANNEL;
				// pxPacket->xHeader.ucEncrypted = (uint8_t)pdTRUE;
				pxPacket->xHeader.ucDataSize = 1;

				uint8_t ucCurrentChannel = (uint8_t)ulMemoryModelGet(MEMORY_MODEL_WIFI_CURRENT_CHANNEL);
				pxPacket->ucFrameData[0] = ucCurrentChannel;

				esp_err_t xRes = send_new_packet((const PacketFrame_t*)pxPacket);

				if(ESP_OK == xRes)
				{
					// Wait msg to be transferred over wifi for at least 50ms.
					// And wait for the Transmitter to switch the WiFi channel.
					vTaskDelay(pdMS_TO_TICKS(50));
					ESP_ERROR_CHECK(esp_wifi_set_channel(ucCurrentChannel, WIFI_SECOND_CHAN_NONE));

#if(WIRELESS_USE_RAW_80211_PACKET == 0)
					esp_now_peer_info_t xNewNodeSettings;
					memcpy(&xNewNodeSettings, &xPeerNode, sizeof(esp_now_peer_info_t));

					xNewNodeSettings.channel = ucCurrentChannel;
					ESP_ERROR_CHECK(esp_now_mod_peer(&xNewNodeSettings));
#endif
				}
				break;
			}

			case W_MSG_EVENT_UPDATE_TX_POWER_1: {
				wifi_set_tx_power((int8_t)ulMemoryModelGet(MEMORY_MODEL_WIFI_TX_POWER_1));
				break;
			}

			case W_MSG_EVENT_UPDATE_TX_POWER_2: {
				PacketFrame_t* pxPacket = (PacketFrame_t*)&xPacket;
				pxPacket->xHeader.ulValue = 0;
				pxPacket->xHeader.ucType = PACKET_TYPE_TX_POWER_UPDATE;
				pxPacket->xHeader.ucDataSize = 1;
				pxPacket->ucFrameData[0] = (uint8_t)ulMemoryModelGet(MEMORY_MODEL_WIFI_TX_POWER_2);
				send_new_packet((const PacketFrame_t*)pxPacket);
				break;
			}

			default:
				break;
			}
		}

		// vTaskDelay(pdMS_TO_TICKS(1));
	}

	vTaskDelete(NULL);
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
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(nvs_flash_init());

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // WIFI_MODE_NULL
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, DEFAULT_WIFI_MODE));
	ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20));

	// A bit of trick over ESP-NOW to get RSSI data and connection reliability
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_raw_packet_rx_cb));
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

	wifi_promiscuous_filter_t xFilter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&xFilter));

	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_ERROR_CHECK(esp_wifi_set_country_code(DEFAULT_WIFI_COUNTRY_CODE, false));
	ESP_ERROR_CHECK(esp_wifi_set_channel(DEFAULT_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
	ESP_ERROR_CHECK(esp_wifi_internal_set_fix_rate(WIFI_IF_STA, true, DEFAULT_WIFI_DATA_RATE));

	wifi_set_tx_power(DEFAULT_WIFI_TX_POWER_1);

	// Now it's time to set up memory model for WiFi
	init_wifi_memory_model();
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
	ESP_ERROR_CHECK(esp_now_register_send_cb(wifi_espnow_packet_tx_cb));
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