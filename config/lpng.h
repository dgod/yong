#pragma once

typedef struct{
	uint32_t img_x;
	uint32_t img_y;
	uint8_t *out;
}L_PNG;

L_PNG *l_png_load(const char *file,...);
void l_png_free(L_PNG *);
