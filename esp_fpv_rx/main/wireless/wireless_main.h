/**
 * @file wireless_main.h
 * 
 * Module type declaration & definitions.
 * Also provide API functions to use for other modules.
 */

#ifndef _WIRELESS_MAIN_H
#define _WIRELESS_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

//
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <esp_mesh_internal.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <stdint.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration

// ----------------------------
// Maximum amount of framebuffers for Rx data from Transmitter
#define IMG_JPG_FRAMEBUFFERS_MAX_NUM (2)
#define IMG_JPG_FRAMEBUFFERS_MASK    (IMG_JPG_FRAMEBUFFERS_MAX_NUM - 1)

// 16k for QVGA is pretty enougth
#define IMG_JPG_FILE_MAX_SIZE (16 * 1024)


// ----------------------------
typedef enum
{
	PACKET_TYPE_ACK = 0,
	PACKET_TYPE_NAK,
	PACKET_TYPE_INITIAL_HEADER_DATA,
	PACKET_TYPE_FRAME_DATA,
	PACKET_TYPE_TELEMETRY,
	PACKET_TYPE_PING,
	PACKET_TYPE_SWITCH_CHANNEL,
	PACKET_TYPE_TX_POWER_UPDATE,
	PACKET_TYPE_ENABLE_LED
} wifi_packet_type_t;

typedef enum
{
	W_MSG_EVENT_FRAME_RECEIVED,
	W_MSG_EVENT_PING,
	W_MSG_EVENT_RTT,
	W_MSG_EVENT_RSSI_UPDATE,
	W_MSG_EVENT_SWITCH_CURRENT_CHANNEL,
	W_MSG_EVENT_UPDATE_TX_POWER_1,
	W_MSG_EVENT_UPDATE_TX_POWER_2,
	W_MSG_EVENT_TOTAL
} wireless_msg_events_t;

// ----------------------------


typedef struct
{
	uint16_t usApNum;
	size_t xChannelId;
	int16_t isRssi;
	int16_t isChannelWeight;
} channel_stats_t;

// ----------------------------
#pragma pack(push, 1)
typedef struct
{
	uint8_t element_id : 8;         // 1 byte
	uint8_t length : 8;             // 1 byte
	uint8_t oui[3];                 // 3 bytes
	uint8_t type : 8;               // 1 byte
	uint8_t version : 8;            // 1 byte
	uint8_t body[0];                // 0~250 bytes
} wifi_vendor_specific_content_t; // 7~255 bytes

typedef struct
{
#if 1
	uint16_t frame_ctrl : 16;    // 2 bytes
	uint16_t duration_id : 16;   // 2 bytes
	uint8_t addr1[6];            // receiver address
	uint8_t addr2[6];            // sender address
	uint8_t addr3[6];            // filtering address
	uint16_t sequence_ctrl : 16; // 2 bytes
#else
	uint8_t some_mac_data[24];
#endif
} wifi_espnow_mac_hdr_t; // 24 bytes

typedef struct
{
	wifi_espnow_mac_hdr_t hdr;
	uint8_t category_code : 8; // 1 byte
	uint8_t oui[3];            // 3 bytes
	uint32_t random : 32;      // 4 bytes
	wifi_vendor_specific_content_t content;
} wifi_espnow_packet_t;
#pragma pack(pop)

// ----------------------------
// Common protocol type definitions
#pragma pack(push, 1)

typedef struct
{
	uint8_t ucOtherNodeMac[6]; // Node MAC in STA mode
	uint8_t ucLMK[16];         // Local Master Key
	uint8_t ucPMK[16];         // Primary Master Key
	uint8_t ucAES[16];         // AES128 key
} pairing_data_t;


typedef struct
{
	union
	{
		uint32_t ulValue;

		struct
		{
			uint8_t ucType; // See @ref ''wifi_packet_type_t''
			union
			{
				uint8_t ucFlags;
				struct
				{
					uint8_t ucEncrypted : 1;  // Received ucFrameData[] is encrypted
					uint8_t ucFinalBlock : 1; // All previously splitted data is now fully transmitted and ready for process
					uint8_t ucUnused : 6;
				};
			};
			uint8_t ucDataSize; // Amount of bytes in ucFrameData[]
			uint8_t ucReserved; // For padding and easy 32bit value copy via ulValue
		};
	};
} PacketHeader_t; // 4 bytes total

// Used to describe how much data is left for useful playload.
// Remember what maximum Frame size should be less or equal to ESP_NOW_MAX_DATA_LEN bytes
#define PACKET_FREE_DATA_SIZE (ESP_NOW_MAX_DATA_LEN - sizeof(PacketHeader_t))

typedef struct
{
	PacketHeader_t xHeader;
	uint8_t ucFrameData[PACKET_FREE_DATA_SIZE];
} PacketFrame_t; // About 4~250 bytes

typedef struct
{
	PacketHeader_t xHeader;
	uint8_t usBlockId;
	uint8_t ucImageData[PACKET_FREE_DATA_SIZE - 1];
} PacketImageData_t; // About 5~250 bytes

#define PACKET_IMAGE_DATA_MAX_SIZE (PACKET_FREE_DATA_SIZE - 1)

typedef struct
{
	PacketHeader_t xHeader;
	uint64_t ullTimestamp;
} PacketPing_t; // About 12 bytes


#pragma pack(pop)

// ----------------------------------------------------------------------
// Variables


// ----------------------------------------------------------------------
// Accessors functions

/**
 * @brief Fill device MAC address which is required for Pairing
 * 
 * @param pucMAC Pointer to the byte array where to store self MAC address
 */ 
void vWirelessGetOwnMAC(uint8_t* pucMAC);

/**
 * @brief Apply loaded Keys from NVS and New one from Pairing
 * 
 * @param pxKeysData Pointer to the Secret Key Storage
 */ 
void vWirelessSetNodeKeys(pairing_data_t* pxKeysData);

/**
 * @brief Encrypts first 16 bytes of input buffer
 * 
 * @param pucDataIn Input buffer with data to encrypt
 * @param pucDataOut Output buffer where encrypted data will be placed
 * @param xInputSize Size of input buffer and amount of data in it
 * @param ucMode pass 1 for decrypt, pass 0 to encrypt. Check @ref ''AES_DECRYPT''
 * 
 * @note 	Encrypt at least first 16 bytes of data
 * 				Even if ES_NOW support encryption out of box,
 * 				there are few problems:
 * 					 - If ES_NOW encryption enabled, it's not possible to check 802.11 packets
 *   					 by vendor-specific data. That mean: NO VALID RSSi values!
 *				   - When ''WIRELESS_USE_RAW_80211_PACKET'' is enabled there is no encryption at all
 *				     for the playload!
 */
void wifi_crypt_packet(const uint8_t* pucDataIn, uint8_t* pucDataOut, size_t xInputSize, uint8_t ucMode);

/**
 * @brief
 * 
 * @retval
 */
const pairing_data_t* xWifiEncryptionGetKeys(void);


/**
 * @brief Get pointer to data and swap buffers: Receiving and Decoding
 * 
 * @return pointer to current buffer with image data
 */
uint8_t* pucWirelessTakeCurrentRxBuffer(void);

/**
 * @brief
 */
BaseType_t xWirelessSendEvent(wireless_msg_events_t xEvent);

/**
 * @brief Do the channel scan for the best one.
 */
void vScanAirForBestChannel(void);

// ----------------------------------------------------------------------
// Core functions

/**
 * @brief
 */
void init_wireless(void);

/**
 * @brief Do complete initialisation of WiFi module
 * 
 */
void init_wifi(void);

/**
 * @brief Do initialisation of ESP_NOW module
 * 
 * @note ESP_NOW is not used when @ref ''WIRELESS_USE_RAW_80211_PACKET'' is enabled
 */
void init_espnow(void);

/**
 * @brief Check Pairing Pin if it's HIGH load Encryption keys from NVS,
 *        otherwise start Pairing sequence.
 * 
 * @attention This is really important to call AFTER @ref ''init_wifi()'' !
 */
void init_encryption(void);


#ifdef __cplusplus
}
#endif

#endif /* _WIRELESS_MAIN_H */