#ifndef _DATA_COMMON_H
#define _DATA_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

//
#include <driver/gpio.h>
//
#include <stdint.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration

#define TASK_SYNC_EVENT_BIT_DATA_TX        (1 << 0)
#define TASK_SYNC_EVENT_BIT_IMG_OSD_DRAW   (1 << 1)
#define TASK_SYNC_EVENT_BIT_IMG_CHUNK_DRAW (1 << 2)
#define TASK_SYNC_EVENT_BIT_IMG_PROCESS    (1 << 3)
#define TASK_SYNC_EVENT_BIT_ALL                                                                                        \
	(TASK_SYNC_EVENT_BIT_DATA_TX | TASK_SYNC_EVENT_BIT_IMG_OSD_DRAW | TASK_SYNC_EVENT_BIT_IMG_CHUNK_DRAW |               \
	 TASK_SYNC_EVENT_BIT_IMG_PROCESS)


typedef enum
{
	BUTTON_STATE_PRESSED = 0,
	BUTTON_STATE_RELEASED
} button_states_t;

// ----------------------------------------------------------------------
// Accessors functions

/**
 * @brief 
 * 
 * @param
 * 
 * @retval
 */ 
button_states_t xReadButton(gpio_num_t gpio_num);

/**
 * @brief Just a linear range converter
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
