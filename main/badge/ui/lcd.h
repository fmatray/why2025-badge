#ifndef _LCD_H
#define _LCD_H
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB888))

void lcd_init();
void lcd_free();

#endif