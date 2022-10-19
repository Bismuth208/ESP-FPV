/**
 * @file debug_tools_conf.h
 * 
 * @brief This file provide control of various debug outputs
 * 
 * All of the prints are done asynchronously to the output stream.
 * To enable specific printouts just uncomment required defines below.
 */

#ifndef _DEBUG_TOOLS_CONF_H
#define _DEBUG_TOOLS_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

// Enable/disable whole debug output
// #define ENABLE_DEBUG_TOOLS


#ifdef ENABLE_DEBUG_TOOLS
#include <debug_tools_esp.h>

// ---------------------------------
// Uncomment defines below to enable logs:
//  - with general debug prints
//
// Naming rule:
//  - #define YOUR_NAME##_DBG_PRINTOUT

// Add defines here

// ---------------------------------
// Uncomment defines below to enable logs:
//  - with profile debug prints
// Note: only one at time should be uncommented!  
//
// Profiler point Id's
//
// NOTE: Point Id Should not be higher than @ref PROFILER_POINTS_MAX
//
// Naming rule:
//  - #define YOUR_NAME##_DBG_PROFILER
//  - #define YOUR_NAME##_DBG_PROFILER_POINT_ID (someMagicNumberId)

// Add defines here

#ifdef __cplusplus
}
#endif
#endif // ENABLE_DEBUG_TOOLS
#endif // _DEBUG_TOOLS_CONF_H
