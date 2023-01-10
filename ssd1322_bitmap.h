#pragma once

#include "ssd1322.h"

typedef struct {
	const unsigned char *map;
	const unsigned char w;
	const unsigned char h;
} ssd1322_bitmap_t;

void ssd1322_draw_bitmap(ssd1322_t *disp, int x, int y, const void *bmp);
