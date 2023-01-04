/**
 * @file memory_model.h
 * 
 * @brief Provide API functions to rest of the modules
 */

#ifndef _MEMORY_MODEL_H
#define _MEMORY_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "memory_model_types.h"

//
#include <sdkconfig.h>
//
#include <freertos/FreeRTOS.h>
//
#include <stdint.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration

/**
 * Definition of the callback function what will be called when any variable
 * will be updated via @ref ''vMemoryModelSet''
 * 
 * @note DO NOT CALL @ref ''vMemoryModelSet'' INSIDE CALLBACK FUNCTION!
 *       OR IT WILL CREATE RECURSIVE CALL AND BLOW UP YOUR STACK!
 */
typedef void (*memory_model_callback_t)(memory_model_types_t xDataId);

// ----------------------------------------------------------------------
// Accessors functions

/**
 * @brief Tell memory_model to register callback for any updates of any variable
 * 
 * @param xFunc Pointer to external callback function 
 *              which will be called after ANY @ref ''vMemoryModelSet''
 * 
 * @retval pdTRUE if operation was successful, 
 *         pdFALSE if MEMORY_MODEL_MAX_CALLBACKS has been reached
 * 
 * @attention If one process called memory_model other will be blocked! 
 */
BaseType_t xMemoryModelRegisterCallback(memory_model_callback_t xFunc);

/**
 * @brief Tell memory_model to occupy one slot for provided xDataId
 * 
 * @param xDataId Data identification in memory_model see @ref ''memory_model_types_t''
 * 
* @retval pdTRUE if operation was successful, 
 *         pdFALSE if MEMORY_MODEL_MAX_ITEMS has been reached
 * 
 * @attention If one process called memory_model other will be blocked! 
 */
BaseType_t xMemoryModelRegisterItem(memory_model_types_t xDataId);

/**
 * @brief Update value inside memory_model
 * 
 * @param xDataId Data identification in memory_model see @ref ''memory_model_types_t''
 * @param ulData New value to be stored
 * 
 * @attention Call all registered callback one by one!
 * @attention If one process called memory_model other will be blocked!
 */
void vMemoryModelSet(memory_model_types_t xDataId, uint32_t ulData);

/**
 * @brief Return requested value specified by xDataId
 * 
 * @param xDataId Data identification in memory_model see @ref ''memory_model_types_t''
 * 
 * @retval If ''xDataId'' has been found return it's value, otherwise 0 is returned.
 * 
 * @attention If one process called memory_model other will be blocked! 
 */
uint32_t ulMemoryModelGet(memory_model_types_t xDataId);


// ----------------------------------------------------------------------
// Core functions

/**
 * @brief Clean internal storage and all settings
 * 
 * Also creates FreeRTOS task observer for data updates and callbacks.
 */
void init_memory_model(void);

#ifdef __cplusplus
}
#endif

#endif /* _MEMORY_MODEL_H */