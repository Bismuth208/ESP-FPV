#include "data_common.h"
#include "debug_tools_conf.h"

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
#include <assert.h>
#include <stdint.h>
#include <string.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration


// ----------------------------------------------------------------------
// FreeRTOS Variables

#ifdef ENABLE_DEBUG_TOOLS
// Print all stuff to console
#define STACK_WORDS_SIZE_FOR_TASK_PRINTF (2048)
#define PRIORITY_LEVEL_FOR_TASK_PRINTF   (1)
#define PINNED_CORE_FOR_TASK_PRINTF      (0)
const char* assigned_name_for_task_printf = "task_printf";
TaskHandle_t xPrintfTaskHandler = NULL;
StaticTask_t xPrintfTaskControlBlock;
StackType_t xPrintfStack[STACK_WORDS_SIZE_FOR_TASK_PRINTF];

#ifdef SYS_STATS_DBG_PRINTOUT
TaskStatus_t* pxTaskStatusArray = NULL;

//
#define SYS_STATS_PLOT_TIMEOUT (1000)
TimerHandle_t xSysStatsPlotterTimer = NULL;
StaticTimer_t xSysStatsPlotterTimerControlBlock;
#endif // SYS_STATS_DBG_PRINTOUT

#endif // ENABLE_DEBUG_TOOLS


// ----------------------------------------------------------------------
// Variables

#ifdef SYS_STATS_DBG_PRINTOUT
uint8_t ucWriteBuffer[8192];
volatile UBaseType_t uxArraySizeAllocated;
#endif


// ----------------------------------------------------------------------
// Static functions declaration

#ifdef ENABLE_DEBUG_TOOLS
static void init_debug_assist_rtos(void);

static void vPrintfTask(void* pvArg);
#endif


#ifdef SYS_STATS_DBG_PRINTOUT
static void vTaskStatsAlloc(void);

static void vGetSysStats(char* pcWriteBuffer);

static void debug_sys_stats_plotter_timer(void);
#endif


// ----------------------------------------------------------------------
// Static functions

#ifdef SYS_STATS_DBG_PRINTOUT
static void
vTaskStatsAlloc(void)
{
	uxArraySizeAllocated = uxTaskGetNumberOfTasks();
	pxTaskStatusArray = pvPortMalloc(uxArraySizeAllocated * sizeof(TaskStatus_t));
}

static void
vGetSysStats(char* pcWriteBuffer)
{
	volatile UBaseType_t uxArraySize = 0, x;
	unsigned long ulTotalRunTime, ulStatsAsPercentage;

	// Make sure the write buffer does not contain a string.
	*pcWriteBuffer = 0x00;

	if(pxTaskStatusArray != NULL)
	{
		// Generate raw status information about each task.
		uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySizeAllocated, &ulTotalRunTime);
		ulTotalRunTime /= 100UL;

		if(ulTotalRunTime > 0)
		{
			sprintf(pcWriteBuffer, "\n%16s %10s %9s %6s %7s\n", "Task name", "Runtime", "CPU", "Core", "Prior.");
			pcWriteBuffer += strlen((char*)pcWriteBuffer);

			for(x = 0; x < uxArraySize; x++)
			{
				ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;

				if(ulStatsAsPercentage > 0UL)
				{
					sprintf(pcWriteBuffer,
					        "%16s %10u %7u %s  %5d %7d \n",
					        pxTaskStatusArray[x].pcTaskName,
					        pxTaskStatusArray[x].ulRunTimeCounter,
					        ulStatsAsPercentage,
					        "%%",
					        pxTaskStatusArray[x].xCoreID,
					        pxTaskStatusArray[x].uxBasePriority);
				}
				else
				{
					sprintf(pcWriteBuffer,
					        "%16s %10u %10s  %5d %7d \n",
					        pxTaskStatusArray[x].pcTaskName,
					        pxTaskStatusArray[x].ulRunTimeCounter,
					        "<1 %%",
					        pxTaskStatusArray[x].xCoreID,
					        pxTaskStatusArray[x].uxBasePriority);
				}

				pcWriteBuffer += strlen((char*)pcWriteBuffer);
			}
		}
	}
}

static void
debug_sys_stats_plotter_timer(void)
{
	vGetSysStats((char*)&ucWriteBuffer[0]);
	async_printf(async_print_type_str, (const char*)&ucWriteBuffer[0], 0);
}
#endif // SYS_STATS_DBG_PRINTOUT


static void
init_debug_assist_rtos(void)
{
#ifdef ENABLE_DEBUG_TOOLS
	xPrintfTaskHandler = xTaskCreateStaticPinnedToCore((TaskFunction_t)(vPrintfTask),
	                                                   assigned_name_for_task_printf,
	                                                   STACK_WORDS_SIZE_FOR_TASK_PRINTF,
	                                                   NULL,
	                                                   PRIORITY_LEVEL_FOR_TASK_PRINTF,
	                                                   xPrintfStack,
	                                                   &xPrintfTaskControlBlock,
	                                                   (BaseType_t)PINNED_CORE_FOR_TASK_PRINTF);
	assert(xPrintfTaskHandler);


#ifdef SYS_STATS_DBG_PRINTOUT
	xSysStatsPlotterTimer = xTimerCreateStatic("xSysStatsPlotterTimer",
	                                           pdMS_TO_TICKS(SYS_STATS_PLOT_TIMEOUT),
	                                           pdTRUE,
	                                           NULL,
	                                           (TimerCallbackFunction_t)(debug_sys_stats_plotter_timer),
	                                           &xSysStatsPlotterTimerControlBlock);
	assert(xSysStatsPlotterTimer);
#endif // SYS_STATS_DBG_PRINTOUT
#endif // ENABLE_DEBUG_TOOLS
}

// ----------------------------------------------------------------------
// Accessors functions

void
debug_assist_start(void)
{
#ifdef SYS_STATS_DBG_PRINTOUT
	vTaskStatsAlloc();
	xTimerStart(xSysStatsPlotterTimer, 0UL);
#endif
}

// ----------------------------------------------------------------------
// FreeRTOS functions

#ifdef ENABLE_DEBUG_TOOLS
static void
vPrintfTask(void* pvArg)
{
	(void)pvArg;

#ifdef TASK_START_EVENT_DBG_PRINTOUT
	async_printf(async_print_type_str, assigned_name_for_task_printf, 0);
#endif

	for(;;)
	{
		async_printf_sync();
		vTaskDelay(1);
	}
}
#endif

// ----------------------------------------------------------------------
// Core functions

void
init_debug_assist(void)
{
	init_debug_assist_rtos();
}