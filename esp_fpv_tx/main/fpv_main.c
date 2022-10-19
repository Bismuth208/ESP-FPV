#include "camera.h"
#include "data_common.h"
#include "debug_tools_conf.h"
#include "wireless/wireless_main.h"

//
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
//
#include <driver/gpio.h>
#include <driver/uart.h>
//
#include <stdint.h>
#include <string.h>


// ----------------------------------------------------------------------
// Static functions declaration

#ifdef ENABLE_DEBUG_TOOLS
static void init_async_printf(void);

static void vPrintfTask(void* pvArg);
#endif


// ----------------------------------------------------------------------
// FreeRTOS Variables

#ifdef ENABLE_DEBUG_TOOLS
// Print all stuff to console
#define STACK_WORDS_SIZE_FOR_TASK_PRINTF (2048)
#define PRIORITY_LEVEL_FOR_TASK_PRINTF   (1)
#define PINNED_CORE_FOR_TASK_PRINTF      (0)
const char* assigned_name_for_task_printf = "task_printf\n\0";
TaskHandle_t xPrintfTaskHandler = NULL;
StaticTask_t xPrintfTaskControlBlock;
StackType_t xPrintfStack[STACK_WORDS_SIZE_FOR_TASK_PRINTF];
#endif


EventGroupHandle_t xTaskSyncEventGroupHandler;
StaticEventGroup_t xTaskSyncBlockEventGroup;

// ----------------------------------------------------------------------
#ifdef ENABLE_DEBUG_TOOLS
void
vPrintfTask(void* pvArg)
{
	(void)pvArg;

#ifdef TASK_START_EVENT_DBG_PRINTOUT
	async_printf(async_print_type_str, assigned_name_for_task_logs, 0);
#endif

	for(;;)
	{
		async_printf_sync();
		vTaskDelay(1);
	}
}
#endif

// ----------------------------------------------------------------------
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
#ifdef ENABLE_DEBUG_TOOLS
void
init_async_printf(void)
{
	xPrintfTaskHandler = xTaskCreateStaticPinnedToCore((TaskFunction_t)(vPrintfTask),
	                                                   assigned_name_for_task_printf,
	                                                   STACK_WORDS_SIZE_FOR_TASK_PRINTF,
	                                                   NULL,
	                                                   PRIORITY_LEVEL_FOR_TASK_PRINTF,
	                                                   xPrintfStack,
	                                                   &xPrintfTaskControlBlock,
	                                                   (BaseType_t)PINNED_CORE_FOR_TASK_PRINTF);
	assert(xPrintfTaskHandler);
}
#endif

void
init_main_rtos(void)
{
	xTaskSyncEventGroupHandler = xEventGroupCreateStatic(&xTaskSyncBlockEventGroup);
	assert(xTaskSyncEventGroupHandler);
}

void
app_main()
{
#ifdef ENABLE_DEBUG_TOOLS
	init_async_printf();
#endif
	init_main_rtos();
	init_wireless();
	init_camera();

	// ----------------------------------------------------------------------
	// start everything now safely
	task_sync_set_bits(TASK_SYNC_EVENT_BIT_ALL);

	// I don't want to play with you anymore
	vTaskDelete(NULL);
}