/**
 * @file memory_model.c
 * 
 * @brief Provide Global data storage with thread safe access
 */

#include "memory_model.h"

#include "memory_model_conf.h"

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
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


// ----------------------------------------------------------------------
// Definitions, type & enum declaration

typedef struct
{
	memory_model_types_t xDataId;
	volatile uint32_t ulValue;
	volatile BaseType_t xUpdated;
} memory_model_t;


// ----------------------------------------------------------------------
// FreeRTOS Variables

#define STACK_WORDS_SIZE_FOR_TASK_MEMORY_MODEL (2048)
#define PRIORITY_LEVEL_FOR_TASK_MEMORY_MODEL   (1)
#define PINNED_CORE_FOR_TASK_MEMORY_MODEL      (0)
const char* assigned_name_for_task_memory_model = "memory_model";
TaskHandle_t xMemoryModelTaskHandler = NULL;
StaticTask_t xMemoryModelTaskControlBlock;
StackType_t xMemoryModelStack[STACK_WORDS_SIZE_FOR_TASK_MEMORY_MODEL];

#ifdef MEMORY_MODEL_USE_RTOS_LOCK
SemaphoreHandle_t xMemoryModelMutexHandler = NULL;
StaticSemaphore_t xMemoryModelMutexControlBlock;
#endif

// ----------------------------------------------------------------------
// Variables

//
#ifndef MEMORY_MODEL_USE_RTOS_LOCK
static volatile BaseType_t xMemoryModelLockState = pdFALSE;
#endif

// ----------------------------
// Internal storage of the memory_model
static memory_model_t xMemoryModelStorage[MEMORY_MODEL_MAX_ITEMS];
static memory_model_callback_t xMemoryModelCallbackStorage[MEMORY_MODEL_MAX_CALLBACKS];

// ----------------------------------------------------------------------
// Static functions declaration

/**
 * @brief Call all registered callback functions and tell them what kind of variable was updated
 * 
 * @param xDataId Data identification in memory_model see @ref ''memory_model_types_t'' 
 */
static inline void MemoryModelUpdated(memory_model_types_t xDataId);

/**
 * @brief Lock acces for memory_model and prevent multiple writes
 * 
 */
static inline void vMemoryModelLock(void);

/**
 * @brief Unlock acces for memory_model
 */
static inline void vMemoryModelUnlock(void);

/**
 * @brief Memory model observer task.
 */
static void vMemoryModelTask(void* pvArg);

/**
 * @brief Create FreeRTOS objects if @ref ''MEMORY_MODEL_USE_RTOS_LOCK'' is enabled.
 *        If not, then just BaseType_t variable is used as Mutex.
 */
static void init_memory_model_rtos(void);

// ----------------------------------------------------------------------
// Static functions

static inline void
MemoryModelUpdated(memory_model_types_t xDataId)
{
	for(size_t i = 0; i < MEMORY_MODEL_MAX_CALLBACKS; i++)
	{
		if(NULL != xMemoryModelCallbackStorage[i])
		{
			xMemoryModelCallbackStorage[i](xDataId);
		}
	}
}


static inline void
vMemoryModelLock(void)
{
#ifdef MEMORY_MODEL_USE_RTOS_LOCK
	xSemaphoreTake(xMemoryModelMutexHandler, portMAX_DELAY);
#else
	if(xMemoryModelLockState == pdTRUE)
	{
		do
		{
			vTaskDelay(1); // pause for one sys tick
		} while(xMemoryModelLockState == pdTRUE);
	}

	xMemoryModelLockState = pdTRUE;
#endif
}


static inline void
vMemoryModelUnlock(void)
{
#ifdef MEMORY_MODEL_USE_RTOS_LOCK
	xSemaphoreGive(xMemoryModelMutexHandler);
#else
	xMemoryModelLockState = pdFALSE;
#endif
}


static void
init_memory_model_rtos(void)
{
#ifdef MEMORY_MODEL_USE_RTOS_LOCK
	xMemoryModelMutexHandler = xSemaphoreCreateMutexStatic(&xMemoryModelMutexControlBlock);
	assert(xMemoryModelMutexHandler);
#endif

	xMemoryModelTaskHandler = xTaskCreateStaticPinnedToCore((TaskFunction_t)(vMemoryModelTask),
	                                                        assigned_name_for_task_memory_model,
	                                                        STACK_WORDS_SIZE_FOR_TASK_MEMORY_MODEL,
	                                                        NULL,
	                                                        PRIORITY_LEVEL_FOR_TASK_MEMORY_MODEL,
	                                                        xMemoryModelStack,
	                                                        &xMemoryModelTaskControlBlock,
	                                                        (BaseType_t)PINNED_CORE_FOR_TASK_MEMORY_MODEL);
	assert(xMemoryModelTaskHandler);
}

// ----------------------------------------------------------------------
// Accessors functions

BaseType_t
xMemoryModelRegisterCallback(memory_model_callback_t xFunc)
{
	assert(xFunc);

	BaseType_t xRes = pdFALSE;
	vMemoryModelLock();

	for(size_t i = 0; i < MEMORY_MODEL_MAX_CALLBACKS; i++)
	{
		if(NULL == xMemoryModelCallbackStorage[i])
		{
			xMemoryModelCallbackStorage[i] = xFunc;
			xRes = pdTRUE;
			break;
		}
	}

	vMemoryModelUnlock();
	return xRes;
}


BaseType_t
xMemoryModelRegisterItem(memory_model_types_t xDataId)
{
	assert(xDataId < MEMORY_MODEL_TOTAL);

	BaseType_t xRes = pdFALSE;
	vMemoryModelLock();

	for(memory_model_t* pxStorage = &xMemoryModelStorage[0]; pxStorage < &xMemoryModelStorage[MEMORY_MODEL_MAX_ITEMS];
	    pxStorage++)
	{
		if(pxStorage->xDataId == MEMORY_MODEL_EMPTY)
		{
			pxStorage->xDataId = xDataId;
			xRes = pdTRUE;
			break;
		}
	}

	vMemoryModelUnlock();
	return xRes;
}


void
vMemoryModelSet(memory_model_types_t xDataId, uint32_t ulData)
{
	assert(xDataId < MEMORY_MODEL_TOTAL);

	vMemoryModelLock();

	memory_model_t* pxStorage = &xMemoryModelStorage[xDataId];

	if(pxStorage->ulValue != ulData)
	{
		pxStorage->ulValue = ulData;
		pxStorage->xUpdated = pdTRUE;
	}

	vMemoryModelUnlock();
}


uint32_t
ulMemoryModelGet(memory_model_types_t xDataId)
{
	assert(xDataId < MEMORY_MODEL_TOTAL);

	uint32_t ulValue = 0;
	vMemoryModelLock();

	ulValue = xMemoryModelStorage[xDataId].ulValue;

	vMemoryModelUnlock();
	return ulValue;
}

// ----------------------------------------------------------------------
// FreeRTOS functions

static void
vMemoryModelTask(void* pvArg)
{
	(void)pvArg;

	const TickType_t xNextFrameTime = pdMS_TO_TICKS(MEMORY_MODEL_REFRESH_TIMEOUT);
	TickType_t xLastWakeTime = xTaskGetTickCount();

	for(;;)
	{
		for(memory_model_t* pxStorage = &xMemoryModelStorage[0]; pxStorage < &xMemoryModelStorage[MEMORY_MODEL_MAX_ITEMS];
		    pxStorage++)
		{
			if(pxStorage->xUpdated == pdTRUE)
			{
				// Do it before callback function.
				// Callback could overwrite once again xUpdated flag.
				// In normal cases this SHOULD NOT happen, if it does,
				// then, rethink you architecture...
				pxStorage->xUpdated = pdFALSE;

				// Now, tell to the subscribed module what there is an update of data.
				MemoryModelUpdated(pxStorage->xDataId);
			}
		}

		vTaskDelayUntil(&xLastWakeTime, xNextFrameTime);
	}

	vTaskDelete(NULL);
}

// ----------------------------------------------------------------------
// Core functions

void
init_memory_model(void)
{
	memset(&xMemoryModelStorage[0], 0xff, sizeof(xMemoryModelStorage));
	memset(&xMemoryModelCallbackStorage[0], 0x00, sizeof(xMemoryModelCallbackStorage));

	init_memory_model_rtos();
}