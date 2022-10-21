/**
 * @file wireless_stuff_conf.h
 * 
 * Contains definitions for WiFi
 */ 

#ifndef _WIRELESS_STUFF_CONF_H
#define _WIRELESS_STUFF_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

// On average sending data is faster by 30-40us in compare with ESP-NOW
#define WIRELESS_USE_RAW_80211_PACKET (0)

// Use MCU optimized AES calls for 16 bytes block encryption
// If set to (0) then HAL will be used
#define WIFI_AES_ENCRYPT_USE_REGISTERS (1)

// Available in all regions on whole globe... i hope...
#define DEFAULT_WIFI_CHANNEL (6)

// By default will set WiFi to long range (WIFI_PROTOCOL_LR) mode.
// But note what WIFI_PROTOCOL_11B mode use less power but of cost lower range.
// Select between WIFI_PROTOCOL_LR and WIFI_PROTOCOL_11B
#define DEFAULT_WIFI_MODE (WIFI_PROTOCOL_LR)

// By default ESP-NOW use 1M.
#define DEFAULT_WIFI_DATA_RATE (WIFI_PHY_RATE_1M_L)

// By default device will use as much as possible power for Transmitting
// Declare value in percentage with range [WIFI_MIN_TX_POWER_PERCENTAGE : WIFI_MAX_TX_POWER_PERCENTAGE]
#define DEFAULT_WIFI_TX_POWER_1 (60)
// Default Tx value for the Transmitter
#define DEFAULT_WIFI_TX_POWER_2 (60)

#define WIFI_MIN_TX_POWER_PERCENTAGE (1)
#define WIFI_MAX_TX_POWER_PERCENTAGE (100)

#if(DEFAULT_WIFI_TX_POWER_1 < WIFI_MIN_TX_POWER_PERCENTAGE)
#error "WiFi Tx power 1 is too low, see WIFI_MIN_TX_POWER_PERCENTAGE"
#endif

#if(DEFAULT_WIFI_TX_POWER_1 > WIFI_MAX_TX_POWER_PERCENTAGE)
#error "WiFi Tx power 1 is too high, see WIFI_MAX_TX_POWER_PERCENTAGE"
#endif

#if(DEFAULT_WIFI_TX_POWER_2 < WIFI_MIN_TX_POWER_PERCENTAGE)
#error "WiFi Tx power 2 is too low, see WIFI_MIN_TX_POWER_PERCENTAGE"
#endif

#if(DEFAULT_WIFI_TX_POWER_2 > WIFI_MAX_TX_POWER_PERCENTAGE)
#error "WiFi Tx power 2 is too high, see WIFI_MAX_TX_POWER_PERCENTAGE"
#endif

// Range [8, 84] corresponds to 2dBm - 20dBm and is taken from the documentation.
#define WIFI_MIN_TX_POWER (8)
#define WIFI_MAX_TX_POWER (84)

// GB should be fine ? Right?
//  “AT”,”AU”,”BE”,”BG”,”BR”,
//  “CA”,”CH”,”CN”,”CY”,”CZ”,”DE”,”DK”,”EE”,”ES”,”FI”,”FR”,”GB”,”GR”,”HK”,”HR”,”HU”,
//  “IE”,”IN”,”IS”,”IT”,”JP”,”KR”,”LI”,”LT”,”LU”,”LV”,”MT”,”MX”,”NL”,”NO”,”NZ”,”PL”,”PT”,
//  “RO”,”SE”,”SI”,”SK”,”TW”,”US”
#define DEFAULT_WIFI_COUNTRY_CODE ("GB")


#ifdef __cplusplus
}
#endif
#endif /* _WIRELESS_STUFF_CONF_H */