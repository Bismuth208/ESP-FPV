#include "button_poller.h"

#include "data_common.h"
#include "memory_model/memory_model.h"
#include "pins_definitions.h"

#include <debug_tools_esp.h>

//
#include <sdkconfig.h>
//
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>
//
#include <esp_attr.h>
#include <esp_timer.h>
//
#include <assert.h>
#include <stdint.h>
#include <string.h>


static void init_gpio(void);


button_states_t
xReadButton(gpio_num_t gpio_num)
{
	return (button_states_t)gpio_get_level(gpio_num);
}


#if 0

void button_released(uint32_t ulButton)
{

}

void button_pressed(uint32_t ulButton)
{
  
}


/**
 * @brief Scan buttons states with a software filtering
 */ 
void poll_buttons(void)
{
  for(size_t i=0; i < ; i++) {

    buttons[i].xCurTrigState = xReadButton(buttons[i].xBtn);

    if (buttons[i].xPrevTrigState != buttons[i].xCurTrigState) {

      buttons[i].xPrevTrigState = buttons[i].xCurTrigState;
      buttons[i].xStarted = pdTRUE;
      buttons[i].ulTimeStart = get_time();

    }


    if (buttons[i].xStarted == pdTRUE) {

      if ((get_time() - buttons[i].ulTimeStart  ) >= BUTTON_POLLER_STABLE_EVENT_TIMEOUT) {

        buttons[i].xStarted = pdFALSE;

        if (buttons[i].xLastPinState != buttons[i].xCurTrigState) {
          
          buttons[i].xLastPinState == buttons[i].xCurTrigState;
          // buttons[i].xBtnIsPressed = pdTRUE;

          if (buttons[i].xLowState == buttons[i].xLastPinState) {
            button_pressed(buttons[i].xBtn);
          } else {
            button_released(buttons[i].xBtn);
          }
        } 
        }
    }
  }
}
#endif


static void
init_gpio(void)
{
	gpio_set_direction(BUTTON_1, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BUTTON_1, GPIO_PULLUP_ONLY);
}


void
init_button_poller(void)
{
	init_gpio();
}