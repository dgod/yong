#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <cairo.h>
#include <gio/gio.h>
#include <wayland-client.h>

typedef struct w_window *w_win_t;

enum{
	W_NONE,
	W_CLOSE,
	W_CONFIG,
	W_DESTROY,
	W_DRAW,
	W_MOUSE_MOVE,
	W_MOUSE_DOWN,
	W_MOUSE_UP,
	W_MOUSE_ENTER,
	W_MOUSE_LEAVE,
	W_SCROLL,
	W_KEY_DOWN,
	W_KEY_UP,
	W_EVENT_MAX,
};

enum{
	W_BUTTON_LEFT,
	W_BUTTON_RIGHT,
	W_BUTTON_MIDDLE,
};

enum{
	W_CURSOR_DEFAULT=1,
	W_CURSOR_POINTER=4,
	W_CURSOR_MOVE=13,
};

enum{
	W_EFFECT_NONE=0,
	W_EFFECT_BLUR=1,
};

typedef struct{
	int type;
	union{
		struct{
			int button;
			int x,y;
		}mouse;
		struct{
			int button;
			int x,y;
			double dx,dy;
		}scroll;
		struct{
			cairo_t *cr;
			double origin_x;
			double origin_y;
		}draw;
		struct{
			uint32_t key;
			uint32_t state;
			uint32_t serial;
		}key;
	};
}W_EVENT;

typedef struct{
	void *(*set_role)(struct wl_surface *);
	void (*clr_role)(void *);
	int (*set_size)(void *,int,int);
	int (*configure)(void);
	int (*get_pos)(int *,int *);
}W_INPUT_HOOK;

typedef int (*w_cb_t)(w_win_t win,const W_EVENT *e,void *data);

typedef struct{
	const char *(*translate)(const char *);
	W_INPUT_HOOK *input_hook;
	int *workarea;
}W_OPTIONS;

int w_init(const char *id);
int w_set_options(const W_OPTIONS *options);
int w_loop(void);
int w_quit(void);
int w_toast(const char *s);
int w_get_n_outputs(void);
extern const char *(*w_translate)(const char *s);

int w_win_get_dpi(w_win_t win);
w_win_t w_win_create(const char *title, const char *role,void *data);
void *w_win_get_data(w_win_t win);
int w_win_set_title(w_win_t win,const char *title);
int w_win_destroy(w_win_t win);
double w_win_get_scale(w_win_t win);
int w_win_move(w_win_t win,int x,int y);
int w_win_get_pos(w_win_t win,int *x,int *y);
int w_win_resize(w_win_t win,int w,int h);
int w_win_resize_internal(w_win_t win,int w,int h);
int w_win_get_size(w_win_t win,int *w,int *h);
int w_win_redraw(w_win_t win);
int w_win_connect(w_win_t win,int event,w_cb_t cb,void *data);
int w_win_disconnect(w_win_t win,int event);
int w_win_show(w_win_t win);
int w_win_hide(w_win_t win);
int w_win_set_input_region(w_win_t win,cairo_region_t *region);
int w_win_popup_menu(w_win_t win,GMenu *menu,GSimpleActionGroup *actions,int flags);
int w_win_set_capture(w_win_t win,bool capture);
int w_win_set_cursor(w_win_t win,int cursor);
int w_win_bell(w_win_t win);
int w_win_tran(w_win_t win,int tran);
char *w_clipboard_get_text(void);
int w_clipboard_set_text(const char *text);
int w_clipboard_set_mime(const char *mime,const void *data,int size);
int w_win_alert(w_win_t win,const char *title,const char *text);
int w_win_preferred_size(w_win_t win,int *w,int *h);
int w_win_effect(w_win_t win,int effect);

int w_wayland_set_serial(uint32_t serial);
void *w_wayland_get_surface(w_win_t win);
void *w_wayland_get_interface(const char *interface);
void *w_wayland_get_display(void);
bool w_wayland_has_interface(const char *interface, uint32_t version, uint32_t *name);

// Output info structure (read-only for users)
typedef struct wl_screen_info {
	struct wl_screen_info *next;
	uint32_t name;
	void *output;  			// Internal: Wayland output object
	int32_t x, y;           // Position in compositor space
	int32_t width, height;  // Resolution
	int32_t scale;          // Scale factor
	bool done;              // Info ready
} w_output_t;

// Screen/Output APIs
w_output_t *w_win_get_output(w_win_t win);
int w_win_get_workarea(w_win_t win,int *x,int *y,int *w,int *h);
int w_win_get_output_size(w_win_t win,int *w,int *h);

typedef struct{
	int (*init)(const char *id);
	int (*set_options)(const W_OPTIONS *options);
	int (*loop)(void);
	int (*quit)(void);
	int (*toast)(const char *s);
	int (*get_n_outputs)(void);
	int (*win_get_dpi)(w_win_t win);
	double (*win_get_scale)(w_win_t win);
	w_win_t (*win_create)(const char *title, const char *role,void *data);
	void *(*win_get_data)(w_win_t win);
	int (*win_destroy)(w_win_t win);
	int (*win_set_title)(w_win_t win,const char *title);
	int (*win_move)(w_win_t win,int x,int y);
	int (*win_get_pos)(w_win_t win,int *x,int *y);
	int (*win_resize)(w_win_t win,int w,int h);
	int (*win_resize_internal)(w_win_t win,int w,int h);
	int (*win_get_size)(w_win_t win,int *w,int *h);
	int (*win_redraw)(w_win_t win);
	int (*win_connect)(w_win_t win,int event,w_cb_t cb,void *data);
	int (*win_disconnect)(w_win_t win,int event);
	int (*win_show)(w_win_t win);
	int (*win_hide)(w_win_t win);
	int (*win_tran)(w_win_t win,int tran);
	int (*win_set_input_region)(w_win_t win,cairo_region_t *region);
	int (*win_popup_menu)(w_win_t win,GMenu *menu,GSimpleActionGroup *actions,int flags);
	int (*win_set_capture)(w_win_t win,bool capture);
	int (*win_set_cursor)(w_win_t win,int cursor);
	int (*win_bell)(w_win_t win);
	int (*win_alert)(w_win_t win,const char *title,const char *text);
	int (*win_center)(w_win_t win);
	int (*win_preferred_size)(w_win_t win,int *w,int *h);
	int (*win_effect)(w_win_t win,int effect);
	char *(*clipboard_get_text)(void);
	int (*clipboard_set_text)(const char *text);
	int (*clipboard_set_mime)(const char *mime,const void *data,int size);
	int (*wayland_set_serial)(uint32_t serial);
	void *(*wayland_get_surface)(w_win_t win);
	void *(*wayland_get_interface)(const char *interface);
	void *(*wayland_get_display)(void);
	bool (*wayland_has_interface)(const char *interface, uint32_t version, uint32_t *name);
	w_output_t *(*get_output)(w_win_t win);
	int (*get_workarea)(w_win_t win, int *x, int *y, int *w, int *h);
	int (*get_output_size)(w_win_t win,int *w, int *h);
}W_UI;

int w_win_center(w_win_t win);

#ifndef WUI_IMPL
extern const W_UI *wui;
#endif
