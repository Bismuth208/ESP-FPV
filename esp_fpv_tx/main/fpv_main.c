#include "camera.h"
#include "data_common.h"
#include "debug_tools_conf.h"
#include "wireless/wireless_main.h"

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
#include <driver/gpio.h>
#include <driver/uart.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>


// ----------------------------------------------------------------------
// Static functions declaration


// ----------------------------------------------------------------------
// FreeRTOS Variables

EventGroupHandle_t xTaskSyncEventGroupHandler;
StaticEventGroup_t xTaskSyncBlockEventGroup;


// ----------------------------------------------------------------------
// Variables


// ----------------------------------------------------------------------
// Static functions

void
init_main_rtos(void)
{
	xTaskSyncEventGroupHandler = xEventGroupCreateStatic(&xTaskSyncBlockEventGroup);
	assert(xTaskSyncEventGroupHandler);
}


// ----------------------------------------------------------------------
// Accessors functions

int32_t
ul_map_val(const int32_t x, int32_t imin, int32_t imax, int32_t omin, int32_t omax)
{
	return (x - imin) * (omax - omin) / (imax - imin) + omin;
}


void
task_sync_set_bits(uint32_t ulBits)
{
	xEventGroupSetBits(xTaskSyncEventGroupHandler, ulBits);
}

void
task_sync_get_bits(uint32_t ulBits)
{
	xEventGroupWaitBits(xTaskSyncEventGroupHandler, ulBits, pdTRUE, pdFALSE, portMAX_DELAY);
}


// ----------------------------------------------------------------------
// FreeRTOS functions


// ----------------------------------------------------------------------
// Core functions

void
app_main()
{
	init_debug_assist();
	init_main_rtos();
	init_wireless();
	init_camera();

	// ----------------------------------------------------------------------
	// start everything now safely
	task_sync_set_bits(TASK_SYNC_EVENT_BIT_ALL);

	debug_assist_start();

	// I don't want to play with you anymore
	vTaskDelete(NULL);
}