#include "ssd1322.h"
#include "ssd1322_regmap.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unistd.h>

#include <string.h>
#include <sys/time.h>

typedef struct {
    uint8_t start;
    uint8_t stop;
} ssd1322_window_t;

typedef struct {
    uint8_t divider : 4;
    uint8_t freq : 4;
} ssd1322_clock_t;

typedef struct {
    uint8_t vert_inc : 1;
    uint8_t clmn_remap : 1;
    uint8_t nibble_remap : 1;
    uint8_t : 1;
    uint8_t hor_mirror : 1;
    uint8_t odd_even_split : 1;
    uint8_t : 2;
    uint8_t reserved : 1;
    uint8_t : 3;
    uint8_t dual_line : 1;
    uint8_t : 3;
} ssd1322_remap_t;

// static const ssd1322_grayscale_table_t ssd1322_default_grayscale_table = {

// };

static void ssd1322_write(ssd1322_t *disp, ssd1322_regmap_t cmd, void *data, size_t data_size)
{
    static spi_transaction_t trs;
    
    trs.length = 8;
    trs.tx_buffer = &cmd;
    gpio_set_level(disp->pinmap.dc, 0);
    gpio_set_level(disp->pinmap.cs, 0);
    spi_device_polling_transmit(disp->spi_device, &trs);
    gpio_set_level(disp->pinmap.cs, 1);

    if (data == NULL || data_size == 0) return;

    trs.length = data_size * 8;
    trs.tx_buffer = data;
    gpio_set_level(disp->pinmap.dc, 1);
    gpio_set_level(disp->pinmap.cs, 0);
    spi_device_polling_transmit(disp->spi_device, &trs);
    gpio_set_level(disp->pinmap.cs, 1);
}

static void ssd1322_hw_reset(ssd1322_t *disp)
{
    gpio_set_level(disp->pinmap.reset, 1);
    usleep(10);
    gpio_set_level(disp->pinmap.reset, 0);
    usleep(100);
    gpio_set_level(disp->pinmap.reset, 1);
    vTaskDelay(1);
}

static void ssd1322_set_command_lock(ssd1322_t *disp, bool lock)
{
    uint8_t b = lock ? 0x16 : 0x12;
    ssd1322_write(disp, SSD1322_SET_COMMAND_LOCK, &b, 1);
}

static void ssd1322_set_clmn_window(ssd1322_t *disp, uint16_t x_start, uint16_t x_stop)
{
    ssd1322_window_t wnd = {
        .start = 28 + (x_start & 0x7F),
        .stop = 28 + (x_stop & 0x7F),
    };
    ssd1322_write(disp, SSD1322_SET_CLMN_ADDR, &wnd, sizeof(ssd1322_window_t));
}

static void ssd1322_set_row_window(ssd1322_t *disp, uint16_t y_start, uint16_t y_stop)
{
    ssd1322_window_t wnd = {
        .start = y_start & 0x7F,
        .stop = y_stop & 0x7F,
    };
    ssd1322_write(disp, SSD1322_SET_ROW_ADDR, &wnd, sizeof(ssd1322_window_t));
}

static void ssd1322_set_offset(ssd1322_t *disp, uint8_t offset)
{
    ssd1322_write(disp, SSD1322_SET_OFFSET, &offset, sizeof(offset));
}

static void ssd1322_set_start_line(ssd1322_t *disp, uint8_t line)
{
    ssd1322_write(disp, SSD1322_SET_START_LINE, &line, sizeof(line));
}

static void ssd1322_set_remap(ssd1322_t *disp)
{
    ssd1322_remap_t remap = {
        .vert_inc = 0,
        .clmn_remap = 0,
        .nibble_remap = 1,
        .hor_mirror = 1,
        .odd_even_split = 0,
        .reserved = 1,
        .dual_line = 1,
    };
    ssd1322_write(disp, SSD1322_REMAP_DUAL_COM_LINE_MODE, &remap, 2);
}

static void ssd1322_setup_ram(ssd1322_t *disp)
{
    uint8_t mux = disp->clmn_cnt - 1;
    ssd1322_write(disp, SSD1322_SET_MUX_RATIO, &mux, sizeof(mux));
    ssd1322_write(disp, SSD1322_EXIT_PARTIAL_MODE, NULL, 0);

    ssd1322_set_clmn_window(disp, 0, disp->clmn_cnt - 1);
    ssd1322_set_row_window(disp, 0, disp->rows_cnt - 1);

    ssd1322_set_offset(disp, 0);
    ssd1322_set_start_line(disp, 0);

    ssd1322_set_remap(disp);
}

static void ssd1322_setup_clock(ssd1322_t *disp)
{
    ssd1322_clock_t clock = {
        .divider = 2,
        .freq = 15,
    };
    ssd1322_write(disp, SSD1322_SET_CLOCK_DIV, &clock, sizeof(ssd1322_clock_t));
}

static void ssd1322_setup_volatage(ssd1322_t *disp)
{
    uint8_t b;

    b = 1;
    ssd1322_write(disp, SSD1322_FUNCTION_SELECT, &b, sizeof(b));
    b = 0x1F;
    ssd1322_write(disp, SSD1322_SET_PRECHARGE_VOLTAGE, &b, sizeof(b));
    b = 0x08;
    ssd1322_write(disp, SSD1322_SET_2ND_PRECHARGE_PERIOD, &b, sizeof(b));
    b = 0x07;
    ssd1322_write(disp, SSD1322_SET_VCOMH, &b, sizeof(b));
}

static void ssd1322_setup_enhancement(ssd1322_t *disp)
{
    uint8_t enh_a[2] = {
        0b10100000,
        0b11111101,
    };
    ssd1322_write(disp, SSD1322_DISPLAY_ENHANCEMENT_A, enh_a, sizeof(enh_a));

    uint8_t enh_b[2] = {
        0b10000010,
        0b00100000,
    };
    ssd1322_write(disp, SSD1322_DISPLAY_ENHANCEMENT_B, enh_b, sizeof(enh_b));
}

static void ssd1322_init_sequence(ssd1322_t *disp)
{
    ssd1322_set_command_lock(disp, false);
    ssd1322_sleep(disp);
    ssd1322_setup_clock(disp);
    ssd1322_setup_volatage(disp);
    ssd1322_setup_enhancement(disp);
    ssd1322_setup_ram(disp);
    ssd1322_wakeup(disp);
}

ssd1322_t *ssd1322_init(spi_host_device_t spi_host, ssd1322_pinmap_t pinmap, uint16_t res_x, uint16_t res_y)
{
    ssd1322_t *disp = malloc(sizeof(ssd1322_t));
    if (disp == NULL)
        return NULL;

    disp->spi_host = spi_host;
    disp->pinmap = pinmap;
    disp->res_x = res_x;
    disp->res_y = res_y;
    disp->clmn_cnt = res_y;
    disp->rows_cnt = (res_x + 1) / 2;

    disp->framebuffer_size = disp->clmn_cnt * disp->rows_cnt;
    disp->framebuffer = malloc(disp->framebuffer_size);
    if (disp->framebuffer == NULL)
        goto error;

    gpio_set_direction(disp->pinmap.reset, GPIO_MODE_OUTPUT);
    gpio_set_direction(disp->pinmap.dc, GPIO_MODE_OUTPUT);
    gpio_set_direction(disp->pinmap.cs, GPIO_MODE_OUTPUT);
    gpio_set_level(disp->pinmap.reset, 0);
    gpio_set_level(disp->pinmap.dc, 1);
    gpio_set_level(disp->pinmap.cs, 1);

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_16M,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    esp_err_t err = spi_bus_add_device(spi_host, &devcfg, &disp->spi_device);
    if (err != ESP_OK)
        goto error;
    
    ssd1322_hw_reset(disp);
    ssd1322_init_sequence(disp);
    ssd1322_set_default_grayscale_table(disp);
    ssd1322_set_contrast(disp, 128);
    ssd1322_set_brightness(disp, 15);
    ssd1322_fill(disp, 0);
    ssd1322_send_framebuffer(disp);
    ssd1322_mode_normal(disp);

    return disp;

error:
    free(disp->framebuffer);
    free(disp);
    return NULL;
}

void ssd1322_deinit(ssd1322_t *disp)
{
    ssd1322_sleep(disp);
    spi_bus_remove_device(disp->spi_device);
    free(disp->framebuffer);
    free(disp);
}

void ssd1322_mode_normal(ssd1322_t *disp)
{
    ssd1322_write(disp, SSD1322_DISPLAY_MODE_NORMAL, NULL, 0);
}

void ssd1322_mode_invert(ssd1322_t *disp)
{
    ssd1322_write(disp, SSD1322_DISPLAY_MODE_INVERSE, NULL, 0);
}

void ssd1322_mode_on(ssd1322_t *disp)
{
    ssd1322_write(disp, SSD1322_DISPLAY_MODE_ON, NULL, 0);
}

void ssd1322_mode_off(ssd1322_t *disp)
{
    ssd1322_write(disp, SSD1322_DISPLAY_MODE_OFF, NULL, 0);
}

void ssd1322_sleep(ssd1322_t *disp)
{
    ssd1322_write(disp, SSD1322_SLEEP_MODE_ON, NULL, 0);
}

void ssd1322_wakeup(ssd1322_t *disp)
{
    ssd1322_write(disp, SSD1322_SLEEP_MODE_OFF, NULL, 0);
}

void ssd1322_set_default_grayscale_table(ssd1322_t *disp)
{
    ssd1322_write(disp, SSD1322_SELECT_DEFAULT_GRAYSCALE_TABLE, NULL, 0);
}

void ssd1322_set_grayscale_table(ssd1322_t *disp, const ssd1322_grayscale_table_t table)
{
    ssd1322_write(disp, SSD1322_ENABLE_GRAYSCALE_TABLE, NULL, 0);
    ssd1322_write(disp, SSD1322_SET_GRAYSCALE_TABLE, &table, sizeof(ssd1322_grayscale_table_t));
}

void ssd1322_set_contrast(ssd1322_t *disp, uint8_t contrast)
{
    uint8_t c = contrast;
    ssd1322_write(disp, SSD1322_SET_CONTRAST_CURRENT, &c, sizeof(c));
}

void ssd1322_set_brightness(ssd1322_t *disp, uint8_t brightness)
{
    uint8_t b = brightness & 0x0F;
    ssd1322_write(disp, SSD1322_MASTER_CONTRAST_CONTROL, &b, sizeof(b));
}

void ssd1322_send_framebuffer(ssd1322_t *disp)
{
    ssd1322_set_clmn_window(disp, 0, disp->clmn_cnt - 1);
    ssd1322_set_row_window(disp, 0, disp->rows_cnt - 1);
    ssd1322_write(disp, SSD1322_WRITE_RAM_CMD, disp->framebuffer, disp->framebuffer_size);
}

void ssd1322_fill(ssd1322_t *disp, uint8_t color)
{
    uint8_t pixels = (color << 4) | (color & 0x0F);
    memset(disp->framebuffer, pixels, disp->framebuffer_size);
}

void ssd1322_set_pixel(ssd1322_t *disp, int x, int y, int color)
{
    int idx = (x >> 1) + y * disp->rows_cnt;
    int offset = (!(x % 2)) << 2;

    disp->framebuffer[idx] &= ~(0x0F << offset);
    disp->framebuffer[idx] |= (color & 0x0F) << offset;
}

void ssd1322_draw_hline(ssd1322_t *disp, int x1, int x2, int y, int color)
{
    for (int x = x1; x < x2; ++x)
    {
        ssd1322_set_pixel(disp, x, y, color);
    }
}

void ssd1322_draw_vline(ssd1322_t *disp, int y1, int y2, int x, int color)
{
    for (int y = y1; y < y2; ++y)
    {
        ssd1322_set_pixel(disp, x, y, color);
    }
}

void ssd1322_draw_bitmap_4bit(ssd1322_t *disp, int x, int y, const void *bitmap, int x_size, int y_size)
{
    uint16_t bitmap_pos = 0;
    uint16_t processed_pixels = 0;
    uint8_t pixel_parity = 0;
    uint8_t *bmp = (uint8_t *)bitmap;

    for (uint16_t i = y; i < y + y_size; i++)
    {
        for (uint16_t j = x; j < x + x_size; j++)
        {
            pixel_parity = processed_pixels % 2;

            if (pixel_parity == 0)
            {
                ssd1322_set_pixel(disp, j, i, bmp[bitmap_pos] >> 4);
                ++processed_pixels;
            }
            else
            {
                ssd1322_set_pixel(disp, j, i, bmp[bitmap_pos]);
                ++processed_pixels;
                ++bitmap_pos;
            }
        }
    }
}

void ssd1322_draw_bitmap_8bit(ssd1322_t *disp, int x, int y, const void *bitmap, int x_size, int y_size)
{
    uint16_t bitmap_pos = 0;
    uint8_t *bmp = (uint8_t *)bitmap;

    for (uint16_t i = y; i < y + y_size; i++)
    {
        for (uint16_t j = x; j < x + x_size; j++)
        {
            ssd1322_set_pixel(disp, j, i, bmp[bitmap_pos] >> 4);
            ++bitmap_pos;
        }
    }
}
