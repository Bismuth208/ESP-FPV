/**
 * @file memory_model.types.h
 * 
 * @brief Contain list of all possible data types used by modules.
 */ 

#ifndef _MEMORY_MODEL_TYPES_H
#define _MEMORY_MODEL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

// Types of data what could be stored
typedef enum
{
	MEMORY_MODEL_WIFI_SCAN_CHANNEL = 0,
	MEMORY_MODEL_WIFI_CURRENT_CHANNEL,
	MEMORY_MODEL_WIFI_TX_POWER_1,
	MEMORY_MODEL_WIFI_TX_POWER_2,
	MEMORY_MODEL_WIFI_RTT_VALUE,
	MEMORY_MODEL_DATA_RX_RATE,
	MEMORY_MODEL_WIFI_RX_RSSI,
	MEMORY_MODEL_TOTAL,
	MEMORY_MODEL_EMPTY = 0xffffffff
} memory_model_types_t;


#ifdef __cplusplus
}
#endif

#endif /* _MEMORY_MODEL_TYPES_H */