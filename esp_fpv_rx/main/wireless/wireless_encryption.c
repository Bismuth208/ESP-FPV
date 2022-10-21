#include "data_common.h"
#include "debug_tools_conf.h"
#include "memory_model/memory_model.h"
#include "pins_definitions.h"
#include "wireless_conf.h"
#include "wireless_main.h"

//
#include <sdkconfig.h>
//
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_attr.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <nvs.h>
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

/// Used UART for the communication
#define UART_SYNC_NUM (UART_NUM_2)

/// The slowest and best one speed for UART
#define UART_SYNC_BAUD_SPEED (9600)


#if(CONFIG_IDF_TARGET_ESP32)
// To be consistant with ESP32-S3
#define AES_TEXT_IN_BASE  AES_TEXT_BASE
#define AES_TEXT_OUT_BASE AES_TEXT_BASE
#define AES_TRIGGER_REG   AES_START_REG
#define AES_STATE_REG     AES_IDLE_REG
#endif

/// Maximum amount of bytes to encrypt with single block
/// This is hardware limitation.
#define AES_ENCRYPTION_MAX_BYTES (16)

/// Where to store the Keys & MAC
#define STORAGE_NAMESPACE "storage"

/// Name of list pair in NVS
#define ENCRYPTION_KEYS_NVS_NAME "sync_keys"

// ----------------------------------------------------------------------
// Variables

const uart_config_t uart_config = {.baud_rate = UART_SYNC_BAUD_SPEED,
                                   .data_bits = UART_DATA_8_BITS,
                                   .parity = UART_PARITY_EVEN,
                                   .stop_bits = UART_STOP_BITS_2,
                                   .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                   .source_clk = UART_SCLK_DEFAULT};


static uint8_t ucCryptLastMode = 0xFF;

esp_aes_context magic_key_storage = {0};
pairing_data_t xSecretSync = {0};


// ----------------------------------------------------------------------
// Static functions declaration

/**
 * @brief Low level encryption optimized for ESP32 only!
 * 
 * @param pulDataIn pointer to 16 byte input buffer
 * @param pulDataOut pointer to 16 byte output buffer
 */
static inline void wifi_aes_crypt_ll(uint32_t* pulDataIn, uint32_t* pulDataOut);


/**
 * @brief Sync Transmitter & Receiver with a bunch of Keys and exchange MAC addresses.
 * 
 * @note Everything is done over UART and single pin.
 */
void wifi_enryption_pair_keys(void);

// ----------------------------------------------------------------------
// Static functions

static inline void IRAM_ATTR
wifi_aes_crypt_ll(uint32_t* pulDataIn, uint32_t* pulDataOut)
{
#if WIFI_AES_ENCRYPT_USE_REGISTERS
	DPORT_REG_WRITE(AES_TEXT_IN_BASE + 0, pulDataIn[0]);
	DPORT_REG_WRITE(AES_TEXT_IN_BASE + 4, pulDataIn[1]);
	DPORT_REG_WRITE(AES_TEXT_IN_BASE + 8, pulDataIn[2]);
	DPORT_REG_WRITE(AES_TEXT_IN_BASE + 12, pulDataIn[3]);

	DPORT_REG_WRITE(AES_TRIGGER_REG, 1);

	do
	{
	} while(DPORT_REG_READ(AES_STATE_REG) != ESP_AES_STATE_IDLE);

	pulDataOut[0] = DPORT_REG_READ(AES_TEXT_OUT_BASE + 0);
	pulDataOut[1] = DPORT_REG_READ(AES_TEXT_OUT_BASE + 4);
	pulDataOut[2] = DPORT_REG_READ(AES_TEXT_OUT_BASE + 8);
	pulDataOut[3] = DPORT_REG_READ(AES_TEXT_OUT_BASE + 12);
#else
	// aes_hal_transform_block(pulDataIn, pulDataOut);
	aes_ll_write_block((void*)pulDataIn);
	aes_ll_start_transform();
	aes_hal_wait_idle();
	aes_ll_read_block((void*)pulDataOut);
#endif
}

void
wifi_enryption_pair_keys(void)
{
	// Setup UART buffered IO with event queue
	const int uart_buffer_size = (1024 * 2);
	QueueHandle_t uart_queue;

	pairing_data_t xSync;
	int length = 0;

	// -----------
	// Set UART TX
	ESP_ERROR_CHECK(uart_param_config(UART_SYNC_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_SYNC_NUM, UART_SYNC_RX_TX_PIN, -1, -1, -1));
	ESP_ERROR_CHECK(uart_driver_install(UART_SYNC_NUM, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
	ESP_ERROR_CHECK(uart_set_mode(UART_SYNC_NUM, UART_MODE_UART));

	// Generate random numbers for at least 100 times
	uint32_t ulEntropyEnrols = 100 + (esp_random() & 0xff);

	for(size_t i = 0; i < ulEntropyEnrols; i++)
	{
		esp_fill_random((void*)&xSync, sizeof(pairing_data_t));
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	// Fill self MAC. It will be used by the Transmitter
	vWirelessGetOwnMAC(&xSync.ucOtherNodeMac[0]);

	// Send data to the Transmitter
	uart_write_bytes(UART_SYNC_NUM, (const char*)&xSync, sizeof(pairing_data_t));
	ESP_ERROR_CHECK(uart_wait_tx_done(UART_SYNC_NUM, pdMS_TO_TICKS(200)));

	// -----------
	// Set UART RX
	ESP_ERROR_CHECK(uart_driver_delete(UART_SYNC_NUM));
	ESP_ERROR_CHECK(gpio_reset_pin(UART_SYNC_RX_TX_PIN));

	ESP_ERROR_CHECK(uart_param_config(UART_SYNC_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_SYNC_NUM, -1, UART_SYNC_RX_TX_PIN, -1, -1));
	ESP_ERROR_CHECK(uart_driver_install(UART_SYNC_NUM, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
	ESP_ERROR_CHECK(uart_set_mode(UART_SYNC_NUM, UART_MODE_UART));

	do
	{
		ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_SYNC_NUM, (size_t*)&length));
	} while(length < (ESP_NOW_ETH_ALEN - 1));

	uart_read_bytes(UART_SYNC_NUM, &xSync.ucOtherNodeMac[0], ESP_NOW_ETH_ALEN, pdMS_TO_TICKS(500));

	ESP_ERROR_CHECK(uart_flush(UART_SYNC_NUM));
	ESP_ERROR_CHECK(uart_driver_delete(UART_SYNC_NUM));

	// -----------
	// Apply Secret Keys and random data with other node MAC
	memcpy(&xSecretSync, &xSync, sizeof(pairing_data_t));

	// Wait Transmitter to setup everything with new keys
	vTaskDelay(pdMS_TO_TICKS(2000));
}


void
vWirelessUpdateNvsSecretKeys(BaseType_t xForcedSave)
{
	nvs_handle_t nvs_keys_handle;
	size_t required_size = sizeof(pairing_data_t);

	ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_keys_handle));
	ESP_ERROR_CHECK(nvs_get_blob(nvs_keys_handle, ENCRYPTION_KEYS_NVS_NAME, NULL, &required_size));

	// Keys have been found?
	if(required_size > 0)
	{
		ESP_ERROR_CHECK(nvs_get_blob(nvs_keys_handle, ENCRYPTION_KEYS_NVS_NAME, &xSecretSync, &required_size));
	}

	if(xForcedSave || (required_size != sizeof(pairing_data_t)))
	{
		ESP_ERROR_CHECK(nvs_set_blob(nvs_keys_handle, ENCRYPTION_KEYS_NVS_NAME, &xSecretSync, required_size));
	}

	ESP_ERROR_CHECK(nvs_commit(nvs_keys_handle));
	nvs_close(nvs_keys_handle);
}


// ----------------------------------------------------------------------
// Accessors functions


void IRAM_ATTR
wifi_crypt_packet(const uint8_t* pucDataIn, uint8_t* pucDataOut, size_t xInputSize, uint8_t ucMode)
{
	// TODO: BD-0003 add lock for multithread access for wifi_crypt_packet
	// TODO: BD-0004 add whole pucDataIn encryption for wifi_crypt_packet

#ifdef AES_ENCRYPTION_TIME_DBG_PROFILER
	profile_point(profile_point_start, AES_ENCRYPTION_TIME_DBG_PROFILER_POINT_ID);
#endif

	if(ucCryptLastMode != ucMode)
	{
		ucCryptLastMode = ucMode;
		aes_hal_setkey((&magic_key_storage)->key, (&magic_key_storage)->key_bytes, ucMode);
	}

	// At encryption less memcpy() is used and we use 1-2us less time, but it's a danger game!
	// Thanks to aligned data with 256 bytes buffers!
	if(ucMode == ESP_AES_ENCRYPT)
	{
		wifi_aes_crypt_ll((uint32_t*)&pucDataIn[0], (uint32_t*)&pucDataOut[0]);
	}
	else
	{
		memcpy(&pucDataOut[AES_ENCRYPTION_MAX_BYTES],
		       &pucDataIn[0],
		       (xInputSize > AES_ENCRYPTION_MAX_BYTES) ? AES_ENCRYPTION_MAX_BYTES : xInputSize);
		wifi_aes_crypt_ll((uint32_t*)&pucDataOut[AES_ENCRYPTION_MAX_BYTES], (uint32_t*)&pucDataOut[0]);
	}

	if(xInputSize > AES_ENCRYPTION_MAX_BYTES)
	{
		memcpy(&pucDataOut[AES_ENCRYPTION_MAX_BYTES],
		       &pucDataIn[AES_ENCRYPTION_MAX_BYTES],
		       xInputSize - AES_ENCRYPTION_MAX_BYTES);
	}

#ifdef AES_ENCRYPTION_TIME_DBG_PROFILER
	profile_point(profile_point_end, AES_ENCRYPTION_TIME_DBG_PROFILER_POINT_ID);
#endif
}


const pairing_data_t*
xWifiEncryptionGetKeys(void)
{
	return &xSecretSync;
}

// ----------------------------------------------------------------------
// Core functions

void
init_encryption(void)
{
	gpio_set_direction(UART_SYNC_RX_TX_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(UART_SYNC_RX_TX_PIN, GPIO_PULLUP_ONLY);

	// If not tied to GND when Sync/Pairing is requested
	if(gpio_get_level(UART_SYNC_RX_TX_PIN) == 1)
	{
		// Reset pin, since it will be re-used by UART
		ESP_ERROR_CHECK(gpio_reset_pin(UART_SYNC_RX_TX_PIN));

		wifi_enryption_pair_keys();
		vWirelessUpdateNvsSecretKeys(pdTRUE);
	}
	else
	{
		vWirelessUpdateNvsSecretKeys(pdFALSE);
	}

	vWirelessSetNodeKeys(&xSecretSync);

	// It will be not used anymore
	ESP_ERROR_CHECK(gpio_reset_pin(UART_SYNC_RX_TX_PIN));

	// Enable AES hardware encryption
	esp_aes_init(&magic_key_storage);
	esp_aes_setkey(&magic_key_storage, &xSecretSync.ucAES[0], sizeof(xSecretSync.ucAES) * 8);
	periph_module_enable(PERIPH_AES_MODULE);
}