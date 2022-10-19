/**
 * @file memory_model_cond.h
 * 
 * @brief Configuration file for the Memory Model Module.
 */

#ifndef _MEMORY_MODEL_CONF_H
#define _MEMORY_MODEL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum amount of added variables via @ref ''xMemoryModelRegisterItem''
 */
#define MEMORY_MODEL_MAX_ITEMS (64)

/**
 *  Maximum amount of added callback functions via @ref ''xMemoryModelRegisterCallback''
 */
#define MEMORY_MODEL_MAX_CALLBACKS (5)

/// In some cases there is no need for heavy Mutex
/// Just a single variable is more than enough
// #define MEMORY_MODEL_USE_RTOS_LOCK

/// Do the update every MEMORY_MODEL_REFRESH_TIMEOUT milliseconds
/// with observer @ref ''vMemoryModelTask''
#define MEMORY_MODEL_REFRESH_TIMEOUT (100)


#ifdef __cplusplus
}
#endif
#endif /* _MEMORY_MODEL_CONF_H */