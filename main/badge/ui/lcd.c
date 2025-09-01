/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <lvgl.h>
#include <stdio.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

// Using SPI2 in the example
#define LCD_HOST SPI2_HOST

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define PIN_NUM_SCLK 6
#define PIN_NUM_MOSI 7
#define PIN_NUM_MISO 2
#define PIN_NUM_LCD_DC 4
#define PIN_NUM_LCD_RST 3
#define PIN_NUM_LCD_CS 10

// The pixel number in horizontal and vertical
#define LCD_H_RES 320
#define LCD_V_RES 240
// Bit number used to represent command and parameter
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define DRAW_BUF_SIZE (LCD_H_RES * LCD_V_RES / 10 * BYTES_PER_PIXEL)

lv_display_t *display;
uint8_t buf1[DRAW_BUF_SIZE];
// uint8_t buf2[DRAW_BUF_SIZE];

lv_display_t *display = NULL;

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx) {
  ESP_LOGI(__FILE__, "notify_lvgl_flush_ready");
  lv_display_t *disp = (lv_display_t *)user_ctx;
  lv_display_flush_ready(disp);
  return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map) {
  ESP_LOGI(__FILE__, "lvgl_flush_cb");
  assert(px_map != NULL);
  // lvgl_port_update_callback(disp);
  esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
  ESP_LOGI(__FILE__, "lvgl_flush_cb : 1");
  int offsetx1 = area->x1;
  int offsetx2 = area->x2;
  int offsety1 = area->y1;
  int offsety2 = area->y2;
  // because SPI LCD is big-endian, we need to swap the RGB bytes order
  ESP_LOGI(__FILE__, "lvgl_flush_cb : 2");
  lv_draw_sw_rgb565_swap(px_map,
                         (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
  ESP_LOGI(__FILE__, "lvgl_flush_cb : 3");
  // copy a buffer's content to a specific area of the display
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
      panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map));
  ESP_LOGI(__FILE__, "lvgl_flush_cb: DONE");
}

void lcd_init(void) {
  ESP_LOGI(__FILE__, "Initialize SPI bus");
  spi_bus_config_t buscfg = {
      .sclk_io_num = PIN_NUM_SCLK,
      .mosi_io_num = PIN_NUM_MOSI,
      .miso_io_num = PIN_NUM_MISO,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
  };
  ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t)LCD_HOST, &buscfg,
                                     SPI_DMA_CH_AUTO));

  ESP_LOGI(__FILE__, "Install panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .dc_gpio_num = PIN_NUM_LCD_DC,
      .cs_gpio_num = PIN_NUM_LCD_CS,
      .pclk_hz = LCD_PIXEL_CLOCK_HZ,
      .lcd_cmd_bits = LCD_CMD_BITS,
      .lcd_param_bits = LCD_PARAM_BITS,
      .spi_mode = 0,
      .trans_queue_depth = 10,
  };
  // Attach the LCD to the SPI bus
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                           &io_config, &io_handle));

  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = PIN_NUM_LCD_RST,
      .color_space = ESP_LCD_COLOR_SPACE_RGB,
      .bits_per_pixel = 16,
      .vendor_config = NULL,
  };
  ESP_LOGI(__FILE__, "Install ST7789 panel driver");
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
  // user can flush pre-defined pattern to the screen before we turn on the
  // screen or backlight
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  ESP_LOGI(__FILE__, "Initialize LVGL library");
  lv_init();

  ESP_LOGI(__FILE__, "LVGL create display");
  // create a lvgl display
  display = lv_display_create(LCD_H_RES, LCD_V_RES);

  // alloc draw buffers used by LVGL
  // it's recommended to choose the size of the draw buffer(s) to be at least
  // 1/10 screen sized

  // initialize LVGL draw buffers
  ESP_LOGI(__FILE__, "Initializating buffers");

  lv_color_format_t cf = lv_display_get_color_format(display);
  uint32_t w = lv_display_get_original_horizontal_resolution(display);
  uint32_t h = lv_display_get_original_vertical_resolution(display);
  uint32_t stride = lv_draw_buf_width_to_stride(w, cf);

  ESP_LOGI(__FILE__, "cf: %d  w: %d  h:%d stride: %d. buff_size:%d", cf, w, h,
           stride, sizeof(buf1));
  lv_display_set_buffers(display, buf1, NULL, sizeof(buf1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  // associate the mipi panel handle to the display
  lv_display_set_user_data(display, panel_handle);
  // set color depth
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  // set the callback which can copy the rendered image to an area of the
  // display
  lv_display_set_flush_cb(display, lvgl_flush_cb);

  ESP_LOGI(
      __FILE__,
      "Register io panel event callback for LVGL flush ready notification");
  const esp_lcd_panel_io_callbacks_t cbs = {
      .on_color_trans_done = notify_lvgl_flush_ready,
  };
  /* Register done callback */
  ESP_ERROR_CHECK(
      esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));
  ESP_LOGI(__FILE__, "LCD INIT DONE");
}

void lcd_free(){};