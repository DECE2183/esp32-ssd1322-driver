#pragma once

#include <stdint.h>

#include <driver/spi_master.h>
#include <driver/gpio.h>

typedef struct {
    gpio_num_t reset;
    gpio_num_t dc;
    gpio_num_t cs;
} ssd1322_pinmap_t;

typedef struct {
    spi_host_device_t   spi_host;
    spi_device_handle_t spi_device;

    ssd1322_pinmap_t      pinmap;

    uint8_t* framebuffer;
    uint32_t framebuffer_size;
    uint16_t res_x, res_y;
    uint16_t rows_cnt, clmn_cnt;
} ssd1322_t;

typedef uint8_t ssd1322_grayscale_table_t[16];

ssd1322_t *ssd1322_init(spi_host_device_t spi_host, ssd1322_pinmap_t pinmap, uint16_t res_x, uint16_t res_y);
void ssd1322_deinit(ssd1322_t *disp);

void ssd1322_mode_normal(ssd1322_t *disp);
void ssd1322_mode_invert(ssd1322_t *disp);
void ssd1322_mode_on(ssd1322_t *disp);
void ssd1322_mode_off(ssd1322_t *disp);
void ssd1322_sleep(ssd1322_t *disp);
void ssd1322_wakeup(ssd1322_t *disp);

void ssd1322_set_default_grayscale_table(ssd1322_t *disp);
void ssd1322_set_grayscale_table(ssd1322_t *disp, const ssd1322_grayscale_table_t table);
void ssd1322_set_contrast(ssd1322_t *disp, uint8_t contrast);
void ssd1322_set_brightness(ssd1322_t *disp, uint8_t brightness);

void ssd1322_send_framebuffer(ssd1322_t *disp);
void ssd1322_fill(ssd1322_t *disp, uint8_t color);

void ssd1322_set_pixel(ssd1322_t *disp, int x, int y, int color);
void ssd1322_draw_hline(ssd1322_t *disp, int x1, int x2, int y, int color);
void ssd1322_draw_vline(ssd1322_t *disp, int y1, int y2, int x, int color);
void ssd1322_draw_rect(ssd1322_t *disp, int x, int y, int width, int height, int color);
void ssd1322_draw_rect_filled(ssd1322_t *disp, int x, int y, int width, int height, int color);
void ssd1322_draw_bitmap_4bit(ssd1322_t *disp, int x, int y, const void *bitmap, int x_size, int y_size);
void ssd1322_draw_bitmap_8bit(ssd1322_t *disp, int x, int y, const void *bitmap, int x_size, int y_size);