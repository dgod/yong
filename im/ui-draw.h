#pragma once

#include <stdint.h>
#include <ui.h>

#ifdef _WIN32

#include <windows.h>

typedef struct{
	int x,y;
	int width,height;
	union{
		uint8_t *bits;
		uint32_t *pixels;
	};
	UI_COLOR pen;
	double line_width;
	HWND win;
	HDC dc;
	bool text;
}DRAW_CONTEXT1;

typedef HBITMAP UI_IMAGE;
typedef HICON UI_ICON;
typedef struct{
	HFONT gdi;
	void *dw;
	int lineGap;
}*UI_FONT;

typedef HDC UI_DC;
typedef HWND UI_WINDOW;
typedef HRGN UI_REGION;

#else

#include <gtk/gtk.h>
#include <cairo.h>

#define UI_IMAGE_USE_GDK_PIXBUF		0

#if UI_IMAGE_USE_GDK_PIXBUF
typedef GdkPixbuf *UI_IMAGE;
#else
typedef cairo_surface_t *UI_IMAGE;
#endif
typedef GtkWidget *UI_WINDOW;
typedef cairo_t *UI_DC;
typedef GdkPixbuf *UI_ICON;
typedef cairo_region_t *UI_REGION;

typedef struct{
	PangoLayout *pango;
	uint8_t extraSpaceAbove;
	uint8_t extraSpaceBelow;
}*UI_FONT;

typedef struct{
	int x,y;
	GtkWidget *win;
	cairo_t *dc;
	double scale;
}DRAW_CONTEXT1;

#endif

#define	IMAGE_SKIN		1
#define IMAGE_SKIN_DEF	2
#define IMAGE_ROOT		4
#define IMAGE_ALL		0xff

void ui_draw_init(void);
void ui_draw_begin(DRAW_CONTEXT1 *ctx,void *win,void *dc);
void ui_draw_end(DRAW_CONTEXT1 *ctx);
void ui_draw_line(DRAW_CONTEXT1 *ctx,int x0,int y0,int x1,int y1,UI_COLOR color,double line_width);
void ui_draw_rect(DRAW_CONTEXT1 *ctx,int x,int y,int w,int h,UI_COLOR color,double line_width);
void ui_fill_rect(DRAW_CONTEXT1 *ctx,int x,int y,int w,int h,UI_COLOR color);
void ui_draw_round_rect(DRAW_CONTEXT1 *ctx,int x,int y,int w,int h,int r,UI_COLOR stroke,UI_COLOR fill,double line_width);
void ui_draw_text_begin(DRAW_CONTEXT1 *ctx);
int ui_draw_text_end(DRAW_CONTEXT1 *ctx);
void ui_draw_text(DRAW_CONTEXT1 *ctx,UI_FONT font,int x,int y,const void *text,UI_COLOR color);
void ui_draw_image(DRAW_CONTEXT1 *ctx,UI_IMAGE image,int x,int y);
void ui_stretch_image(DRAW_CONTEXT1 *ctx,UI_IMAGE image,int x,int y,int w,int h);
void ui_draw_image_full(DRAW_CONTEXT1 *ctx,UI_IMAGE image,int dx,int dy,int dw,int dh,int x,int y,int w,int h);
UI_FONT ui_font_parse(void *win,const char *s,double scale);
void ui_font_free(UI_FONT font);
int ui_text_size(void *dc,UI_FONT font,const void*text,int *w,int *h);
bool ui_image_path(const char *file,char path[],int where);
UI_IMAGE ui_image_load(const char *file,int where);
void ui_image_free(UI_IMAGE img);
UI_IMAGE ui_image_load_at_size(const char *file,int width,int height,int where);
UI_IMAGE ui_image_load_scale(const char *file,double scale,int width,int height,int where);
UI_IMAGE ui_image_part(UI_IMAGE image,int x,int y,int w,int h);
int ui_image_size(UI_IMAGE img,int *w,int *h);
UI_ICON ui_icon_load(const char *file);
void ui_icon_free(UI_ICON icon);
void ui_image_draw(UI_DC dc,UI_IMAGE img,int x,int y);
void ui_image_draw_full(UI_DC hdc,UI_IMAGE bmp,
				int dst_x,int dst_y,int dst_w,int dst_h,
				int x,int y,int w,int h);
UI_REGION ui_image_region(UI_IMAGE img,double scale);

#ifndef _WIN32
GdkPixbuf *ui_image_load_pixbuf_at_size(const char *file,int width,int height,int where);
#endif
