#include "data_common.h"
#include "debug_tools_conf.h"
#include "display_osd.h"
#include "image_decoder.h"
#include "memory_model/memory_model.h"
#include "pins_definitions.h"
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
#include <esp_attr.h>
#include <esp_timer.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>


// ----------------------------------------------------------------------
// Definitions, type & enum declaration


// ----------------------------------------------------------------------
// FreeRTOS Variables

// For Task syncronization
EventGroupHandle_t xTaskSyncEventGroupHandler;
StaticEventGroup_t xTaskSyncBlockEventGroup;


// ----------------------------------------------------------------------
// Variables


// ----------------------------------------------------------------------
// Static functions declaration

static void init_gpio(void);

static void init_main_rtos(void);


// ----------------------------------------------------------------------
// Static functions


static void
init_gpio(void)
{
	gpio_set_direction(BUTTON_1, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BUTTON_1, GPIO_PULLUP_ONLY);
}


static void
init_main_rtos(void)
{
	xTaskSyncEventGroupHandler = xEventGroupCreateStatic(&xTaskSyncBlockEventGroup);
	assert(xTaskSyncEventGroupHandler);
}

// ----------------------------------------------------------------------
// Accessors functions

button_states_t
xReadButton(gpio_num_t gpio_num)
{
	return (button_states_t)gpio_get_level(gpio_num);
}


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
app_main(void)
{
	init_debug_assist();
	init_main_rtos();
	init_memory_model();
	init_gpio();

	init_wireless();
	init_display();
	init_osd_stats();
	init_image_decoder();

	// --------------------------
	// start everything now safely
	task_sync_set_bits(TASK_SYNC_EVENT_BIT_ALL);

	debug_assist_start();

	// I don't want to play with you anymore
	vTaskDelete(NULL);
}
