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
#define UART_SYNC_RX_TX_PIN (GPIO_NUM_14)

// ----------------------------------------------------------------------

#endif /* _PINS_DEFINITIONS_H */