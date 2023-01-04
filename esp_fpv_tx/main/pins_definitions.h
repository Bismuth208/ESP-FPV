/**
 * @file pins_definition.h
 * @brief ESP32-S3 definitions for GPIOs
 */

#ifndef _PINS_DEFINITIONS_H
#define _PINS_DEFINITIONS_H

#include <driver/gpio.h>

// ----------------------------------------------------------------------
/// Single Pin is used for the communication
/// @attention In normal operation mode this pin MUST be connected to the GND
/// @note 3bit SD_MMC will be not possible! Only 1bit mode.
#define UART_SYNC_RX_TX_PIN (GPIO_NUM_12)


// ----------------------------------------------------------------------
// On ESP-CAM from Ai-Thinker
#define CAMERA_LED_PIN (GPIO_NUM_4)

// ----------------------------------------------------------------------

#endif /* _PINS_DEFINITIONS_H */