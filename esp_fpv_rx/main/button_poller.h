#ifndef _BUTTON_POLLER_H
#define _BUTTON_POLLER_H

#ifdef __cplusplus
extern "C" {
#endif

//
#include <driver/gpio.h>
//
#include <stdint.h>


typedef enum
{
	BUTTON_STATE_PRESSED = 0,
	BUTTON_STATE_RELEASED
} button_states_t;


// Time in microseconds
#define BUTTON_POLLER_STABLE_EVENT_TIMEOUT (20)

#define BUTTON_POLLER_MAX_BUTTONS (3)


/**
 * @brief 
 * 
 * @param
 * 
 * @retval
 */ 
button_states_t xReadButton(gpio_num_t gpio_num);


void init_button_poller(void);


#ifdef __cplusplus
}
#endif
#endif /* _BUTTON_POLLER_H */