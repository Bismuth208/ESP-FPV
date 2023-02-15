#include "async_printf.h"
#include "async_printf_conf.h"

#include <esp_attr.h>
#include <stdio.h>
#include <string.h>

// ----------------------------------------------------------------------
// Definitions, type & enum declaration

/// Maximum amount of bytes in formatted string to output via UART 
#define ASYNC_PRINTF_MAX_OUTPUT_BUF_LEN (2048)

/// Create bit mask, which will be used as "pass" range and filter overflow
#define ASYNC_PRINTF_BUFFER_MASK (ASYNC_PRINTF_MAX_ITEMS - 1)

typedef struct
{
	uint32_t write_index;
	uint32_t read_index;
	async_print_item_t items[ASYNC_PRINTF_MAX_ITEMS];
} async_printf_ring_buffer_t;

// ----------------------------------------------------------------------
// Variables

static volatile async_printf_ring_buffer_t async_printf_buffer = {0};

uint8_t async_printf_fmt_buf[ASYNC_PRINTF_MAX_OUTPUT_BUF_LEN] = {0};

// ----------------------------------------------------------------------
// Static functions declaration

/**
 * @brief Print items from the circular buffer as one item per call of function.
 */
static void async_printf_print(void);

// ----------------------------------------------------------------------
// Static functions

static void
async_printf_print(void)
{
	volatile async_print_item_t* local_tail = &async_printf_buffer.items[async_printf_buffer.read_index];
	async_printf_buffer.read_index = (async_printf_buffer.read_index + 1) & ASYNC_PRINTF_BUFFER_MASK;

	switch(local_tail->type)
	{
	case async_print_type_str: {
		snprintf((char*)&async_printf_fmt_buf, ASYNC_PRINTF_MAX_OUTPUT_BUF_LEN, (const char*)local_tail->msg);
		puts((const char*)&async_printf_fmt_buf);
		break;
	}

	case async_print_type_u32: {
		snprintf((char*)&async_printf_fmt_buf, ASYNC_PRINTF_MAX_OUTPUT_BUF_LEN, (const char*)local_tail->msg, (uint32_t)local_tail->value);
		puts((const char*)&async_printf_fmt_buf);
		break;
	}

	default:
		break;
	}
}

// ----------------------------------------------------------------------
// Accessors functions

void IRAM_ATTR
async_printf(async_print_type_t item_type, const char* new_msg, uint32_t new_value)
{
	// Get current head position and move it as fast as possible
	volatile async_print_item_t* local_head = &async_printf_buffer.items[async_printf_buffer.write_index];
	async_printf_buffer.write_index = (async_printf_buffer.write_index + 1) & ASYNC_PRINTF_BUFFER_MASK;

	local_head->type = item_type;
	local_head->msg = new_msg;
	local_head->value = new_value;
}

void IRAM_ATTR
async_printf_sync(void)
{
	if(async_printf_buffer.read_index != async_printf_buffer.write_index)
	{
		async_printf_print();
	}
}