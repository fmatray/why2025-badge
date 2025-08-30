#include "lcd.h"
#include <lvgl.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"

/*Using SPI2 in the example*/
#define LCD_HOST SPI2_HOST

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL 1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_SCLK 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_MISO 6
#define PIN_NUM_LCD_DC 9
#define PIN_NUM_LCD_RST 8
#define PIN_NUM_LCD_CS 16
#define PIN_NUM_BK_LIGHT -1
#define PIN_NUM_TOUCH_CS -1

/*The pixel number in horizontal and vertical*/
#define LCD_H_RES 320
#define LCD_V_RES 240

/*Bit number used to represent command and parameter*/
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

void lcd_init() {

}

void lcd_free() {}