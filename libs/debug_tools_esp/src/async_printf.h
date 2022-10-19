/**
 * @file     async_printf.h
 * 
 * @brief    Provide light-weight async printf
 */

#ifndef _ASYNC_PRINTF_H
#define _ASYNC_PRINTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration

/// Describes what type of item should be printed and formatted right
typedef enum
{
	/// Simple text string with no use of any value
	async_print_type_str = 0,
	/// Print string with formatted uint32_t value
	async_print_type_u32 = 1,
} async_print_type_t;

typedef struct
{
	async_print_type_t type;
	const char* msg;
	uint32_t value;
} async_print_item_t;

// ----------------------------------------------------------------------
// Accessors functions

/**
 * @brief Add item for async printf later in @ref ''async_printf_sync'' function
 *
 * @param item_type
 * @param new_msg
 * @param new_value
 *
 * @attention this is async printf, it's not possible to use local buffers for new_msg!
 */
void async_printf(async_print_type_t item_type, const char* new_msg, uint32_t new_value);

/**
 * @brief Check if there is any items in buffer and print'em
 *
 * @attention to prevent high CPU loads, prints are as one at loop cycle
 */
void async_printf_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* _ASYNC_PRINTF_H */