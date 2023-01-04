#ifndef _DATA_COMMON_H
#define _DATA_COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// ----------------------------------------------------------------------
// Definitions, type & enum declaration
#define TASK_SYNC_EVENT_BIT_DATA_TX (1 << 0)
#define TASK_SYNC_EVENT_BIT_CAMERA  (1 << 1)
#define TASK_SYNC_EVENT_BIT_ALL     (TASK_SYNC_EVENT_BIT_DATA_TX | TASK_SYNC_EVENT_BIT_CAMERA)


// ----------------------------------------------------------------------
// Accessors functions

/**
 * @brief Just a liniar range converter
 * 
 * @param
 * @param
 * @param
 * @param
 * @param
 * 
 * @retval Convereted value
 */
int32_t ul_map_val(const int32_t x, int32_t imin, int32_t imax, int32_t omin, int32_t omax);

void task_sync_set_bits(uint32_t ulBits);
void task_sync_get_bits(uint32_t ulBits);

/**
 * @brief 
 */
void init_debug_assist(void);

/**
 * @brief 
 */
void debug_assist_start(void);

#ifdef __cplusplus
}
#endif

#endif // _DATA_COMMON_H
