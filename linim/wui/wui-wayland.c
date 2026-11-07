#include <glib.h>
#include <wayland-client.h>
#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <stdbool.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include "wui.h"
#include "wui-buffer.h"
#include "llib.h"
#include "ext-data-control-v1.h"
#include "xdg-shell.h"
#include "tablet-v2.h"
#include "wlr-layer-shell-unstable-v1.h"
#include "fractional-scale-v1.h"
#include "cursor-shape-v1.h"
#include "xdg-system-bell-v1.h"
#include "viewporter.h"
#include "alpha-modifier-v1.h"
#include "ext-background-effect-v1.h"
#include "xx-fractional-scale-v2.h"

#include "ext-data-control-v1.c"
#include "xdg-shell.c"
#include "tablet-v2.c"
#include "wlr-layer-shell-unstable-v1.c"
#include "fractional-scale-v1.c"
#include "cursor-shape-v1.c"
#include "xdg-system-bell-v1.c"
#include "viewporter.c"
#include "alpha-modifier-v1.c"
#include "ext-background-effect-v1.c"
#include "xx-fractional-scale-v2.c"

// Wayland global info structure
typedef struct wl_global_info {
	void *next;  // Must be first member for hash table chaining
	uint32_t name;
	uint32_t version;
	char interface[64];
	void *bind;
} wl_global_info_t;

// Linked list of outputs
static w_output_t *wl_outputs = NULL;

// Window role types
typedef enum {
	W_ROLE_DEFAULT = 0,      // xdg_toplevel
	W_ROLE_POPUP,            // xdg_popup
	W_ROLE_INPUT,            // input method surface
	W_ROLE_LAYER,            // layer shell
} w_role_type_t;

// Window info structure
typedef struct {
	w_role_type_t role_type;
	union {
		void *surface;
		struct {
			struct xdg_surface *xdg_surface;
			struct xdg_toplevel *xdg_toplevel;
		} toplevel;
		struct {
			struct xdg_surface *xdg_surface;
			struct xdg_popup *xdg_popup;
			struct xdg_positioner *xdg_positioner;
		} popup;
		struct {
			void *input_popup;
		} input;
		struct {
			struct zwlr_layer_surface_v1 *layer_surface;
			int anchor;
		} layer;
	};
	struct wl_global_info *role_global;
} wl_window_info_t;

struct w_event_item {
	w_cb_t cb;
	void *data;
};

typedef struct w_ui_style {
	uint8_t border_color[4];
	int border_width;
	uint8_t bg_menu[4];		// RGBA
	uint8_t fg_menu[4];
	uint8_t sep_color[4];
	uint8_t separator_color[4];
	int sep_height;
	uint8_t padding[2];
	const char *font_name;
	int font_size;

	uint8_t bg_window[4];
	uint8_t bg_title[4];
	uint8_t fg_title[4];
	uint8_t sep_title[4];
	uint8_t padding_title;
} w_ui_style_t;

static const w_ui_style_t default_ui_style = {
	.border_color = {209, 209, 209, 255},	// light gray border
	.border_width = 1,
	.bg_menu = {255, 255, 255, 255},		// white background
	.fg_menu = {0, 0, 0, 255},				// black text
	.sep_color = {229, 243, 255, 255},		// light blue selection
	.separator_color = {224, 224, 224, 255},// light gray separator
	.sep_height = 1,
	.padding = {2, 5},
	.font_name = "Sans",
	.font_size = 16,

	.bg_window = {255, 255, 255, 255},
	.bg_title = {236, 246, 249, 255},
	.fg_title = {12, 12, 12, 255},
	.sep_title = {204, 204, 204, 255},
	.padding_title = 8,
};
static PangoFontDescription *font_desc = NULL;
static int line_height = 0;

struct w_window {
	void *data;
	struct w_event_item events[W_EVENT_MAX];
	bool visible;
	bool show_hack;
	guint redraw_source_id; 	// GSource ID for idle redraw (0 if none pending)
	guint config_debounce_id;	// GSource ID for config debounce (0 if none pending)
	double scale;
	uint32_t scale_num,scale_denom;	// 缩放比例，用分子除以分母表示
	int width;
	int height;
	struct wl_surface *surface;
	wl_window_info_t wl_info;
	union{
		struct wp_fractional_scale_v1 *fractional_scale;
		struct xx_fractional_scale_v2 *fractional_scale_v2;
	};
	struct wp_viewport *viewport;
	struct wp_alpha_modifier_surface_v1 *alpha_modifier;
	struct ext_background_effect_surface_v1 *background_effect;
	bool decorated;
	bool configured;
	bool cursor_updated;
	int tran;
	int effect;			// 当前的窗口特效（W_EFFECT_*）
	WUI_BUFFER *buf;
	PangoLayout *pango_layout;

	int mouse_x;
	int mouse_y;
	w_output_t *outputs[4];  	// 窗口所在的output列表（最多4个）
	int outputs_count;          // output数量
	w_output_t *output;       	// 主要output（第一个）
	w_win_t next;  				// 全局窗口链表
	char *title;
	cairo_rectangle_int_t close_btn;

	bool dragging;           // 正在拖拽标题栏移动窗口
	bool close_hovered;       // mouse hovering over close button
	bool close_pressed;       // mouse pressed on close button
	int drag_start_x;        // 拖拽起始鼠标X
	int drag_start_y;        // 拖拽起始鼠标Y
	int drag_origin_x;       // 拖拽起始窗口位置X（仅layer）
	int drag_origin_y;       // 拖拽起始窗口位置Y（仅layer）
	int window_x;            // 窗口当前位置X（仅layer）
	int window_y;            // 窗口当前位置Y（仅layer）
};

static bool w_initialized = false;
static const char *w_app_id = NULL;

// Wayland specific
static GMainLoop *wl_loop=NULL;
static struct wl_display *wl_display = NULL;
static struct wl_registry *wl_registry = NULL;
static struct wl_seat *wl_seat = NULL;
static struct wl_pointer *wl_pointer = NULL;
static struct wl_keyboard *wl_keyboard = NULL;
static struct xdg_wm_base *wl_xdg_wm_base = NULL;
static struct wl_shm *wl_shm = NULL;
static struct wp_fractional_scale_manager_v1 *wl_fractional_scale_manager = NULL;
static struct xx_fractional_scale_manager_v2 *wl_fractional_scale_manager_v2 = NULL;
static struct wl_compositor *wl_compositor = NULL;
static struct wp_viewporter *wl_viewporter = NULL;
static struct wp_cursor_shape_manager_v1 *wl_cursor_shape_manager = NULL;
static struct xdg_system_bell_v1 *wl_system_bell = NULL;
static struct zwlr_layer_shell_v1 *wl_layer_shell = NULL;
static struct ext_background_effect_manager_v1 *wl_background_effect = NULL;
static uint32_t wl_background_effect_caps = 0;
static uint32_t wl_last_serial = 0;
static w_win_t wl_pointer_win = NULL;
static w_win_t wl_keyboard_win = NULL;
static w_win_t wl_modal_win = NULL;
static int wl_pointer_x = 0;
static int wl_pointer_y = 0;
static const bool wl_debug = false;
static W_INPUT_HOOK *w_input_hook;
const char *(*w_translate)(const char *s);
static bool wl_tran_self = false;
static struct wl_region *wl_region_empty = NULL;
static w_win_t root;

int w_workarea_update(w_output_t *output);
int w_workarea_fallback(int *x, int *y, int *w, int *h, w_output_t *output);
static void w_win_update_effect_region(w_win_t win);

// Hash table to store all global interfaces
static LHashTable *wl_globals_hash = NULL;

// Clipboard state for ext-data-control
static struct ext_data_control_device_v1 *wl_data_control_device = NULL;
static struct ext_data_control_offer_v1 *wl_data_control_offer = NULL;

// Data source state for clipboard set
static struct {
	char *data;
	int size;
	char *mime;
} wl_data_control_source_data = {0};

// Currently active data source (for cleanup on cancelled/new selection)
static struct ext_data_control_source_v1 *wl_active_source = NULL;

// Standard clipboard fallback (wl_data_device)
static struct wl_data_device_manager *wl_data_device_manager = NULL;
static struct wl_data_device *wl_data_device = NULL;
static struct wl_data_offer *wl_data_offer = NULL;
static struct wl_data_source *wl_data_source_active = NULL;

// Data source state for standard clipboard (reuses wl_data_control_source_data)

// Track whether we currently own the clipboard selection
static bool wl_owns_clipboard = false;

// External display mode (no own loop)
static GSource *wl_gsource = NULL;

// Global window list for config change notification
static w_win_t wl_windows = NULL;

// Global config debounce timer
static guint wl_config_debounce_id = 0;
static guint wl_config_done_id = 0;

// Config debounce interval (ms)
#define CONFIG_DEBOUNCE_INTERVAL 100

// Functions to free global info
static void wl_global_info_free(wl_global_info_t *info) {
	if(info) {
		l_free(info);
	}
}

// Idle redraw callback - performs actual drawing
static gboolean idle_redraw_cb(gpointer data)
{
	w_win_t win = (w_win_t)data;
	win->redraw_source_id = 0;  // Mark as removed
	
	if(w_buffer_busy(win->buf))
	{
		win->redraw_source_id=g_timeout_add(50,idle_redraw_cb,data);
		return G_SOURCE_REMOVE;
	}
	
	// Skip if not visible
	if(!win->visible && !win->show_hack)
		return G_SOURCE_REMOVE;
	if(!win->configured)
		return G_SOURCE_REMOVE;

	// When decorated, add border and title bar space to buffer allocation
	int border = win->decorated ? default_ui_style.border_width : 0;
	int title_height = win->decorated ? (line_height + 2 * default_ui_style.padding_title + 1) : 0;
	int buf_w = (int)round((win->width + 2 * border) * win->scale);
	int buf_h = (int)round((win->height + 2 * border + title_height) * win->scale);

	// Get a non-busy buffer slot to draw into
	cairo_t *cr = w_buffer_begin_frame(win->buf,buf_w,buf_h);
	if(!cr)
		return G_SOURCE_REMOVE;  // both slots busy, skip frame
	cairo_set_operator(cr,CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr,CAIRO_OPERATOR_OVER);

	if(!win->visible)
	{
		goto END;
	}
	// Apply scale transformation so drawing uses logical coordinates
	if(win->scale > 1.0)
		cairo_scale(cr, win->scale, win->scale);

	// Draw title bar and border decorations for decorated windows
	if(border > 0)
	{
		double total_w = win->width + 2 * border;
		double total_h = win->height + 2 * border + title_height;
		const w_ui_style_t *style = &default_ui_style;

		// Draw window background
		cairo_set_source_rgb(cr, style->bg_window[0] / 255.0, style->bg_window[1] / 255.0, style->bg_window[2] / 255.0);
		cairo_paint(cr);

		// Draw title bar background
		cairo_rectangle(cr, 0, 0, total_w, title_height);
		cairo_set_source_rgb(cr, style->bg_title[0] / 255.0, style->bg_title[1] / 255.0, style->bg_title[2] / 255.0);
		cairo_fill(cr);

		// Draw centered title text
		if(win->title)
		{
			PangoLayout *layout = win->pango_layout;
			if(!layout)
				win->pango_layout=layout=pango_cairo_create_layout(cr);
			if(layout)
			{
				pango_layout_set_font_description(layout, font_desc);
				pango_layout_set_text(layout, win->title, -1);

				double close_btn_size = line_height;
				double close_margin = style->padding_title;
				double close_x = total_w - border - close_btn_size - close_margin;

				int text_w, text_h;
				pango_layout_get_pixel_size(layout, &text_w, &text_h);
				double text_area_w = close_x - style->padding_title;
				double text_x = style->padding_title + (text_area_w - text_w) / 2;
				if(text_x < style->padding_title)
					text_x = style->padding_title;
				double text_y = (title_height - text_h) / 2;

				cairo_set_source_rgb(cr, style->fg_title[0] / 255.0, style->fg_title[1] / 255.0, style->fg_title[2] / 255.0);
				cairo_move_to(cr, text_x, text_y);
				pango_cairo_show_layout(cr, layout);
			}
		}

		// Draw close button (X mark) and store its rect for hit testing
		{
			double close_btn_size = line_height + 6;
			double close_margin = (title_height - close_btn_size) / 2;
			double close_x = total_w - border - close_btn_size - close_margin;
			double close_y = (title_height - close_btn_size) / 2;

			win->close_btn.x = (int)close_x;
			win->close_btn.y = (int)close_y;
			win->close_btn.width = (int)close_btn_size;
			win->close_btn.height = (int)close_btn_size;

			// Win11-style: red background on hover/press
			if(win->close_pressed)
			{
				cairo_set_source_rgb(cr, 0.71, 0.13, 0.13); // darker red (E32020)
				cairo_rectangle(cr, close_x, close_y, close_btn_size, close_btn_size);
				cairo_fill(cr);
				cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // white X
			}
			else if(win->close_hovered)
			{
				cairo_set_source_rgb(cr, 0.84, 0.18, 0.15); // light red (D72C2C)
				cairo_rectangle(cr, close_x, close_y, close_btn_size, close_btn_size);
				cairo_fill(cr);
				cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // white X
			}
			else
			{
				cairo_set_source_rgb(cr, style->fg_title[0] / 255.0, style->fg_title[1] / 255.0, style->fg_title[2] / 255.0);
			}
			cairo_set_line_width(cr, 2.0);
			double cx = close_x + close_btn_size / 2;
			double cy = close_y + close_btn_size / 2;
			// X mark uses original (smaller) size, background rect uses close_btn_size
			double half = line_height * 0.3;
			cairo_move_to(cr, cx - half, cy - half);
			cairo_line_to(cr, cx + half, cy + half);
			cairo_move_to(cr, cx + half, cy - half);
			cairo_line_to(cr, cx - half, cy + half);
			cairo_stroke(cr);
		}

		// Draw separator line at bottom of title bar
		cairo_set_source_rgb(cr, style->sep_title[0] / 255.0, style->sep_title[1] / 255.0, style->sep_title[2] / 255.0);
		cairo_set_line_width(cr, 1.0);
		cairo_move_to(cr, 0, title_height);
		cairo_line_to(cr, total_w, title_height);
		cairo_stroke(cr);

		// Draw outer border
		cairo_set_source_rgb(cr, style->border_color[0] / 255.0, style->border_color[1] / 255.0, style->border_color[2] / 255.0);
		cairo_set_line_width(cr, (double)border);
		double half_border = border / 2.0;
		cairo_rectangle(cr, half_border, half_border, total_w - border, total_h - border);
		cairo_stroke(cr);
	}

	// Translate to content area origin when decorated, and clip to content bounds
	if(border > 0)
	{
		cairo_translate(cr, border, border + title_height);
		cairo_rectangle(cr, 0, 0, win->width, win->height);
		cairo_clip(cr);
	}

	W_EVENT e = {0};
	e.type = W_DRAW;
	e.draw.cr = cr;
	if(border)
	{
		e.draw.origin_x = border*win->scale;
		e.draw.origin_y = (border+title_height)*win->scale;
	}

	if(win->events[W_DRAW].cb)
		win->events[W_DRAW].cb(win, &e, win->events[W_DRAW].data);

	if(win->tran && wl_tran_self)
	{
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, (255-win->tran)/255.0);
		cairo_set_operator(cr, CAIRO_OPERATOR_DEST_IN);
		cairo_paint(cr);
	}
END:
	cairo_destroy(cr);
	// Submit the buffer to the compositor
	struct wl_buffer *wl_buf = w_buffer_end_frame(win->buf);
	wl_surface_attach(win->surface, wl_buf, 0, 0);
	wl_surface_damage_buffer(win->surface, 0, 0, buf_w, buf_h);
	wl_surface_commit(win->surface);

	return G_SOURCE_REMOVE;
}

// Window-level config debounce callback
static gboolean window_config_debounce_cb(gpointer data)
{
	w_win_t win = (w_win_t)data;
	win->config_debounce_id = 0;

	if(win->events[W_CONFIG].cb)
	{
		W_EVENT e = {0};
		e.type = W_CONFIG;
		win->events[W_CONFIG].cb(win, &e, win->events[W_CONFIG].data);
		if(wl_debug)
			fprintf(stderr, "[wui-wayland] sent W_CONFIG to window %p\n", (void*)win);
	}

	return G_SOURCE_REMOVE;
}

// Trigger window config event with debounce
static void trigger_window_config(w_win_t win)
{
	if(!win)
		return;

	// Remove previous debounce timer
	if(win->config_debounce_id > 0)
		g_source_remove(win->config_debounce_id);

	// Set new debounce timer
	win->config_debounce_id = g_timeout_add(CONFIG_DEBOUNCE_INTERVAL, window_config_debounce_cb, win);
}

static gboolean global_get_config_done_cb(gpointer data)
{
	wl_config_done_id = 0;
	w_win_hide(root);

	for(w_win_t win = wl_windows; win; win = win->next)
		trigger_window_config(win);

	return G_SOURCE_REMOVE;
}

// Global config debounce callback - update workarea and notify all windows
static gboolean global_config_debounce_cb(gpointer data)
{
	wl_config_debounce_id = 0;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] global config debounce triggered\n");

	if(wl_config_done_id)
	{
		wl_config_done_id=g_timeout_add(10,global_get_config_done_cb,NULL);
	}

	w_win_show(root);
	wl_display_roundtrip(wl_display);
	// Update workarea for all outputs
	w_workarea_update(wl_outputs);

	return G_SOURCE_REMOVE;
}

// Trigger global config change with debounce
static void trigger_global_config(void)
{
	if(!w_initialized)
		return;
	// Remove previous debounce timer
	if(wl_config_debounce_id > 0)
		g_source_remove(wl_config_debounce_id);

	// Set new debounce timer
	wl_config_debounce_id = g_timeout_add(CONFIG_DEBOUNCE_INTERVAL, global_config_debounce_cb, NULL);
}

// xdg_wm_base ping listener
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

static void update_win_font(w_win_t win)
{
	if(!win->pango_layout)
		return;
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr=cairo_create(surface);
	cairo_scale(cr,win->scale,win->scale);
	pango_cairo_update_layout(cr,win->pango_layout);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

// Fractional scale listener
static void fractional_scale_preferred_scale(void *data,
	struct wp_fractional_scale_v1 *wp_fractional_scale_v1, uint32_t scale)
{
	w_win_t win = (w_win_t)data;

	// scale is in 120ths (120 = 1.0, 180 = 1.5, 240 = 2.0)
	double new_scale = scale / 120.0;

	// 直接使用系统推荐的缩放比例
	if(new_scale != win->scale)
	{
		win->scale = new_scale;
		win->scale_num = scale;
		win->scale_denom = 120;

		if(wl_debug)
			fprintf(stderr, "[wui-wayland] window %p(%s) scale changed to %.2f\n",
				(void*)win, win->title,new_scale);
		update_win_font(win);
		w_win_redraw(win);
		// Trigger config event with debounce
		if(l_slist_length(wl_outputs)==1)
			trigger_window_config(win);
	}
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
	.preferred_scale = fractional_scale_preferred_scale,
};

static void fractional_scale_v2_scale_factor(void *data,
			     struct xx_fractional_scale_v2 *xx_fractional_scale_v2,
			     uint32_t scale_8_24)
{
	w_win_t win = (w_win_t)data;
	double new_scale = scale_8_24 / ((double)(1<<24));
	
	if(new_scale != win->scale)
	{
		win->scale = new_scale;
		win->scale_num = scale_8_24;
		win->scale_denom = 1<<24;

		if(wl_debug)
			fprintf(stderr, "[wui-wayland] window %p(%s) scale(v2) changed to %.2f\n",
				(void*)win, win->title,new_scale);
		update_win_font(win);
		xx_fractional_scale_v2_set_scale_factor(xx_fractional_scale_v2,scale_8_24);
		w_win_redraw(win);
		// Trigger config event with debounce
		if(l_slist_length(wl_outputs)==1)
			trigger_window_config(win);
	}

}

static const struct xx_fractional_scale_v2_listener fractional_scale_v2_listener = {
	.scale_factor = fractional_scale_v2_scale_factor,
};

// wl_surface listeners - track which outputs the window is on
static int output_array_find(w_win_t win, uint32_t name)
{
	for(int i = 0; i < win->outputs_count; i++)
	{
		if(win->outputs[i]->name == name)
			return i;
	}
	return -1;
}

static void surface_enter(void *data, struct wl_surface *surface, struct wl_output *output)
{
	w_win_t win = (w_win_t)data;
	if(!win || !output)
		return;

	uint32_t output_name = wl_proxy_get_id((struct wl_proxy *)output);

	// 查找output是否已在数组中
	if(output_array_find(win, output_name) >= 0)
		return;  // 已存在

	// 从全局列表中找到对应的screen info
	w_output_t *screen;
	for(screen = wl_outputs; screen; screen = screen->next)
	{
		if(screen->name == output_name)
		{
			// 添加到窗口的output数组
			if(win->outputs_count < 4)
			{
				win->outputs[win->outputs_count++] = screen;
				win->output = win->outputs[0];  // 主要output为第一个
			}
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] surface enter: added output %u (%dx%d, scale=%d), count=%d\n",
					output_name, screen->width, screen->height, screen->scale, win->outputs_count);
			break;
		}
	}
}

static void surface_leave(void *data, struct wl_surface *surface, struct wl_output *output)
{
	w_win_t win = (w_win_t)data;
	if(!win || !output)
		return;

	uint32_t output_name = wl_proxy_get_id((struct wl_proxy *)output);

	int idx = output_array_find(win, output_name);
	if(idx < 0)
		return;  // 不在数组中

	// 从数组中移除（移动后面的元素）
	for(int i = idx; i < win->outputs_count - 1; i++)
		win->outputs[i] = win->outputs[i + 1];
	win->outputs_count--;
	win->outputs[win->outputs_count] = NULL;

	// 更新主要output
	win->output = win->outputs_count > 0 ? win->outputs[0] : NULL;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] surface leave: removed output %u, remaining=%d\n",
			output_name, win->outputs_count);
}

static void surface_preferred_buffer_scale(void *data,
				       struct wl_surface *wl_surface,
				       int32_t factor)
{
	if(!wl_fractional_scale_manager && !wl_fractional_scale_manager_v2)
	{
		w_win_t win = (w_win_t)data;
		if(!win)
			return;
		win->scale=factor;
		win->scale_num=factor;
		win->scale_denom=1;

		if(wl_debug)
			fprintf(stderr, "[wui-wayland] window %p scale changed to %d\n",
				(void*)win, factor);

		update_win_font(win);
		w_win_redraw(win);
		// Trigger config event with debounce
		if(l_slist_length(wl_outputs)==1)
			trigger_window_config(win);
	}
}

static void surface_preferred_buffer_transform(void *data,
					   struct wl_surface *wl_surface,
					   uint32_t transform)
{
}

static const struct wl_surface_listener surface_listener = {
	.enter = surface_enter,
	.leave = surface_leave,
	.preferred_buffer_scale = surface_preferred_buffer_scale,
	.preferred_buffer_transform = surface_preferred_buffer_transform,
};

// wl_output listeners
static void output_geometry(void *data, struct wl_output *wl_output,
	int32_t x, int32_t y,
	int32_t physical_width, int32_t physical_height,
	int32_t subpixel,
	const char *make, const char *model,
	int32_t transform)
{
	w_output_t *info = (w_output_t *)data;
	if(!info)
		return;

	info->x = x;
	info->y = y;
}

static void output_mode(void *data, struct wl_output *wl_output,
	uint32_t flags,
	int32_t width, int32_t height,
	int32_t refresh)
{
	w_output_t *info = (w_output_t *)data;
	if(!info)
		return;

	if(flags & 0x1)  // WL_OUTPUT_MODE_CURRENT
	{
		// Check if resolution actually changed
		if(info->width != width || info->height != height)
		{
			info->width = width;
			info->height = height;
		}
	}
}

static void output_done(void *data, struct wl_output *wl_output)
{
	w_output_t *info = (w_output_t *)data;
	if(!info)
		return;

	info->done = true;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] output done: name=%u, %dx%d, scale=%d\n",
			info->name, info->width, info->height, info->scale);

	// Trigger global config change with debounce (will update workarea)
	trigger_global_config();
}

static void output_scale(void *data, struct wl_output *wl_output, int32_t scale)
{
	w_output_t *info = (w_output_t *)data;
	if(!info)
		return;

	// Check if scale actually changed
	if(info->scale != scale)
	{
		info->scale = scale;
	}
}

static const struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
};

// ext-background-effect-v1 listeners
static void background_effect_capabilities(void *data,
	struct ext_background_effect_manager_v1 *manager, uint32_t flags)
{
	wl_background_effect_caps = flags;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] background effect capabilities: %x\n", flags);
}

static const struct ext_background_effect_manager_v1_listener background_effect_manager_listener = {
	.capabilities = background_effect_capabilities,
};

#include "wui-clipboard.c"

static void registry_handle_global(void *data, struct wl_registry *registry,
			   uint32_t name, const char *interface, uint32_t version)
{
	if(wl_debug)
		fprintf(stderr, "[wui-wayland] registry global: %s v%u (name=%u)\n", interface, version, name);

	// Handle wl_output specially - bind and track for screen info
	if(strcmp(interface, "wl_output") == 0)
	{
		// Bind wl_output (use version 2 for scale/done events, but cap at available)
		uint32_t bind_version = version < 2 ? version : 2;
		struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, bind_version);
		if(output)
		{
			// Create screen info structure
			w_output_t *screen = l_new0(w_output_t);
			screen->name = name;
			screen->output = output;
			screen->scale = 1;  // Default scale
			screen->done = false;

			// Add listener to get geometry/mode/scale info
			wl_output_add_listener(output, &output_listener, screen);

			// Add to outputs list
			wl_outputs = (w_output_t *)l_slist_append(wl_outputs, screen);

			if(wl_debug)
				fprintf(stderr, "[wui-wayland] bound wl_output: name=%u, version=%u\n", name, bind_version);
		}
		return;
	}

	// Check if interface already exists
	wl_global_info_t *existing = l_hash_table_lookup(wl_globals_hash, interface);
	if(existing) {
		// Update version if newer
		if(version > existing->version) {
			existing->version = version;
			existing->name = name;
		}
		return;
	}

	// Create new global info
	wl_global_info_t *info = l_new0(wl_global_info_t);
	info->name = name;
	info->version = version;
	strncpy(info->interface, interface, sizeof(info->interface) - 1);
	info->interface[sizeof(info->interface) - 1] = '\0';

	// Insert into hash table
	l_hash_table_insert(wl_globals_hash, info);
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
				      uint32_t name)
{
	if(wl_debug)
		fprintf(stderr, "[wui-wayland] registry global remove: %u\n", name);

	// Check if it's an output being removed
	w_output_t *screen;
	for(screen = wl_outputs; screen != NULL; screen = screen->next)
	{
		if(screen->name == name)
		{
			wl_output_destroy(screen->output);
			wl_outputs = (w_output_t *)l_slist_remove(wl_outputs, screen);
			l_free(screen);
			return;
		}
	}

	// Find and remove from globals hash table
	LHashIter iter;
	l_hash_iter_init(&iter, wl_globals_hash);
	while(1) {
		wl_global_info_t *info = l_hash_iter_next(&iter);
		if(!info) break;

		if(info->name == name) {
			l_hash_table_remove(wl_globals_hash, info);
			wl_global_info_free(info);
			break;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

// wl_pointer listeners
static void pointer_enter(void *data, struct wl_pointer *pointer,
	uint32_t serial, struct wl_surface *surface,
	wl_fixed_t sx, wl_fixed_t sy)
{
	wl_last_serial = serial;
	wl_pointer_win = surface ? (w_win_t)wl_surface_get_user_data(surface) : NULL;
	wl_pointer_x = wl_fixed_to_int(sx);
	wl_pointer_y = wl_fixed_to_int(sy);

	if(!wl_pointer_win)
		return;

	wl_pointer_win->cursor_updated=false;
	if(wl_pointer_win->events[W_MOUSE_ENTER].cb)
	{
		W_EVENT e;
		e.type = W_MOUSE_ENTER;
		e.mouse.x = wl_pointer_x;
		e.mouse.y = wl_pointer_y;
		e.mouse.button = 0;
		// Translate to client-area coordinates for decorated windows
		if(wl_pointer_win->decorated)
		{
			e.mouse.x -= default_ui_style.border_width;
			int title_height = line_height + 2 * default_ui_style.padding_title + 1;
			e.mouse.y -= default_ui_style.border_width + title_height;
		}
		wl_pointer_win->events[W_MOUSE_ENTER].cb(wl_pointer_win, &e, wl_pointer_win->events[W_MOUSE_ENTER].data);
	}
	if(!wl_pointer_win->cursor_updated)
	{
		wl_pointer_win->cursor_updated=true;
		w_win_set_cursor(wl_pointer_win,W_CURSOR_DEFAULT);
	}
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
	uint32_t serial, struct wl_surface *surface)
{
	wl_last_serial = serial;

	if(wl_pointer_win && wl_pointer_win->dragging)
	{
		wl_pointer_win->dragging = false;
	}

	if(wl_pointer_win && wl_pointer_win->events[W_MOUSE_LEAVE].cb)
	{
		W_EVENT e;
		e.type = W_MOUSE_LEAVE;
		e.mouse.x = wl_pointer_x;
		e.mouse.y = wl_pointer_y;
		e.mouse.button = 0;
		// Translate to client-area coordinates for decorated windows
		if(wl_pointer_win->decorated)
		{
			e.mouse.x -= default_ui_style.border_width;
			int title_height = line_height + 2 * default_ui_style.padding_title + 1;
			e.mouse.y -= default_ui_style.border_width + title_height;
		}
		wl_pointer_win->events[W_MOUSE_LEAVE].cb(wl_pointer_win, &e, wl_pointer_win->events[W_MOUSE_LEAVE].data);
	}
	// Reset close button state when pointer leaves
	if(wl_pointer_win && (wl_pointer_win->close_hovered || wl_pointer_win->close_pressed))
	{
		wl_pointer_win->close_hovered = false;
		wl_pointer_win->close_pressed = false;
		w_win_redraw(wl_pointer_win);
	}
	wl_pointer_win = NULL;
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
	uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	wl_pointer_x = wl_fixed_to_int(sx);
	wl_pointer_y = wl_fixed_to_int(sy);

	// Handle dragging for layer surfaces (manual move)
	if(wl_pointer_win && wl_pointer_win->dragging &&
		wl_pointer_win->wl_info.role_type == W_ROLE_LAYER)
	{
		// Use current window position (window_x/y) instead of drag_origin_x/y,
		// because the compositor moves the surface between motion events,
		// making wl_pointer_x/y relative to the NEW surface origin.
		// Using drag_origin (which is fixed at drag start) would lose all
		// movement from previous events.
		int dx = wl_pointer_x - wl_pointer_win->drag_start_x;
		int dy = wl_pointer_y - wl_pointer_win->drag_start_y;
		w_win_move(wl_pointer_win,
			wl_pointer_win->window_x + dx,
			wl_pointer_win->window_y + dy);
		return;  // 拖拽中不转发事件
	}

	// Track close button hover for decorated windows
	if(wl_pointer_win && wl_pointer_win->decorated)
	{
		cairo_rectangle_int_t *cb = &wl_pointer_win->close_btn;
		bool over = wl_pointer_x >= cb->x && wl_pointer_x < cb->x + cb->width &&
				wl_pointer_y >= cb->y && wl_pointer_y < cb->y + cb->height;
		if(over != wl_pointer_win->close_hovered)
		{
			wl_pointer_win->close_hovered = over;
			w_win_redraw(wl_pointer_win);
		}

	}

	if(wl_pointer_win && wl_pointer_win->events[W_MOUSE_MOVE].cb)
	{
		W_EVENT e;
		e.type = W_MOUSE_MOVE;
		e.mouse.x = wl_pointer_x;
		e.mouse.y = wl_pointer_y;
		e.mouse.button = 0;
		// Translate to client-area coordinates for decorated windows
		if(wl_pointer_win->decorated)
		{
			e.mouse.x -= default_ui_style.border_width;
			int title_height = line_height + 2 * default_ui_style.padding_title + 1;
			e.mouse.y -= default_ui_style.border_width + title_height;
		}
		wl_pointer_win->events[W_MOUSE_MOVE].cb(wl_pointer_win, &e, wl_pointer_win->events[W_MOUSE_MOVE].data);
	}
}

static void pointer_button(void *data, struct wl_pointer *pointer,
	uint32_t serial, uint32_t time,
	uint32_t button, uint32_t state)
{
	wl_last_serial = serial;

	if(!wl_pointer_win)
		return;

	if(wl_modal_win && wl_modal_win!=wl_pointer_win && wl_pointer_win->wl_info.role_type!=W_ROLE_POPUP)
	{
		if(state == WL_POINTER_BUTTON_STATE_PRESSED)
			w_win_destroy(wl_modal_win);
		return;
	}

	wl_pointer_win->mouse_x = wl_pointer_x;
	wl_pointer_win->mouse_y = wl_pointer_y;

	// Handle close button click for decorated windows
	if(wl_pointer_win->decorated && button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
	{
		cairo_rectangle_int_t *cb = &wl_pointer_win->close_btn;
		if(wl_pointer_x >= cb->x && wl_pointer_x < cb->x + cb->width &&
		   wl_pointer_y >= cb->y && wl_pointer_y < cb->y + cb->height)
		{
			wl_pointer_win->close_pressed = true;
			w_win_redraw(wl_pointer_win);
			return;
		}
	}

	// Handle close button release for decorated windows
	if(wl_pointer_win->decorated && button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_RELEASED)
	{
		if(wl_pointer_win->close_pressed)
		{
			wl_pointer_win->close_pressed = false;
			cairo_rectangle_int_t *cb = &wl_pointer_win->close_btn;
			if(wl_pointer_x >= cb->x && wl_pointer_x < cb->x + cb->width &&
			   wl_pointer_y >= cb->y && wl_pointer_y < cb->y + cb->height)
			{
				w_win_redraw(wl_pointer_win);
				if(wl_pointer_win->events[W_CLOSE].cb)
				{
					W_EVENT e = {0};
					e.type = W_CLOSE;
					wl_pointer_win->events[W_CLOSE].cb(wl_pointer_win, &e, wl_pointer_win->events[W_CLOSE].data);
				}
				return;
			}
			w_win_redraw(wl_pointer_win);
		}
	}

	// Handle title bar right-click for compositor window menu
	if(wl_pointer_win->decorated && button == BTN_RIGHT && state == WL_POINTER_BUTTON_STATE_PRESSED)
	{
		int title_height = line_height + 2 * default_ui_style.padding_title + 1;
		if(wl_pointer_y < title_height)
		{
			if(wl_pointer_win->wl_info.role_type == W_ROLE_DEFAULT &&
			   wl_pointer_win->wl_info.toplevel.xdg_toplevel)
			{
				xdg_toplevel_show_window_menu(wl_pointer_win->wl_info.toplevel.xdg_toplevel,
					wl_seat, wl_last_serial, wl_pointer_x, wl_pointer_y);
			}
			return;
		}
	}

	// Handle title bar drag for decorated windows
	if(wl_pointer_win->decorated && button == BTN_LEFT)
	{
		int title_height = line_height + 2 * default_ui_style.padding_title + 1;
		if(state == WL_POINTER_BUTTON_STATE_PRESSED && wl_pointer_y < title_height)
		{
			wl_pointer_win->dragging = true;
			wl_pointer_win->drag_start_x = wl_pointer_x;
			wl_pointer_win->drag_start_y = wl_pointer_y;

			switch(wl_pointer_win->wl_info.role_type)
			{
				case W_ROLE_DEFAULT:
					if(wl_pointer_win->wl_info.toplevel.xdg_toplevel)
						xdg_toplevel_move(wl_pointer_win->wl_info.toplevel.xdg_toplevel,
							wl_seat, wl_last_serial);
					break;
				case W_ROLE_LAYER:
					// Manual drag: save initial window position
					wl_pointer_win->drag_origin_x = wl_pointer_win->window_x;
					wl_pointer_win->drag_origin_y = wl_pointer_win->window_y;
					break;
				default:
					wl_pointer_win->dragging = false;
					break;
			}
			return;  // 消费press事件，不转发给用户
		}

		// Handle drag stop for all cases
		if(wl_pointer_win->dragging && state == WL_POINTER_BUTTON_STATE_RELEASED)
		{
			wl_pointer_win->dragging = false;
			return;  // 消费release事件
		}
	}

	int w_button = button == BTN_LEFT ? W_BUTTON_LEFT :
		button == BTN_RIGHT ? W_BUTTON_RIGHT : W_BUTTON_MIDDLE;
	int event_type = state == WL_POINTER_BUTTON_STATE_PRESSED ? W_MOUSE_DOWN : W_MOUSE_UP;

	if(wl_pointer_win->events[event_type].cb)
	{
		W_EVENT e;
		e.type = event_type;
		e.mouse.x = wl_pointer_x;
		e.mouse.y = wl_pointer_y;
		e.mouse.button = w_button;
		if(wl_pointer_win->decorated)
		{
			e.mouse.x-=default_ui_style.border_width;
			if(e.mouse.x<0 || e.mouse.x>=wl_pointer_win->width)
				return;
			int title_height = line_height + 2 * default_ui_style.padding_title + 1;
			e.mouse.y-=default_ui_style.border_width+title_height;
			if(e.mouse.y<0 || e.mouse.y>=wl_pointer_win->height)
				return;
		}
		wl_pointer_win->events[event_type].cb(wl_pointer_win, &e, wl_pointer_win->events[event_type].data);
	}
}

static void pointer_axis(void *data, struct wl_pointer *pointer,
	uint32_t time, uint32_t axis, wl_fixed_t value)
{
	if(!wl_pointer_win || !wl_pointer_win->events[W_SCROLL].cb)
		return;

	W_EVENT e;
	e.type = W_SCROLL;
	e.scroll.x = wl_pointer_x;
	e.scroll.y = wl_pointer_y;
	e.scroll.button = 0;

	/* value is in wl_fixed_t, convert to float */
	/* positive value = scroll down/right, negative = scroll up/left */
	if(axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
	{
		e.scroll.dx = 0.0;
		e.scroll.dy = wl_fixed_to_double(value);
	}
	else if(axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
	{
		e.scroll.dx = wl_fixed_to_double(value);
		e.scroll.dy = 0.0;
	}
	else
	{
		return;
	}

	wl_pointer_win->events[W_SCROLL].cb(wl_pointer_win, &e, wl_pointer_win->events[W_SCROLL].data);
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis = pointer_axis,
	.frame = (void*)l_noop,
	.axis_source = (void*)l_noop,
	.axis_stop = (void*)l_noop,
	.axis_discrete = (void*)l_noop,
};

// Keyboard event handlers
static void keyboard_keymap(void *data, struct wl_keyboard *keyboard,
	uint32_t format, int fd, uint32_t size)
{
	// Keymap format - we don't need to parse it for basic key events
	// Close the fd as we don't use it
	if(fd >= 0)
		close(fd);
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, struct wl_surface *surface,
	struct wl_array *keys)
{
	wl_last_serial = serial;
	wl_keyboard_win = surface ? (w_win_t)wl_surface_get_user_data(surface) : NULL;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] keyboard enter: win=%p, serial=%u\n", (void*)wl_keyboard_win, serial);
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, struct wl_surface *surface)
{
	wl_last_serial = serial;
	wl_keyboard_win = NULL;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] keyboard leave: serial=%u\n", serial);
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, uint32_t time,
	uint32_t key, uint32_t state)
{
	wl_last_serial = serial;

	if(!wl_keyboard_win)
		return;

	int event_type = state == WL_KEYBOARD_KEY_STATE_PRESSED ? W_KEY_DOWN : W_KEY_UP;

	if(wl_keyboard_win->events[event_type].cb)
	{
		W_EVENT e;
		e.type = event_type;
		e.key.key = key;
		e.key.state = state;
		e.key.serial = serial;
		wl_keyboard_win->events[event_type].cb(wl_keyboard_win, &e, wl_keyboard_win->events[event_type].data);

		if(wl_debug)
			fprintf(stderr, "[wui-wayland] keyboard key: win=%p, key=%u, state=%u\n",
				(void*)wl_keyboard_win, key, state);
	}
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, uint32_t mods_depressed,
	uint32_t mods_latched, uint32_t mods_locked,
	uint32_t group)
{
	wl_last_serial = serial;
	// Modifier state changes - could be used for shift/ctrl/alt detection
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = (void*)l_noop,
};

static void seat_capabilities(void *data,
			     struct wl_seat *wl_seat,
			     uint32_t capabilities)
{
	if(wl_debug)
		fprintf(stderr,"[wui-wayland] seat capabilities %x\n",capabilities);
	if((capabilities & WL_SEAT_CAPABILITY_POINTER))
	{
		wl_pointer = wl_seat_get_pointer(wl_seat);
		if(wl_pointer)
		{
			wl_pointer_add_listener(wl_pointer, &pointer_listener, NULL);
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] got wl_pointer: %p\n", (void*)wl_pointer);
		}
	}
	if((capabilities & WL_SEAT_CAPABILITY_KEYBOARD))
	{
		wl_keyboard = wl_seat_get_keyboard(wl_seat);
		if(wl_keyboard)
		{
			wl_keyboard_add_listener(wl_keyboard, &keyboard_listener, NULL);
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] got wl_keyboard: %p\n", (void*)wl_keyboard);
		}
	}
}

static void seat_name(void *data,
		     struct wl_seat *wl_seat,
		     const char *name)
{
	if(wl_debug)
		fprintf(stderr,"[wui-wayland] seat name %s\n",name);
}
static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};

// xdg_surface configure listener - must ack configure before commit
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	w_win_t win = (w_win_t)data;
	if(wl_debug)
		fprintf(stderr, "[wui-wayland] xdg_surface configure: win=%p, serial=%u\n", (void*)win, serial);
	if(!win)
		return;

	// Acknowledge configure
	xdg_surface_ack_configure(xdg_surface, serial);

	if (!win->configured)
	{
        win->configured = true;
        if (wl_debug)
            fprintf(stderr, "[wui-wayland] first configure received for window %p\n", (void*)win);
    }

	if(win->visible)
		w_win_redraw(win);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

// xdg_toplevel configure listener
static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
	int32_t width, int32_t height, struct wl_array *states)
{
	w_win_t win = (w_win_t)data;
	if(win->decorated && width > 2 * default_ui_style.border_width && height > 2 * default_ui_style.border_width + line_height + 2 * default_ui_style.padding_title + 1)
	{
		int title_height = line_height + 2 * default_ui_style.padding_title + 1;
		w_win_resize_internal(win, width - 2 * default_ui_style.border_width, height - 2 * default_ui_style.border_width - title_height);
	}
	else
		w_win_resize_internal(win,width,height);
	if(wl_debug)
		fprintf(stderr, "[wui-wayland] xdg_toplevel configure: win=%p, width=%d, height=%d\n", (void*)win, width, height);
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	w_win_t win = (w_win_t)data;
	if(!win)
		return;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] xdg_toplevel close: win=%p\n", (void*)win);

	// Trigger W_CLOSE event
	if(win->events[W_CLOSE].cb)
	{
		W_EVENT e = {0};
		e.type = W_CLOSE;
		win->events[W_CLOSE].cb(win, &e, win->events[W_CLOSE].data);
	}
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close,
	.configure_bounds = (void*)l_noop,
	.wm_capabilities = (void*)l_noop,
};

// zwlr_layer_surface_v1 configure listener
static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface,
	uint32_t serial, uint32_t width, uint32_t height)
{
	w_win_t win = (w_win_t)data;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] layer_surface configure: win=%p %dx%d\n", (void*)win,width,height);

	win->configured = true;
	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
	if(win->visible)
		w_win_redraw(win);
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
	w_win_t win = (w_win_t)data;
	if(!win)
		return;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] layer_surface closed: win=%p\n", (void*)win);

	// Trigger W_CLOSE event
	if(win->events[W_CLOSE].cb)
	{
		W_EVENT e = {0};
		e.type = W_CLOSE;
		win->events[W_CLOSE].cb(win, &e, win->events[W_CLOSE].data);
	}
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

// Helper function to determine role type from role string
static w_role_type_t get_role_type(const char *role,bool *decorated)
{
	if(!role)
		return W_ROLE_DEFAULT;
	if(decorated)
	{
		*decorated = strstr(role, "decorated");
	}
	if(!strcmp(role, "popup"))
		return W_ROLE_POPUP;
	if(!strcmp(role, "input"))
		return W_ROLE_INPUT;
	if(!strncmp(role, "layer",5))
		return W_ROLE_LAYER;
	return W_ROLE_DEFAULT;
}

// GSource functions for Wayland display
static gboolean wayland_prepare(GSource *source, gint *timeout)
{
	wl_display_flush(wl_display);
	*timeout = -1;
	return FALSE;
}

static gboolean wayland_check(GSource *source)
{
	GPollFD *fds = (GPollFD *)(source + 1);
	if(fds && (fds->revents & G_IO_IN))
		return TRUE;
	return FALSE;
}

static gboolean wayland_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct wl_display *display = user_data;
	if (wl_display_dispatch(display) == -1)
		 exit(-1);
	return TRUE;
}

static GSourceFuncs wayland_source_funcs = {
	.prepare = wayland_prepare,
	.check = wayland_check,
	.dispatch = wayland_dispatch,
	.finalize = NULL,
};

static int w_workarea_init(int *workarea);
int w_set_options(const W_OPTIONS *options)
{
	w_translate=options->translate;
	w_input_hook=options->input_hook;
	w_workarea_init(options->workarea);
	return 0;
}

int w_init(const char *id)
{
	if(w_initialized)
		return 0;

	w_app_id = id;

	// Initialize hash table for globals
	wl_globals_hash = L_HASH_TABLE_STRING(wl_global_info_t, interface, 64);

	// Connect to Wayland display
	wl_display = wl_display_connect(NULL);
	if(!wl_display)
	{
		fprintf(stderr, "[wui-wayland] failed to connect to Wayland display\n");
		return -1;
	}

	// Create and attach GSource to default context
	wl_gsource = g_source_new(&wayland_source_funcs, sizeof(GSource) + sizeof(GPollFD));
	if(wl_gsource)
	{
		g_source_set_name(wl_gsource, "wayland-display");

		GPollFD *poll_fd = (GPollFD *)(wl_gsource + 1);
		poll_fd->fd = wl_display_get_fd(wl_display);
		poll_fd->events = G_IO_IN | G_IO_ERR;
		poll_fd->revents = 0;
		g_source_add_poll(wl_gsource, poll_fd);

		g_source_set_callback(wl_gsource, NULL, wl_display, NULL);
		g_source_attach(wl_gsource, NULL);
	}

	// Get registry and add listener
	wl_registry = wl_display_get_registry(wl_display);
	if(wl_registry)
	{
		wl_registry_add_listener(wl_registry, &registry_listener, NULL);
		wl_display_roundtrip(wl_display);

		if(wl_debug)
			fprintf(stderr, "[wui-wayland] initialized with %d globals\n",
					l_hash_table_size(wl_globals_hash));
	}

	// Get wl_seat
	wl_global_info_t *info = l_hash_table_lookup(wl_globals_hash, "wl_seat");
	if(info)
	{
		wl_seat = wl_registry_bind(wl_registry, info->name, &wl_seat_interface, 1);
		if(wl_debug && wl_seat)
			fprintf(stderr, "[wui-wayland] got wl_seat: %p\n", (void*)wl_seat);

		// Get pointer from seat
		if(wl_seat)
		{
			wl_seat_add_listener(wl_seat,&seat_listener,NULL);
		}
	}

	// Bind essential interfaces after registry roundtrip
	// Bind shm
	info = l_hash_table_lookup(wl_globals_hash, "wl_shm");
	if(info)
	{
		wl_shm = wl_registry_bind(wl_registry, info->name, &wl_shm_interface, 1);
		if(wl_shm)
		{
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] bound wl_shm: %p\n", (void*)wl_shm);
		}
	}

	// Bind compositor
	info = l_hash_table_lookup(wl_globals_hash, "wl_compositor");
	if(info)
	{
		// for wl_surface::preferred_buffer_scale event
		wl_compositor = wl_registry_bind(wl_registry, info->name, &wl_compositor_interface, 6);
		if(wl_compositor)
		{
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] bound wl_compositor: %p\n", (void*)wl_compositor);
		}
	}

	// Bind fractional scale manager
	info = l_hash_table_lookup(wl_globals_hash, "xx_fractional_scale_manager_v2");
	if(info)
	{
		wl_fractional_scale_manager_v2 = wl_registry_bind(wl_registry, info->name,
			&xx_fractional_scale_manager_v2_interface, 1);
		if(wl_fractional_scale_manager_v2)
		{
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] bound xx_fractional_scale_manager_v2: %p\n",
					(void*)wl_fractional_scale_manager_v2);
		}
	}
	else
	{
		info = l_hash_table_lookup(wl_globals_hash, "wp_fractional_scale_manager_v1");
		if(info)
		{
			wl_fractional_scale_manager = wl_registry_bind(wl_registry, info->name,
				&wp_fractional_scale_manager_v1_interface, 1);
			if(wl_fractional_scale_manager)
			{
				if(wl_debug)
					fprintf(stderr, "[wui-wayland] bound wp_fractional_scale_manager_v1: %p\n",
						(void*)wl_fractional_scale_manager);
			}
		}
	}

	if(wl_fractional_scale_manager)
	{
		// Bind viewporter (required for fractional scale)
		info = l_hash_table_lookup(wl_globals_hash, "wp_viewporter");
		if(info)
		{
			wl_viewporter = wl_registry_bind(wl_registry, info->name,
				&wp_viewporter_interface, 1);
			if(wl_viewporter)
			{
				if(wl_debug)
					fprintf(stderr, "[wui-wayland] bound wp_viewporter: %p\n",
						(void*)wl_viewporter);
			}
		}
	}

	// Bind cursor shape manager
	info = l_hash_table_lookup(wl_globals_hash, "wp_cursor_shape_manager_v1");
	if(info)
	{
		wl_cursor_shape_manager = wl_registry_bind(wl_registry, info->name,
			&wp_cursor_shape_manager_v1_interface, 1);
		if(wl_cursor_shape_manager)
		{
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] bound wp_cursor_shape_manager_v1: %p\n",
					(void*)wl_cursor_shape_manager);
		}
	}

	// Bind system bell
	info = l_hash_table_lookup(wl_globals_hash, "xdg_system_bell_v1");
	if(info)
	{
		wl_system_bell = wl_registry_bind(wl_registry, info->name,
			&xdg_system_bell_v1_interface, 1);
		if(wl_system_bell)
		{
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] bound xdg_system_bell_v1: %p\n",
					(void*)wl_system_bell);
		}
	}

	// Bind xdg_wm_base
	info = l_hash_table_lookup(wl_globals_hash, "xdg_wm_base");
	if(info)
	{
		wl_xdg_wm_base = wl_registry_bind(wl_registry, info->name, &xdg_wm_base_interface, 1);
		if(wl_xdg_wm_base)
		{
			xdg_wm_base_add_listener(wl_xdg_wm_base, &xdg_wm_base_listener, NULL);
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] bound xdg_wm_base: %p\n", (void*)wl_xdg_wm_base);
		}
	}

	// Bind layer shell
	info = l_hash_table_lookup(wl_globals_hash, "zwlr_layer_shell_v1");
	if(info)
	{
		info->bind = wl_registry_bind(wl_registry, info->name, &zwlr_layer_shell_v1_interface, 1);
		if(wl_debug)
			fprintf(stderr, "[wui-wayland] bound zwlr_layer_shell_v1: %p\n", info->bind);
		wl_layer_shell=info->bind;
	}

	// Bind alpha modifier
	info = l_hash_table_lookup(wl_globals_hash,"wp_alpha_modifier_v1");
	if(info)
	{
		info->bind=wl_registry_bind(wl_registry, info->name, &wp_alpha_modifier_v1_interface, 1);
	}
	else
	{
		wl_tran_self=true;
	}

	// Bind background effect (blur)
	info = l_hash_table_lookup(wl_globals_hash, "ext_background_effect_manager_v1");
	if(info)
	{
		info->bind = wl_registry_bind(wl_registry, info->name,
			&ext_background_effect_manager_v1_interface, 1);
		if(info->bind)
		{
			wl_background_effect = info->bind;
			ext_background_effect_manager_v1_add_listener(wl_background_effect,
				&background_effect_manager_listener, NULL);
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] bound ext_background_effect_manager_v1: %p\n", info->bind);
		}
	}

	// Bind ext-data-control for clipboard
	info = l_hash_table_lookup(wl_globals_hash, "ext_data_control_manager_v1");
	if(info)
	{
		info->bind = wl_registry_bind(wl_registry, info->name, &ext_data_control_manager_v1_interface, 1);
		if(wl_debug)
			fprintf(stderr, "[wui-wayland] bound ext_data_control_manager_v1: %p\n", info->bind);

		// Create data control device
		if(wl_seat)
		{
			struct ext_data_control_manager_v1 *manager = info->bind;
			wl_data_control_device = ext_data_control_manager_v1_get_data_device(manager, wl_seat);
			if(wl_data_control_device)
			{
				ext_data_control_device_v1_add_listener(wl_data_control_device, &data_control_device_listener, NULL);
				if(wl_debug)
					fprintf(stderr, "[wui-wayland] got data control device: %p\n", (void*)wl_data_control_device);
			}
		}
	}

	// Bind wl_data_device_manager as fallback clipboard
	if(!wl_data_control_device)
	{
		info = l_hash_table_lookup(wl_globals_hash, "wl_data_device_manager");
		if(info)
		{
			uint32_t bind_version = info->version < 3 ? info->version : 3;
			wl_data_device_manager = wl_registry_bind(wl_registry, info->name,
				&wl_data_device_manager_interface, bind_version);
			if(wl_data_device_manager && wl_seat)
			{
				wl_data_device = wl_data_device_manager_get_data_device(wl_data_device_manager, wl_seat);
				if(wl_data_device)
				{
					wl_data_device_add_listener(wl_data_device, &data_device_listener, NULL);
					if(wl_debug)
						fprintf(stderr, "[wui-wayland] bound wl_data_device: %p\n", (void*)wl_data_device);
				}
			}
		}
	}

	wl_display_roundtrip(wl_display);

	w_workarea_update(wl_outputs);

	font_desc = pango_font_description_from_string(default_ui_style.font_name);
	if(font_desc)
	{
		pango_font_description_set_absolute_size (font_desc, default_ui_style.font_size * PANGO_SCALE);
		line_height = default_ui_style.font_size;
	}

	wl_region_empty=wl_compositor_create_region(wl_compositor);
	wl_region_add(wl_region_empty,0,0,0,0);

	w_initialized = true;
	root=w_win_create("root",wl_layer_shell?"layer":"toplevel",NULL);
	w_win_resize(root,1,1);
	return 0;
}

int w_quit(void)
{
	if(!wl_loop)
		return -1;
	g_main_loop_quit(wl_loop);
	return 0;
}

int w_loop(void)
{
	if(!w_initialized)
		return -1;

	// Internal display: run own GMainLoop (GSource already attached in w_init)
	wl_loop = g_main_loop_new(NULL, FALSE);
	if(!wl_loop)
		return -1;

	g_main_loop_run(wl_loop);
	g_main_loop_unref(wl_loop);
	wl_loop=NULL;

	// Destroy all remaining windows after loop ends
	// kwin has bug, when process exit, layer surface and input popup won't destroy
	while(wl_windows)
	{
		w_win_t win = wl_windows;
		wl_windows = win->next;
		win->next = NULL;
		w_win_destroy(win);
	}

	return 0;
}

w_win_t w_win_create(const char *title, const char *role, void *data)
{
	if(!w_initialized)
		return NULL;

	w_win_t win = (w_win_t)l_new0(struct w_window);
	win->data = data;
	win->visible = false;
	win->scale = 1.0;
	win->width = 200;
	win->height = 50;
	win->scale_num=1;
	win->scale_denom=1;

	// Create wl_surface
	win->surface = wl_compositor_create_surface(wl_compositor);
	if(!win->surface)
	{
		l_free(win);
		return NULL;
	}

	// Bind w_win to surface for direct lookup via wl_surface_get_user_data
	wl_surface_set_user_data(win->surface, win);

	// Add surface listener to track which output the window is on
	wl_surface_add_listener(win->surface, &surface_listener, win);

	// Create WUI_BUFFER
	win->buf = w_buffer_new(wl_shm);
	if(!win->buf)
	{
		wl_surface_destroy(win->surface);
		l_free(win);
		return NULL;
	}

	w_role_type_t role_type = get_role_type(role, &win->decorated);
	win->wl_info.role_type = role_type;
	
	if(title)
		win->title = l_strdup(title);

	// Setup based on role
	switch(role_type)
	{
		case W_ROLE_DEFAULT:
		{
			if(wl_xdg_wm_base && win->surface)
			{
				win->wl_info.toplevel.xdg_surface = xdg_wm_base_get_xdg_surface(wl_xdg_wm_base, win->surface);
				if(win->wl_info.toplevel.xdg_surface)
				{
					// Add xdg_surface listener
					xdg_surface_add_listener(win->wl_info.toplevel.xdg_surface, &xdg_surface_listener, win);

					win->wl_info.toplevel.xdg_toplevel = xdg_surface_get_toplevel(win->wl_info.toplevel.xdg_surface);
					if(win->wl_info.toplevel.xdg_toplevel)
					{
						// Add xdg_toplevel listener
						xdg_toplevel_add_listener(win->wl_info.toplevel.xdg_toplevel, &xdg_toplevel_listener, win);

						if(title)
							xdg_toplevel_set_title(win->wl_info.toplevel.xdg_toplevel, title);
						xdg_toplevel_set_app_id(win->wl_info.toplevel.xdg_toplevel, w_app_id);
					}
				}
			}
			break;
		}
		case W_ROLE_INPUT:
		{
			if(w_input_hook && w_input_hook->set_role)
			{
				win->wl_info.input.input_popup=w_input_hook->set_role(win->surface);
			}
			break;
		}
		case W_ROLE_LAYER:
		{
			void *layer_shell = w_wayland_get_interface("zwlr_layer_shell_v1");
			if(layer_shell && win->surface)
			{
				// win->show_hack=true;
				win->wl_info.layer.anchor=ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP|ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
				if(strstr(role,"center"))
					win->wl_info.layer.anchor=0;
			}
			break;
		}
		case W_ROLE_POPUP:
		{
			// xdg_popup: create xdg_surface, but xdg_popup is created externally
			if(wl_xdg_wm_base && win->surface)
			{
				win->wl_info.popup.xdg_surface = xdg_wm_base_get_xdg_surface(wl_xdg_wm_base, win->surface);
				if(win->wl_info.popup.xdg_surface)
				{
					// Add xdg_surface listener
					xdg_surface_add_listener(win->wl_info.popup.xdg_surface, &xdg_surface_listener, win);

					if(wl_debug)
						fprintf(stderr, "[wui-wayland] setup popup role for window %p\n", (void*)win);
				}
			}
			break;
		}
	}

	// Setup fractional scale for all surface types (if available)
	if(win->surface)
	{
		if(wl_fractional_scale_manager_v2)
		{
			win->fractional_scale_v2 = xx_fractional_scale_manager_v2_get_fractional_scale(
				wl_fractional_scale_manager_v2, win->surface);
			if(win->fractional_scale_v2)
			{
				xx_fractional_scale_v2_add_listener(win->fractional_scale_v2, &fractional_scale_v2_listener, win);
				if(wl_debug)
					fprintf(stderr, "[wui-wayland] setup fractional scale(v2) for window %p(%s role=%d)\n", win, win->title, role_type);
			}
		}
		else if(wl_fractional_scale_manager)
		{
			win->fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
				wl_fractional_scale_manager, win->surface);
			if(win->fractional_scale)
			{
				wp_fractional_scale_v1_add_listener(win->fractional_scale, &fractional_scale_listener, win);
				if(wl_debug)
					fprintf(stderr, "[wui-wayland] setup fractional scale for window %p(%s role=%d)\n", win, win->title, role_type);
			}
		}
	}

	// Setup viewport for fractional scale (destination = logical surface size)
	if(wl_viewporter && win->surface && win->fractional_scale)
	{
		win->viewport = wp_viewporter_get_viewport(wl_viewporter, win->surface);
	}


	// Add to global window list
	win->next = wl_windows;
	wl_windows = win;

	wl_display_roundtrip(wl_display);

	return win;
}

void *w_win_get_data(w_win_t win)
{
	return win ? win->data : NULL;
}

int w_win_set_title(w_win_t win, const char *title)
{
	if(!win || !title)
		return -1;

	l_free(win->title);
	win->title = l_strdup(title);

	if(win->wl_info.role_type == W_ROLE_DEFAULT && win->wl_info.toplevel.xdg_toplevel)
		xdg_toplevel_set_title(win->wl_info.toplevel.xdg_toplevel, title);

	return 0;
}

int w_win_destroy(w_win_t win)
{
	if(!win)
		return -1;

	// Trigger W_DESTROY event before cleanup
	if(win->events[W_DESTROY].cb)
	{
		W_EVENT e = {0};
		e.type = W_DESTROY;
		win->events[W_DESTROY].cb(win, &e, win->events[W_DESTROY].data);
		if(wl_debug)
			fprintf(stderr, "[wui-wayland] sent W_DESTROY to window %p\n", (void*)win);
	}

	// Clear global pointer if this window is the current pointer window
	if(wl_pointer_win == win)
		wl_pointer_win = NULL;

	// Clear global keyboard if this window is the current keyboard window
	if(wl_keyboard_win == win)
		wl_keyboard_win = NULL;

	// Remove pending idle redraw source if any
	if(win->redraw_source_id > 0)
		g_source_remove(win->redraw_source_id);

	// Remove pending config debounce timer if any
	if(win->config_debounce_id > 0)
		g_source_remove(win->config_debounce_id);

	// Remove from global window list
	w_win_t *prev = &wl_windows;
	w_win_t curr;
	for(curr = wl_windows; curr; prev = &curr->next, curr = curr->next)
	{
		if(curr == win)
		{
			*prev = win->next;
			break;
		}
	}

	// Free WUI_BUFFER
	w_buffer_free(win->buf);

	if(wl_fractional_scale_manager_v2)
	{
		if(win->fractional_scale_v2)
			xx_fractional_scale_v2_destroy(win->fractional_scale_v2);
	}
	else if(wl_fractional_scale_manager)
	{
		if(win->fractional_scale)
			wp_fractional_scale_v1_destroy(win->fractional_scale);
	}

	if(win->viewport)
		wp_viewport_destroy(win->viewport);

	if(win->alpha_modifier)
		wp_alpha_modifier_surface_v1_destroy(win->alpha_modifier);

	if(win->background_effect)
	{
		ext_background_effect_surface_v1_set_blur_region(win->background_effect, NULL);
		ext_background_effect_surface_v1_destroy(win->background_effect);
		win->background_effect = NULL;
	}

	// Destroy role-specific objects
	switch(win->wl_info.role_type)
	{
		case W_ROLE_DEFAULT:
			if(win->wl_info.toplevel.xdg_toplevel)
				xdg_toplevel_destroy(win->wl_info.toplevel.xdg_toplevel);
			if(win->wl_info.toplevel.xdg_surface)
				xdg_surface_destroy(win->wl_info.toplevel.xdg_surface);
			break;
		case W_ROLE_POPUP:
			if(win->wl_info.popup.xdg_positioner)
				xdg_positioner_destroy(win->wl_info.popup.xdg_positioner);
			if(win->wl_info.popup.xdg_popup)
				xdg_popup_destroy(win->wl_info.popup.xdg_popup);
			if(win->wl_info.popup.xdg_surface)
				xdg_surface_destroy(win->wl_info.popup.xdg_surface);
			break;
		case W_ROLE_INPUT:
			if(win->wl_info.input.input_popup)
				w_input_hook->clr_role(win->wl_info.input.input_popup);
			break;
		case W_ROLE_LAYER:
			if(win->wl_info.layer.layer_surface)
				zwlr_layer_surface_v1_destroy(win->wl_info.layer.layer_surface);
			break;
	}

	// Destroy surface
	if(win->surface)
		wl_surface_destroy(win->surface);

	wl_display_roundtrip(wl_display);

	if(win->pango_layout)
		g_object_unref(G_OBJECT(win->pango_layout));

	l_free(win->title);
	l_free(win);
	return 0;
}

double w_win_get_scale(w_win_t win)
{
	if(!win)
		win=root;
	return win->scale;
}

int w_win_move(w_win_t win, int x, int y)
{
	if(!win)
		return -1;

	win->window_x = x;
	win->window_y = y;

	if(win->wl_info.role_type == W_ROLE_LAYER && win->surface && win->wl_info.layer.layer_surface)
	{
		zwlr_layer_surface_v1_set_margin(win->wl_info.layer.layer_surface,y,0,0,x);
		wl_surface_commit(win->surface);
		return 0;
	}

	return -1;
}

int w_win_get_pos(w_win_t win,int *x,int *y)
{
	if(x)
		*x=win->window_x;
	if(y)
		*y=win->window_y;
	return 0;
}

int w_win_resize_internal(w_win_t win, int w, int h)
{
	if(!win)
		return -1;
	if(w>0) win->width=w;
	if(h>0) win->height=h;
	int border = win->decorated ? default_ui_style.border_width : 0;
	int title_height = win->decorated ? (line_height + 2 * default_ui_style.padding_title + 1) : 0;
	if(win->viewport)
		wp_viewport_set_destination(win->viewport, win->width + 2 * border, win->height + 2 * border + title_height);
	w_win_update_effect_region(win);
	return 0;
}

int w_win_resize(w_win_t win, int w, int h)
{
	if(!win || w <= 0 || h <= 0)
		return -1;
	if(!win->surface)
		return -1;
	if(win->wl_info.role_type == W_ROLE_INPUT)
	{
		if(w_input_hook && w_input_hook->set_size)
		{
			w_input_hook->set_size(win->wl_info.input.input_popup,w,h);
		}
		else
		{
			w_win_resize_internal(win,w,h);
			w_win_redraw(win);
		}
		return 0;
	}
	if(win->wl_info.role_type == W_ROLE_DEFAULT)
	{
		int border = win->decorated ? default_ui_style.border_width : 0;
		int title_height = win->decorated ? (line_height + 2 * default_ui_style.padding_title + 1) : 0;
		win->width=w;
		win->height=h;
		xdg_toplevel_set_min_size(win->wl_info.toplevel.xdg_toplevel,w + 2 * border,h + 2 * border + title_height);
		xdg_toplevel_set_max_size(win->wl_info.toplevel.xdg_toplevel,w + 2 * border,h + 2 * border + title_height);
		if(win->configured)
			wl_surface_commit(win->surface);
		return 0;
	}
	if(win->wl_info.role_type == W_ROLE_POPUP)
	{
		if(!win->wl_info.popup.xdg_positioner)
			return -1;
		xdg_positioner_set_size(win->wl_info.popup.xdg_positioner,w,h);
		xdg_popup_reposition(win->wl_info.popup.xdg_popup,win->wl_info.popup.xdg_positioner,0);
		wl_surface_commit(win->surface);
		return 0;
	}
	if(win->wl_info.role_type == W_ROLE_LAYER)
	{
		int border = win->decorated ? default_ui_style.border_width : 0;
		int title_height = win->decorated ? (line_height + 2 * default_ui_style.padding_title + 1) : 0;
		if(win->wl_info.layer.layer_surface)
			zwlr_layer_surface_v1_set_size(win->wl_info.layer.layer_surface,w + 2 * border,h + 2 * border + title_height);
		w_win_resize_internal(win,w,h);
		wl_surface_commit(win->surface);
		return 0;
	}

	return 0;
}

static int w_win_get_real_size(w_win_t win, int *w, int *h)
{
	int border = win->decorated ? default_ui_style.border_width : 0;
	int title_height = win->decorated ? (line_height + 2 * default_ui_style.padding_title + 1) : 0;
	if(w) *w=win->width + 2 * border;
	if(h) *h=win->height + 2 * border + title_height;
	return 0;
}

// 更新窗口背景特效区域（覆盖整个surface，含装饰）
static void w_win_update_effect_region(w_win_t win)
{
	if(!win || !win->background_effect || win->effect != W_EFFECT_BLUR)
		return;
	if(!wl_compositor)
		return;

	int w,h;
	w_win_get_real_size(win,&w,&h);
	if(w<=0 || h<=0)
		return;

	struct wl_region *region=wl_compositor_create_region(wl_compositor);
	if(!region)
		return;
	wl_region_add(region,0,0,w,h);
	ext_background_effect_surface_v1_set_blur_region(win->background_effect,region);
	wl_region_destroy(region);
}

int w_win_get_size(w_win_t win, int *w, int *h)
{
	if(!win)
		return -1;

	if(w) *w = win->width;
	if(h) *h = win->height;
	return 0;
}

int w_win_redraw(w_win_t win)
{
	if(!win || !win->visible)
		return -1;

	// Queue redraw for idle execution (merges multiple requests)
	if(win->redraw_source_id == 0)
		win->redraw_source_id = g_idle_add(idle_redraw_cb, win);

	return 0;
}

int w_win_connect(w_win_t win, int event, w_cb_t cb, void *data)
{
	if(event < 0 || event >= W_EVENT_MAX)
		return -1;
	if(!win)
		win=root;
	win->events[event].cb = cb;
	win->events[event].data = data;
	return 0;
}

int w_win_disconnect(w_win_t win, int event)
{
	if(!win || event < 0 || event >= W_EVENT_MAX)
		return -1;

	win->events[event].cb = NULL;
	win->events[event].data = NULL;
	return 0;
}

int w_win_show(w_win_t win)
{
	if(!win)
		return -1;

	// If already visible, do nothing
	if(win->visible)
		return 0;

	win->visible = true;
	if(win->wl_info.role_type==W_ROLE_DEFAULT || win->wl_info.role_type==W_ROLE_POPUP)
	{
		wl_surface_commit(win->surface);
		printf("commit show\n");
	}
	else if(win->wl_info.role_type==W_ROLE_LAYER)
	{
		if(!win->wl_info.layer.layer_surface)
		{
			win->wl_info.layer.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
				wl_layer_shell, win->surface, NULL,
				ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, win->title);
			if(win->wl_info.layer.layer_surface)
			{
				zwlr_layer_surface_v1_add_listener(win->wl_info.layer.layer_surface, &layer_surface_listener, win);
				zwlr_layer_surface_v1_set_anchor(win->wl_info.layer.layer_surface,win->wl_info.layer.anchor);
				if(win->wl_info.layer.anchor)
				{
					zwlr_layer_surface_v1_set_margin(win->wl_info.layer.layer_surface,win->window_y,0,0,win->window_x);
				}
				int w,h;
				w_win_get_real_size(win,&w,&h);
				zwlr_layer_surface_v1_set_size(win->wl_info.layer.layer_surface,w,h);
			}
		}
		if(!win->configured)
		{
			if(win->show_hack)
				wl_surface_set_input_region(win->surface, NULL);
			wl_surface_commit(win->surface);
		}
		else if(win->show_hack)
		{
			wl_surface_set_input_region(win->surface, NULL);
			w_win_redraw(win);
		}
	}
	else
	{
		if(!w_input_hook->configure)
		{
			win->configured=true;
			w_win_redraw(win);
		}
		else
		{
			w_input_hook->configure();
		}
	}

	return 0;
}

int w_win_hide(w_win_t win)
{
	if(!win)
		return -1;

	win->visible = false;

	if(wl_pointer_win == win)
		wl_pointer_win = NULL;

	if(wl_keyboard_win == win)
		wl_keyboard_win = NULL;

	if(win->wl_info.role_type==W_ROLE_LAYER)
	{
		if(win->show_hack)
		{
			wl_surface_set_input_region(win->surface,wl_region_empty);
			idle_redraw_cb(win);
			return 0;
		}
		else if(win->wl_info.layer.layer_surface)
		{
			zwlr_layer_surface_v1_destroy(win->wl_info.layer.layer_surface);
			win->wl_info.layer.layer_surface=NULL;
		}
	}

	// Reset configured flag so idle_redraw_cb and other operations
	// will wait for a new configure event before committing a buffer.
	// The compositor resets its configured state on unmap (NULL buffer commit).
	win->configured = false;
	win->close_pressed=false;
	win->close_hovered=false;

	// Detach surface
	wl_surface_attach(win->surface, NULL, 0, 0);
	wl_surface_commit(win->surface);
	
	return 0;
}

int w_win_set_input_region(w_win_t win, cairo_region_t *region)
{
	if(!win || !wl_compositor)
		return -1;

	// Create wl_region
	struct wl_region *wl_region = wl_compositor_create_region(wl_compositor);
	if(!wl_region)
		return -1;

	if(region)
	{
		// Get number of rectangles in cairo_region
		int n_rects = cairo_region_num_rectangles(region);

		// Add each rectangle to wl_region
		for(int i = 0; i < n_rects; i++)
		{
			cairo_rectangle_int_t rect;
			cairo_region_get_rectangle(region, i, &rect);
			wl_region_add(wl_region, rect.x, rect.y, rect.width, rect.height);
		}

		// Set input region to surface
		wl_surface_set_input_region(win->surface, wl_region);
	}
	else
	{
		// NULL means reset to default (entire surface accepts input)
		wl_surface_set_input_region(win->surface, NULL);
	}

	// Destroy wl_region after setting
	wl_region_destroy(wl_region);

	return 0;
}

#include "wui-menu.c"
int w_win_popup_menu(w_win_t win, GMenu *gmenu, GSimpleActionGroup *actions, int flags)
{
	if(wl_modal_win)
		return -1;
	W_MENU *menu = w_menu_from_gmenu(gmenu, actions, flags);
	if(!menu)
		return -1;
	g_object_unref(gmenu);
	g_object_unref(actions);
	int ret = w_popup_menu_internal(menu, win);
	if(ret!=0)
	{
		w_menu_free(menu);
	}
	return ret;
}

int w_win_set_capture(w_win_t win, bool capture)
{
	// Not directly supported in Wayland
	return -1;
}

int w_win_set_cursor(w_win_t win, int cursor)
{
	if(!win || !wl_cursor_shape_manager || !wl_seat)
		return -1;

	// Map W_CURSOR_* to wp_cursor_shape_device_v1_shape
	enum wp_cursor_shape_device_v1_shape shape;
	switch(cursor)
	{
		case W_CURSOR_DEFAULT:
			shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
			break;
		case W_CURSOR_POINTER:
			shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
			break;
		case W_CURSOR_MOVE:
			shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE;
			break;
		default:
			return -1;
	}

	// Get cursor shape device for seat
	struct wp_cursor_shape_device_v1 *device =
		wp_cursor_shape_manager_v1_get_pointer(wl_cursor_shape_manager, wl_pointer);
	if(!device)
		return -1;

	// Set cursor shape using last serial
	wp_cursor_shape_device_v1_set_shape(device, wl_last_serial, shape);

	// Destroy device (it's per-operation)
	wp_cursor_shape_device_v1_destroy(device);

	win->cursor_updated=true;

	return 0;
}

int w_win_bell(w_win_t win)
{
	if(!wl_system_bell)
		return -1;

	xdg_system_bell_v1_ring(wl_system_bell, win?win->surface:NULL);

	return 0;
}

// Wayland specific implementations
int w_wayland_set_serial(uint32_t serial)
{
	wl_last_serial = serial;
	return 0;
}

void *w_wayland_get_surface(w_win_t win)
{
	if(!win)
		return NULL;

	return (void *)win->surface;
}

void *w_wayland_get_interface(const char *interface)
{
	if(!wl_display || !wl_globals_hash || !interface)
		return NULL;

	// Lookup in hash table
	wl_global_info_t *info = l_hash_table_lookup(wl_globals_hash, interface);
	if(!info)
		return NULL;

	return info->bind;
}

void *w_wayland_get_display(void)
{
	return (void *)wl_display;
}

bool w_wayland_has_interface(const char *interface, uint32_t version, uint32_t *name)
{
	if(!wl_display || !wl_globals_hash || !interface)
		return false;
	wl_global_info_t *info = l_hash_table_lookup(wl_globals_hash, interface);
	if(!info)
		return false;
	if(info->version < version)
		return false;
	if(name)
		*name = info->name;
	return true;
}

// Screen/Output related APIs

// Get the output a window is currently on
w_output_t *w_win_get_output(w_win_t win)
{
	if(win && win->output)
		return win->output;
	return wl_outputs;
}

int w_win_get_output_size(w_win_t win,int *w,int *h)
{
	w_output_t *o=w_win_get_output(win);
	if(w)
		*w=(int)(o->width/win->scale);
	if(h)
		*h=(int)(o->height/win->scale);
	return 0;
}

#include "wui-workarea.c"

int w_win_get_workarea(w_win_t win, int *x, int *y, int *w, int *h)
{
	w_output_t *output = win ? win->output : NULL;
	if(!output) output=wl_outputs;
	return w_workarea_fallback(x, y, w, h, output);
}

int w_win_get_dpi(w_win_t win)
{
	(void)win;
	return 96;
}

#define WUI_BACKEND_WAYLAND
#include "wui-toast.c"

#include "wui-alert.c"

int w_win_effect(w_win_t win,int effect)
{
	if(!win || !win->surface)
		return -1;
	if(effect<0)
		return win->effect;

	// 关闭特效
	if(effect==W_EFFECT_NONE)
	{
		if(win->background_effect)
		{
			ext_background_effect_surface_v1_set_blur_region(win->background_effect,NULL);
			ext_background_effect_surface_v1_destroy(win->background_effect);
			win->background_effect=NULL;
			wl_surface_commit(win->surface);
		}
		win->effect=W_EFFECT_NONE;
		return 0;
	}

	if(effect!=W_EFFECT_BLUR)
		return -1;

	// compositor不支持背景模糊
	if(!wl_background_effect ||
		!(wl_background_effect_caps & EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR))
		return -1;

	if(!win->background_effect)
	{
		win->background_effect=ext_background_effect_manager_v1_get_background_effect(
			wl_background_effect,win->surface);
		if(!win->background_effect)
			return -1;
	}
	win->effect=effect;
	w_win_update_effect_region(win);
	wl_surface_commit(win->surface);

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] window %p(%s) effect=%d\n",
			(void*)win,win->title,effect);

	return 0;
}

int w_win_tran(w_win_t win,int tran)
{
	if(tran<0)
		return win->tran;
	if(wl_tran_self)
	{
		win->tran=tran;
		w_win_redraw(win);
		return 0;
	}
	if(!win->alpha_modifier)
	{
		if(tran==0)
			return 0;
		void *alpha_modifier=w_wayland_get_interface("wp_alpha_modifier_v1");
		if(!alpha_modifier)
			return -1;
		win->alpha_modifier=wp_alpha_modifier_v1_get_surface(alpha_modifier,win->surface);
		if(!win->alpha_modifier)
			return -1;
	}
	uint32_t factor=UINT32_MAX;
	if(tran!=0)
	{
		factor=UINT32_MAX/255*(255-tran);
	}
	wp_alpha_modifier_surface_v1_set_multiplier(win->alpha_modifier,factor);
	wl_surface_commit(win->surface);
	win->tran=tran;
	return 0;
}

int w_win_preferred_size(w_win_t win,int *w,int *h)
{
	if(!wl_fractional_scale_manager && !wl_fractional_scale_manager_v2)
		return 0;
	if(win->scale == (int)win->scale)
		return 0;
	int ox=0,oy=0;
	if(win->decorated)
	{
		int border=default_ui_style.border_width;
		int title_height = line_height + 2 * default_ui_style.padding_title + 1;
		ox = border * 2;
		oy = border *2 + title_height;
	}
	if(w)
	{
		for(int i=0;i<12;i++)
		{
			int preferred=*w+ox+i;
			if(((preferred*win->scale_num)%win->scale_denom)==0)
			{
				*w=preferred-ox;
				break;
			}
		}
	}
	if(h)
	{
		for(int i=0;i<12;i++)
		{
			int preferred=*h+oy+i;
			if(((preferred*win->scale_num)%win->scale_denom)==0)
			{
				*h=preferred-oy;
				break;
			}
		}
	}
	return 0;
}

int w_get_n_outputs(void)
{
	return l_slist_length(wl_outputs);
}

