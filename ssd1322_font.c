#include "ssd1322_font.h"

inline void ssd1322_draw_char(ssd1322_t *disp, int x, int y, char ch, const void *font)
{
    ssd1322_font_t *f = (ssd1322_font_t*) font;
    if (ch < f->first_index || ch > f->last_index) return;
    ssd1322_draw_bitmap_4bit(disp, x, y, f->chars[(unsigned char)ch].bitmap, f->chars[(unsigned char)ch].w, f->chars[(unsigned char)ch].h);
}

void ssd1322_draw_string(ssd1322_t *disp, int x, int y, const char *str, const void *font)
{
    ssd1322_font_t *f = (ssd1322_font_t *)font;

    while (*str != '\0' && *str != '\n' && x < disp->res_x)
    {
        ssd1322_draw_char(disp, x, y, *str, font);
        x += f->chars[(unsigned char)*str].w;
        ++str;
    }
}