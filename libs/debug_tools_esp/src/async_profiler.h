/**
 * @file profiler.h
 *
 * @brief Module what allows to measure time execution of the code with a simple
 * way
 */

#ifndef _ASYNC_PROFILER_H
#define _ASYNC_PROFILER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/// Maximum amount of available independent points to use
#define PROFILER_POINTS_MAX (32u)

#ifdef ENABLE_DEBUG_TOOLS
#define PROFILE_POINT(name, type)                                              \
  if (name == 1) {                                                             \
    profile_point(type, name##_POINT_ID);                                      \
  }
#else
#define PROFILE_POINT(name, type)
#endif

typedef enum {
  profile_point_start = 0, ///! Reset time measurements and start new counter
  profile_point_end ///! Finish time measurements and add log item with results
} profile_point_t;

/**
 * @brief Measure code execution between multiple points
 *
 * @param state see @ref profile_point_t
 */
void profile_point(profile_point_t state, uint32_t point_id);

#ifdef __cplusplus
}
#endif

#endif // _ASYNC_PROFILER_H