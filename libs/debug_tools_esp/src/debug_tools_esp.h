/**
 * @file debug_tools_esp.h
 * @brief This file provide includes of various debug outputs
 *        Common include file for hefty things to make life simpler ;)
 *
 *        All of the prints are done asynchronously to the output stream.
 *        To enable specific printouts just check Kconfig:
 *          - Component config -> Debug assistant configuration ->
 *          - Component config -> Async printf configuration ->
 */
#ifndef _DEBUG_TOOLS_ESP_H
#define _DEBUG_TOOLS_ESP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "async_printf.h"
#include "async_printf_conf.h"
#include "async_profiler.h"
#include "debug_assist.h"

#ifdef __cplusplus
}
#endif

#endif // _DEBUG_TOOLS_ESP_H