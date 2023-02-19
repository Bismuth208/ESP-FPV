#include "debug_assist.h"

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
#include <assert.h>
#include <stdint.h>
#include <string.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration


// ----------------------------------------------------------------------
// FreeRTOS Variables

#if((CONFIG_ENABLE_DEBUG_TOOLS == 1) && (CONFIG_SYS_STATS_DBG_PRINTOUT == 1))
TaskStatus_t* pxTaskStatusArray = NULL;
//
TimerHandle_t xSysStatsPlotterTimer = NULL;
StaticTimer_t xSysStatsPlotterTimerControlBlock;
#endif // CONFIG_ENABLE_DEBUG_TOOLS && CONFIG_SYS_STATS_DBG_PRINTOUT


// ----------------------------------------------------------------------
// Variables

#if((CONFIG_ENABLE_DEBUG_TOOLS == 1) && (CONFIG_SYS_STATS_DBG_PRINTOUT == 1))
uint8_t ucWriteBuffer[CONFIG_SYS_STATS_BUF_SIZE];
volatile UBaseType_t uxArraySizeAllocated;
#endif // CONFIG_ENABLE_DEBUG_TOOLS && CONFIG_SYS_STATS_DBG_PRINTOUT


// ----------------------------------------------------------------------
// Static functions declaration

#if((CONFIG_ENABLE_DEBUG_TOOLS == 1) && (CONFIG_SYS_STATS_DBG_PRINTOUT == 1))

/**
 * @brief
 */ 
static void init_debug_assist_rtos(void);

/**
 * @brief
 */ 
static void vTaskStatsAlloc(void);


/**
 * @brief
 * 
 * 
 * @example:
 *      Task name    Runtime       CPU   Core  Prior.
 * img_chunk_draw    4604206      23 %      1       1 
 *        Tmr Svc      16530      <1 %      0       1 
 *    task_printf     347239       1 %      0       1 
 *           IDLE   14641658      73 %      1       0 
 *           IDLE    2997849      15 %      0       0 
 *   memory_model       2212      <1 %      0       1 
 *           ipc1     356404       1 %      1      24 
 *           ipc0     341636       1 %      0      24 
 *   img_osd_draw     231030       1 %      1       1 
 *        data_tx      64848      <1 %      1       2 
 *           wifi     641799       3 %      0      23 
 *    img_decoder   15331723      77 %      0       2 
 *      esp_timer          7      <1 %      0      22 
 *        sys_evt         37      <1 %      0      20
 */
static void vGetSysStats(char* pcWriteBuffer, size_t xMaxWriteBufferLen);

/**
 * @brief
 */ 
static void debug_sys_stats_plotter_timer(void);
#endif // CONFIG_ENABLE_DEBUG_TOOLS && CONFIG_SYS_STATS_DBG_PRINTOUT


// ----------------------------------------------------------------------
// Static functions

#if((CONFIG_ENABLE_DEBUG_TOOLS == 1) && (CONFIG_SYS_STATS_DBG_PRINTOUT == 1))
static void
vTaskStatsAlloc(void)
{
	uxArraySizeAllocated = uxTaskGetNumberOfTasks();
	pxTaskStatusArray = pvPortMalloc(uxArraySizeAllocated * sizeof(TaskStatus_t));
}

static void
vGetSysStats(char* pcWriteBuffer, size_t xMaxWriteBufferLen)
{
	volatile UBaseType_t uxArraySize = 0, x;
	unsigned long ulTotalRunTime, ulStatsAsPercentage;
	int iStat = -1;

	// Make sure the write buffer does not contain a string.
	*pcWriteBuffer = 0x00;

	if(pxTaskStatusArray != NULL)
	{
		// Generate raw status information about each task.
		uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySizeAllocated, &ulTotalRunTime);
		ulTotalRunTime /= 100UL;

		if(ulTotalRunTime > 0)
		{
			iStat = snprintf(pcWriteBuffer,
			                 xMaxWriteBufferLen,
			                 "\n%16s %10s %9s %6s %7s\n",
			                 "Task name",
			                 "Runtime",
			                 "CPU",
			                 "Core",
			                 "Prior.");
			if((iStat >= 0) && (iStat < xMaxWriteBufferLen))
			{
				pcWriteBuffer += strnlen((char*)pcWriteBuffer, xMaxWriteBufferLen);
			}


			for(x = 0; x < uxArraySize; x++)
			{
				ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;

				if(ulStatsAsPercentage > 0UL)
				{
					iStat = snprintf(pcWriteBuffer,
					                 xMaxWriteBufferLen,
					                 "%16s %10u %7u %s  %5d %7d \n",
					                 pxTaskStatusArray[x].pcTaskName,
					                 (unsigned int)pxTaskStatusArray[x].ulRunTimeCounter,
					                 (unsigned int)ulStatsAsPercentage,
					                 "%%",
					                 pxTaskStatusArray[x].xCoreID,
					                 pxTaskStatusArray[x].uxBasePriority);
				}
				else
				{
					iStat = snprintf(pcWriteBuffer,
					                 xMaxWriteBufferLen,
					                 "%16s %10u %10s  %5d %7d \n",
					                 pxTaskStatusArray[x].pcTaskName,
					                 (unsigned int)pxTaskStatusArray[x].ulRunTimeCounter,
					                 "<1 %%",
					                 pxTaskStatusArray[x].xCoreID,
					                 pxTaskStatusArray[x].uxBasePriority);
				}

				if((iStat >= 0) && (iStat < xMaxWriteBufferLen))
				{
					pcWriteBuffer += strnlen((char*)pcWriteBuffer, xMaxWriteBufferLen);
				}
				else
				{
					break;
				}
			}
		}
	}
}
#endif // CONFIG_ENABLE_DEBUG_TOOLS && CONFIG_SYS_STATS_DBG_PRINTOUT


static void
init_debug_assist_rtos(void)
{
#if((CONFIG_ENABLE_DEBUG_TOOLS == 1) && (CONFIG_SYS_STATS_DBG_PRINTOUT == 1))
	xSysStatsPlotterTimer = xTimerCreateStatic("xSysStatsPlotterTimer",
	                                           pdMS_TO_TICKS(CONFIG_SYS_STATS_PLOT_TIMEOUT),
	                                           pdTRUE,
	                                           NULL,
	                                           (TimerCallbackFunction_t)(debug_sys_stats_plotter_timer),
	                                           &xSysStatsPlotterTimerControlBlock);
	assert(xSysStatsPlotterTimer);
#endif // CONFIG_ENABLE_DEBUG_TOOLS && CONFIG_SYS_STATS_DBG_PRINTOUT
}

// ----------------------------------------------------------------------
// Accessors functions

void
debug_assist_start(void)
{
#if((CONFIG_ENABLE_DEBUG_TOOLS == 1) && (CONFIG_SYS_STATS_DBG_PRINTOUT == 1))
	vTaskStatsAlloc();
	xTimerStart(xSysStatsPlotterTimer, 0UL);
#endif // CONFIG_ENABLE_DEBUG_TOOLS && CONFIG_SYS_STATS_DBG_PRINTOUT
}

// ----------------------------------------------------------------------
// FreeRTOS functions

#if((CONFIG_ENABLE_DEBUG_TOOLS == 1) && (CONFIG_SYS_STATS_DBG_PRINTOUT == 1))
static void
debug_sys_stats_plotter_timer(void)
{
	vGetSysStats((char*)&ucWriteBuffer[0], sizeof(ucWriteBuffer));
	ASYNC_PRINTF(1, async_print_type_str, (const char*)&ucWriteBuffer[0], 0);
}
#endif // CONFIG_ENABLE_DEBUG_TOOLS && CONFIG_SYS_STATS_DBG_PRINTOUT

// ----------------------------------------------------------------------
// Core functions

void
init_debug_assist(void)
{
	init_debug_assist_rtos();
}