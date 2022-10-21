/**
 * @file pins_definition.h
 * @brief ESP32-S3 definitions for GPIOs
 */

#ifndef _PINS_DEFINITIONS_H
#define _PINS_DEFINITIONS_H

#include <driver/gpio.h>

// ----------------------------------------------------------------------
// SPI Pins for ili9341 TFT Screen
#define TFT_DC_PIN  (GPIO_NUM_8)
#define TFT_CS_PIN  (GPIO_NUM_10)
#define TFT_RST_PIN (GPIO_NUM_4)
#define TFT_LED_PIN (GPIO_NUM_2)

// Hardware SPI2_HOST
//#define TFT_MOSI_PIN          (GPIO_NUM_11)
//#define TFT_CLK_PIN           (GPIO_NUM_12)
//#define TFT_MISO_PIN          (-1)

// ----------------------------------------------------------------------
/// I2C Pins for SSD1306 Display
#define OLED_SDA_PIN (GPIO_NUM_39)
#define OLED_SCL_PIN (GPIO_NUM_40)

// ----------------------------------------------------------------------
/// Pin for user control (But right is now used only to launch AirScanner)
#define BUTTON_1 (GPIO_NUM_47)

// ----------------------------------------------------------------------
/// Single Pin is used for the communication
/// @attention In normal operation mode this pin MUST be connected to the GND
#define UART_SYNC_RX_TX_PIN (GPIO_NUM_19)

// ----------------------------------------------------------------------

#endif /* _PINS_DEFINITIONS_H */
