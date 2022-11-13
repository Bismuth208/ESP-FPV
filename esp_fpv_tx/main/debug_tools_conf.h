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

#ifdef __cplusplus
extern "C" {
#endif

// Enable/disable whole debug output
// #define ENABLE_DEBUG_TOOLS


#ifdef ENABLE_DEBUG_TOOLS
#include <debug_tools_esp.h>

// ---------------------------------
// Uncomment defines below to enable logs:
//  - with general debug prints
//
// Naming rule:
//  - #define YOUR_NAME##_DBG_PRINTOUT

// Print what "TaskName" is started
//#define ENABLE_TASK_START_EVENT_DBG_PRINTOUT

// esp_now
//#define ESP_NOW_SEND_PACKET_FAIL_DBG_PRINTOUT

//
//#define TOTAL_PACKETS_SEND_DBG_PRINTOUT

//
//#define ESP_NOW_INIT_FAIL_DBG_PRINTOUT

//
//#define CAMERA_INIT_FAIL_DBG_PRINTOUT

//
// #define CAMERA_CAPTURE_FAIL_DBG_PRINTOUT

//
//#define IMAGE_TX_TIME_DBG_PRINTOUT

//
//#define IMAGE_TX_SIZE_DBG_PRINTOUT

// wifi_packet_rx_cb
//#define WIFI_RX_PACKET_CB_DBG_PRINTOUT

// rx_data_callback
//#define WIFI_RX_DATA_CB_DBG_PRINTOUT

// vForceFrameUpdateTimer
//#define TIMER_FRAME_UPDATE_DBG_PRINTOUT

//
//#define SELF_MAC_TELL_DBG_PRINTOUT

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
//  - #define YOUR_NAME##_DBG_PROFILER
//
// Profiler point Id's
//
// NOTE: Point Id Should not be higher than @ref PROFILER_POINTS_MAX
//
// Naming rule:
//  - #define YOUR_NAME##_DBG_PROFILER_POINT_ID ()

// 
// #define QUEUE_PACKET_SEND_DBG_PROFILER
#define QUEUE_PACKET_SEND_DBG_PROFILER_POINT_ID (3)

//
// #define NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER
#define NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER_POINT_ID (4)

// 
//#define ESP_NOW_TASK_PACKET_SEND_DBG_PROFILER
#define ESP_NOW_TASK_PACKET_SEND_DBG_PROFILER_POINT_ID (5)

// Check for how long it take to Encypt/Decrypt data
// #define AES_ENCRYPTION_TIME_DBG_PROFILER
#define AES_ENCRYPTION_TIME_DBG_PROFILER_POINT_ID (9)

// Check for how long it take to find actual Jpg EOI
// #define JPG_EOI_SEARCH_TIME_DBG_PROFILER
#define JPG_EOI_SEARCH_TIME_DBG_PROFILER_POINT_ID (11)

// How much time it takes to convert and copy data from DMA callback.
// And how much time it takes to copy whole image to Tx queue
// #define JPG_DMA_COPY_TIME_DBG_PROFILER
#define JPG_DMA_COPY_TIME_DBG_PROFILER_POINT_ID (12)


#ifdef __cplusplus
}
#endif
#endif // ENABLE_DEBUG_TOOLS
#endif // _DEBUG_TOOLS_CONF_H
