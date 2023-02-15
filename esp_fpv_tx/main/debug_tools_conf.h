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
#define TOTAL_PACKETS_SEND_DBG_PRINTOUT (0)

// esp_now
#define ESP_NOW_SEND_PACKET_FAIL_DBG_PRINTOUT (0)

//
#define ESP_NOW_INIT_FAIL_DBG_PRINTOUT (0)

//
#define CAMERA_INIT_FAIL_DBG_PRINTOUT (1)

//
#define CAMERA_CAPTURE_FAIL_DBG_PRINTOUT (0)

//
#define IMAGE_TX_TIME_DBG_PRINTOUT (0)

//
#define IMAGE_TX_SIZE_DBG_PRINTOUT (0)

// wifi_packet_rx_cb
#define WIFI_RX_PACKET_CB_DBG_PRINTOUT (0)

// rx_data_callback
#define WIFI_RX_DATA_CB_DBG_PRINTOUT (0)

// vForceFrameUpdateTimer
#define TIMER_FRAME_UPDATE_DBG_PRINTOUT (0)

//
#define SELF_MAC_TELL_DBG_PRINTOUT (0)

/**
 * This parameter requirer to enable
 * at "Component config -> FreeRTOS -> Kernel ->":
 * - configUSE_TRACE_FACILITY
 * - Enable display of xCoreId in vTaskList
 * - configGENERATE_RUN_TIME_STATS
 * 
 * Output example:
 *        Task name    Runtime       CPU   Core  Prior.
 *        Tmr Svc         56      <1 %      0       1 
 *    task_printf       1448      <1 %      0       1 
 *           IDLE    1524369      96 %      1       0 
 *           IDLE    1135987      72 %      0       0 
 *           ipc1      44731       2 %      1      24 
 *     dma_filter         15      <1 %      1      10 
 *           ipc0      14188      <1 %      0       1 
 *     CameraTask         40      <1 %      1       1 
 *        data_tx         38      <1 %      0       1 
 *           wifi     149289       9 %      0      23 
 *      esp_timer          9      <1 %      0      22 
 */
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
#define QUEUE_PACKET_SEND_DBG_PROFILER          (0)
#define QUEUE_PACKET_SEND_DBG_PROFILER_POINT_ID (3)

//
#define NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER          (0)
#define NEW_IMAGE_FRAME_TX_TIME_DBG_PROFILER_POINT_ID (4)

//
#define ESP_NOW_TASK_PACKET_SEND_DBG_PROFILER          (0)
#define ESP_NOW_TASK_PACKET_SEND_DBG_PROFILER_POINT_ID (5)

//
#define ESP_NOW_RX_DATA_DBG_PROFILER          (0)
#define ESP_NOW_RX_DATA_DBG_PROFILER_POINT_ID (6)

// Check for how long it take to Encrypt/Decrypt data
#define AES_ENCRYPTION_TIME_DBG_PROFILER          (0)
#define AES_ENCRYPTION_TIME_DBG_PROFILER_POINT_ID (9)

// Check for how long it take to find actual Jpg EOI
#define JPG_EOI_SEARCH_TIME_DBG_PROFILER          (0)
#define JPG_EOI_SEARCH_TIME_DBG_PROFILER_POINT_ID (11)

// How much time it takes to convert and copy data from DMA callback.
// And how much time it takes to copy whole image to Tx queue
#define JPG_DMA_COPY_TIME_DBG_PROFILER          (0)
#define JPG_DMA_COPY_TIME_DBG_PROFILER_POINT_ID (12)


#ifdef __cplusplus
}
#endif
#endif // _DEBUG_TOOLS_CONF_H
