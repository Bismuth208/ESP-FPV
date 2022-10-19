#include "async_profiler.h"

#include "async_printf.h"

#include <esp_attr.h>
#include <esp_timer.h>

static volatile uint32_t local_profile_time[PROFILER_POINTS_MAX] = {0};

void IRAM_ATTR
profile_point(profile_point_t state, uint32_t point_id)
{
	if(state == profile_point_start)
	{
		local_profile_time[point_id] = esp_timer_get_time();
	}
	else
	{
		local_profile_time[point_id] = (esp_timer_get_time() - local_profile_time[point_id]);
		async_printf(async_print_type_u32, "profile: %lu ", point_id);
		async_printf(async_print_type_u32, "time: %lu us\n", local_profile_time[point_id]);
	}
}
