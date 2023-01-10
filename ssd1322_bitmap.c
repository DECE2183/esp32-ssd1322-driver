#include "ssd1322_bitmap.h"

inline void ssd1322_draw_bitmap(ssd1322_t *disp, int x, int y, const void *bmp)
{
    ssd1322_bitmap_t *b = (ssd1322_bitmap_t *)bmp;
    ssd1322_draw_bitmap_4bit(disp, x, y, b->map, b->w, b->h);
}