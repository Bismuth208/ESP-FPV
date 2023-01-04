/**
 * @file debug_tools_conf.h
 * 
 * @brief This file provide control of various debug outputs
 * 
 * All of the prints are done asynchronously to the output stream.
 * To enable specific printouts just uncomment required defines below.
 */

#ifndef _DEBUG_TOOLS_CONF_H
#define _DEBUG_TOOLS_CONF_H

// #ifdef __cplusplus
// extern "C" {
// #endif

// Enable/disable whole debug output
// #define ENABLE_DEBUG_TOOLS


#include <debug_tools_esp.h>

// ---------------------------------
// Uncomment defines below to enable logs:
//  - with general debug prints
//
// Naming rule:
//  - #define YOUR_NAME##_DBG_PRINTOUT (enable_state: 1/0)

// Print what "TaskName" is started
#define ENABLE_TASK_START_EVENT_DBG_PRINTOUT (0)

//
#define IMAGE_DECODE_TIME_DBG_PRINTOUT (0)

//
#define OSD_UPDATE_TIME_DBG_PRINTOUT (0)

// wifi_packet_rx_cb
#define WIFI_RX_PACKET_CB_DBG_PRINTOUT (0)

// rx_data_callback
#define WIFI_RX_DATA_CB_DBG_PRINTOUT (0)

// esp_now
#define ESP_NOW_SEND_PACKET_FAIL_DBG_PRINTOUT (0)

//
#define ESP_NOW_INIT_FAIL_DBG_PRINTOUT (0)

//
#define SELF_MAC_TELL_DBG_PRINTOUT (0)

//
#define WIRELESS_TELL_SCAN_PROGRESS (0)

// This parameter requirer to enable
// at "Component config -> FreeRTOS -> Kernel ->":
//  - configUSE_TRACE_FACILITY
//  - Enable display of xCoreId in vTaskList
//  - configGENERATE_RUN_TIME_STATS
// #define SYS_STATS_DBG_PRINTOUT


// ---------------------------------
// Uncomment defines below to enable logs:
//  - with profile debug prints
// Note: only one at time should be uncommented!
//
// Naming rule:
//  - #define YOUR_NAME##_DBG_PROFILER (enable_state: 1/0)
//
// Profiler point Id's
//
// NOTE: Point Id Should not be higher than @ref PROFILER_POINTS_MAX
//
// Naming rule:
//  - #define YOUR_NAME##_DBG_PROFILER_POINT_ID ()

//
#define JD_CHUNK_DECODE_TIME_DBG_PROFILER          (0)
#define JD_CHUNK_DECODE_TIME_DBG_PROFILER_POINT_ID (2)

//
#define JD_OUTPUT_DBG_PROFILER          (0)
#define JD_OUTPUT_DBG_PROFILER_POINT_ID (3)

//
#define JD_DECODE_DBG_PROFILER          (0)
#define JD_DECODE_DBG_PROFILER_POINT_ID (4)

//
#define ESP_NOW_TASK_PACKET_SEND_DBG_PROFILER          (0)
#define ESP_NOW_TASK_PACKET_SEND_DBG_PROFILER_POINT_ID (5)

//
#define ESP_NOW_RX_DATA_DBG_PROFILER          (0)
#define ESP_NOW_RX_DATA_DBG_PROFILER_POINT_ID (6)

//
#define IMG_CHUNK_DRAW_DBG_PROFILER          (0)
#define IMG_CHUNK_DRAW_DBG_PROFILER_POINT_ID (7)

//
#define OSD_DRAW_TIME_DBG_PROFILER          (0)
#define OSD_DRAW_TIME_DBG_PROFILER_POINT_ID (8)

// Check for how long it take to Encrypt/Decrypt data
#define AES_ENCRYPTION_TIME_DBG_PROFILER          (0)
#define AES_ENCRYPTION_TIME_DBG_PROFILER_POINT_ID (9)

// #ifdef __cplusplus
// }
// #endif
#endif // _DEBUG_TOOLS_CONF_H
