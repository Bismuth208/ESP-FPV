/**
 * @file wireless_scanner.c
 * 
 * Scan all available channels and peek the best one.
 * 
 * Selection depends on:
 *  - How many AP on channel.
 *  - How much noise on channel for every AP.
 *    i.e. If ch1 have 1 AP with RSSi -20dBm,
 *    and ch2 have 2 AP with RSSi -39dbm each.
 *    Second channel will be selected.
 *  - How much noise from neighborhood channel.
 *    i.e. Scanner will try to go far away as much as possible 
 *    from noisy channel.
 * 
 * Channels:
 *  - Japan [1 : 14]
 *  - North America [1 : 12]
 *  - Most of the World [1 : 13]
 */

#include "memory_model/memory_model.h"
#include "wireless_conf.h"
#include "wireless_main.h"

#include <debug_tools_esp.h>
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
#include <esp_now.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration

// Total amount of all available channels
// This value DO NOT depend on region
#define AIR_MAX_CHANNELS_TO_SCAN (14)


#if(CONFIG_WIFI_PROV_SCAN_MAX_ENTRIES <= 24)
#warning "For best Scan results please, set WiFi provisioning Scan entries to at least 24"
#endif

// How much AP on channel to collect info from
#define AIR_SCANNER_MAX_AP_NUM (CONFIG_WIFI_PROV_SCAN_MAX_ENTRIES)

// Yeah, this is not over9000...
// Sensitivity as -90dBm is lowest signal value (according to the docs)
#define AIR_BEST_RSSI_NOSE (90 * AIR_SCANNER_MAX_AP_NUM)

// Magic number what represent how much noise is generated by
// neighborhood channel
#define AIR_NEAR_CHANNEL_NOSE_AFFECTION_COEFF ((float)0.75)


// ----------------------------------------------------------------------
// Accessors functions

void
vScanAirForBestChannel(void)
{
	uint16_t usApNum = 0;
	size_t ucBestChannel = 0;
	int16_t isBestRssi = AIR_BEST_RSSI_NOSE;

#if(CONFIG_WIRELESS_TELL_SCAN_PROGRESS == 1)
	uint16_t usApNumMax = 0xFFFF;
#endif

	channel_stats_t xChannelsStats[AIR_MAX_CHANNELS_TO_SCAN];
	memset(&xChannelsStats[0], 0x00, sizeof(channel_stats_t) * AIR_MAX_CHANNELS_TO_SCAN);

	wifi_country_t xWirelessCountrySettings;
	ESP_ERROR_CHECK(esp_wifi_get_country(&xWirelessCountrySettings));
	assert(xWirelessCountrySettings.nchan > 1);

	uint16_t usDummyRecordsRead = AIR_SCANNER_MAX_AP_NUM;
	wifi_ap_record_t* pxWiFiApRecords = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * AIR_SCANNER_MAX_AP_NUM);

	if(NULL == pxWiFiApRecords)
	{
		return;
	}

	// Look for AP on each channel as long as possible
	wifi_scan_config_t xScanConfig = {.ssid = NULL,
	                                  .bssid = NULL,
	                                  .channel = 0,
	                                  .show_hidden = true,
	                                  .scan_type = WIFI_SCAN_TYPE_ACTIVE,
	                                  .scan_time = {.active = {.min = 1200, .max = 1500}, .passive = 1500}};

	ASYNC_PRINTF(CONFIG_WIRELESS_TELL_SCAN_PROGRESS,
	             async_print_type_u32,
	             "Total channels: %u\n",
	             (uint32_t)xWirelessCountrySettings.nchan);

	// STEP: 1
	// Scan all channels
	for(size_t i = 0; i < xWirelessCountrySettings.nchan; i++)
	{
		xScanConfig.channel = i + 1; // convert to ESP API
		usDummyRecordsRead = AIR_SCANNER_MAX_AP_NUM;

		vMemoryModelSet(MEMORY_MODEL_WIFI_SCAN_CHANNEL, xScanConfig.channel);

		ASYNC_PRINTF(CONFIG_WIRELESS_TELL_SCAN_PROGRESS,
		             async_print_type_u32,
		             "Current channel: %u\n",
		             (uint32_t)xScanConfig.channel);

		// STEP: 1.1
		// Get all data from current channel
		ESP_ERROR_CHECK(esp_wifi_scan_start(&xScanConfig, true));
		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&usApNum));
		// According to the docs this need to be called to free up allocated memory by esp_wifi_scan_start
		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&usDummyRecordsRead, pxWiFiApRecords));

		// STEP: 1.2
		// Collect total noise on this channel from all APs on it
		for(size_t m = 0; m < usDummyRecordsRead; m++)
		{
			// Summarizing noise from all APs is entirely wrong, but i think it will work...
			// And YES, this is NOT how signal physics work!
			xChannelsStats[i].isRssi = (xChannelsStats[i].isRssi - pxWiFiApRecords[m].rssi);
		}

		xChannelsStats[i].usApNum = usApNum;
		xChannelsStats[i].xChannelId = xScanConfig.channel;

#if(CONFIG_WIRELESS_TELL_SCAN_PROGRESS == 1)
		ASYNC_PRINTF(1, async_print_type_u32, "Channel APs: %u\n", (uint32_t)xChannelsStats[i].usApNum);
		ASYNC_PRINTF(1, async_print_type_u32, "Channel Noise: %d\n\n", (uint32_t)xChannelsStats[i].isRssi);
#endif
	}

	// STEP: 2
	// Now parse collected info from all channels
	// Also collect Weight for each channel using formula:
	//  channel_weight = current_channel_total_rssi +
	//									(prev_channel_total_rssi * magic_const) + (next_channel_total_rssi * magic_const)
	// where magic_const is AIR_NEAR_CHANNEL_NOSE_AFFECTION_COEFF
	for(size_t i = 0; i < xWirelessCountrySettings.nchan; i++)
	{
		// Edge case, there are no -1 channel
		if(0 == i)
		{
			xChannelsStats[i].isChannelWeight =
			    xChannelsStats[i].isRssi + (float)xChannelsStats[i + 1].isRssi * AIR_NEAR_CHANNEL_NOSE_AFFECTION_COEFF;
		}

		// Edge case, there are no (xWirelessCountrySettings.nchan + 1) channel
		if((xWirelessCountrySettings.nchan - 1) == i)
		{
			xChannelsStats[i].isChannelWeight =
			    xChannelsStats[i].isRssi + (float)xChannelsStats[i - 1].isRssi * AIR_NEAR_CHANNEL_NOSE_AFFECTION_COEFF;
		}

		if(i < (xWirelessCountrySettings.nchan - 1))
		{
			xChannelsStats[i].isChannelWeight = xChannelsStats[i].isRssi +
			                                    (float)xChannelsStats[i - 1].isRssi * AIR_NEAR_CHANNEL_NOSE_AFFECTION_COEFF +
			                                    (float)xChannelsStats[i + 1].isRssi * AIR_NEAR_CHANNEL_NOSE_AFFECTION_COEFF;
		}

		// STEP: 2.1
		// Meanwhile select most quiet channel based on Weight
		if(xChannelsStats[i].isChannelWeight <= isBestRssi)
		{
#if(CONFIG_WIRELESS_TELL_SCAN_PROGRESS == 1)
			usApNumMax = xChannelsStats[i].usApNum;
#endif
			ucBestChannel = xChannelsStats[i].xChannelId;
			isBestRssi = xChannelsStats[i].isChannelWeight;
		}
	}

#if(CONFIG_WIRELESS_TELL_SCAN_PROGRESS == 1)
	ASYNC_PRINTF(1, async_print_type_u32, "Best channel: %u\n", (uint32_t)ucBestChannel);
	ASYNC_PRINTF(1, async_print_type_u32, "Channel APs: %u\n", (uint32_t)usApNumMax);
	ASYNC_PRINTF(1, async_print_type_u32, "Channel avg. Noise: %d\n", (uint32_t)isBestRssi);
#endif

	// if(DEFAULT_WIFI_CHANNEL != ucBestChannel)
	{
		vMemoryModelSet(MEMORY_MODEL_WIFI_CURRENT_CHANNEL, ucBestChannel);
	}

	free(pxWiFiApRecords);
}