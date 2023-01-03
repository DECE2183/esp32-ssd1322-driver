#pragma once

#include "ssd1322.h"

typedef struct
{
    const unsigned char *bitmap;
    unsigned char w;
    unsigned char h;
} ssd1322_char_t;

typedef struct
{
    const ssd1322_char_t *chars;
    unsigned char first_index;
    unsigned char last_index;
} ssd1322_font_t;

void ssd1322_draw_char(ssd1322_t *disp, int x, int y, char ch, const void *font);
void ssd1322_draw_string(ssd1322_t *disp, int x, int y, const char *str, const void *font);