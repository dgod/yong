#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <dlfcn.h>

#include "llib.h"
#include "wui.h"

struct w_event_item {
	w_cb_t cb;
	void *data;
};

static GdkDisplay *gdk_display = NULL;
static w_output_t current_output;  // Single output for return (updated on each call)
static bool w_initialized = false;
static const char *w_app_id = NULL;
static struct w_event_item root_config_event;
static W_INPUT_HOOK *w_input_hook;

static GtkClipboard *w_clipboard = NULL;
static char *w_clipboard_mime_data = NULL;
static int w_clipboard_mime_size = 0;
static char *w_clipboard_mime_type = NULL;
static char w_clipboard_text[512];
static gint (*p_gdk_screen_get_monitor_scale_factor)(GdkScreen* screen,gint monitor_num);
static gint (*p_gtk_widget_get_scale_factor)(GtkWidget* widget);

int w_workarea_update(w_output_t *output);
int w_workarea_fallback(int *x, int *y, int *w, int *h, w_output_t *output);

struct w_window {
	GtkWidget *widget;
	void *data;
	struct w_event_item events[W_EVENT_MAX];
	bool visible;
	bool decorated;
	double scale;
	int mouse_x;
	int mouse_y;
	int x,y;

	uint8_t *surface_data;
	int surface_size;

	bool pressed;
};

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, w_win_t win)
{
	if(!win->events[W_DRAW].cb)
		return TRUE;

	cairo_surface_t *target=cairo_get_target(cr);
	int type=cairo_surface_get_type(target);
	if(type==CAIRO_SURFACE_TYPE_IMAGE)
	{
		W_EVENT e;
		e.type = W_DRAW;
		e.draw.cr = cr;

		win->events[W_DRAW].cb(win, &e, win->events[W_DRAW].data);
	}
	else if(type==CAIRO_SURFACE_TYPE_XLIB)
	{
		int w=cairo_xlib_surface_get_width(target);
		int h=cairo_xlib_surface_get_height(target);
		if(w*h*4>win->surface_size)
		{
			win->surface_size=(w*h*4+0xffff)&~0xffff;
			win->surface_data=realloc(win->surface_data,win->surface_size);
		}
		memset(win->surface_data,0,w*h*4);
		cairo_surface_t *surface=cairo_image_surface_create_for_data(win->surface_data,CAIRO_FORMAT_ARGB32,w,h,w*4);
		double sx, sy;
		cairo_surface_get_device_scale(target, &sx, &sy);
		cairo_t *cr2=cairo_create(surface);
		cairo_scale(cr2,sx,sy);

		W_EVENT e;
		e.type = W_DRAW;
		e.draw.cr = cr2;

		win->events[W_DRAW].cb(win, &e, win->events[W_DRAW].data);

		cairo_destroy(cr2);

		cairo_surface_set_device_scale(surface,sx,sy);
		cairo_set_source_surface(cr, surface, 0, 0);
		cairo_paint(cr);

		cairo_surface_destroy(surface);
	}
	return TRUE;
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, w_win_t win)
{
	if(!win->events[W_MOUSE_DOWN].cb)
		return FALSE;

	// 避免发出重复按下事件
	if(win->pressed)
		return TRUE;
	win->pressed=true;

	win->mouse_x = (int)event->x;
	win->mouse_y = (int)event->y;

	W_EVENT e;
	e.type = W_MOUSE_DOWN;
	e.mouse.x = (int)event->x;
	e.mouse.y = (int)event->y;
	e.mouse.button = event->button == 1 ? W_BUTTON_LEFT :
	                 event->button == 3 ? W_BUTTON_RIGHT : W_BUTTON_MIDDLE;

	win->events[W_MOUSE_DOWN].cb(win, &e, win->events[W_MOUSE_DOWN].data);
	return TRUE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, w_win_t win)
{
	if(!win->events[W_MOUSE_UP].cb)
		return FALSE;

	win->pressed=false;

	W_EVENT e;
	e.type = W_MOUSE_UP;
	e.mouse.x = (int)event->x;
	e.mouse.y = (int)event->y;
	e.mouse.button = event->button == 1 ? W_BUTTON_LEFT :
	                 event->button == 3 ? W_BUTTON_RIGHT : W_BUTTON_MIDDLE;

	win->events[W_MOUSE_UP].cb(win, &e, win->events[W_MOUSE_UP].data);
	return TRUE;
}

static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, w_win_t win)
{
	if(!win->events[W_MOUSE_MOVE].cb)
		return FALSE;

	W_EVENT e;
	e.type = W_MOUSE_MOVE;
	e.mouse.x = (int)event->x;
	e.mouse.y = (int)event->y;
	e.mouse.button = 0;

	win->events[W_MOUSE_MOVE].cb(win, &e, win->events[W_MOUSE_MOVE].data);
	return TRUE;
}

static gboolean on_enter_notify(GtkWidget *widget, GdkEventCrossing *event, w_win_t win)
{
	if(!win->events[W_MOUSE_ENTER].cb)
		return FALSE;

	W_EVENT e;
	e.type = W_MOUSE_ENTER;
	e.mouse.x = (int)event->x;
	e.mouse.y = (int)event->y;
	e.mouse.button = 0;

	win->events[W_MOUSE_ENTER].cb(win, &e, win->events[W_MOUSE_ENTER].data);
	return TRUE;
}

static gboolean on_leave_notify(GtkWidget *widget, GdkEventCrossing *event, w_win_t win)
{
	if(!win->events[W_MOUSE_LEAVE].cb)
		return FALSE;

	win->pressed=false;

	W_EVENT e;
	e.type = W_MOUSE_LEAVE;
	e.mouse.x = (int)event->x;
	e.mouse.y = (int)event->y;
	e.mouse.button = 0;

	win->events[W_MOUSE_LEAVE].cb(win, &e, win->events[W_MOUSE_LEAVE].data);
	return TRUE;
}

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event, w_win_t win)
{
	if(!win->events[W_SCROLL].cb)
		return FALSE;

	W_EVENT e;
	e.type = W_SCROLL;
	e.scroll.x = (int)event->x;
	e.scroll.y = (int)event->y;
	e.scroll.button = 0;

	/* support smooth scrolling */
	if(event->direction == GDK_SCROLL_SMOOTH)
	{
		e.scroll.dx = event->delta_x;
		e.scroll.dy = event->delta_y;
	}
	else
	{
		e.scroll.dx = event->direction == GDK_SCROLL_LEFT ? -1.0 :
	              event->direction == GDK_SCROLL_RIGHT ? 1.0 : 0.0;
		e.scroll.dy = event->direction == GDK_SCROLL_UP ? -1.0 :
	              event->direction == GDK_SCROLL_DOWN ? 1.0 : 0.0;
	}

	win->events[W_SCROLL].cb(win, &e, win->events[W_SCROLL].data);
	return TRUE;
}

static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, w_win_t win)
{
	if(win->events[W_CLOSE].cb)
	{
		W_EVENT e;
		e.type = W_CLOSE;
		win->events[W_CLOSE].cb(win, &e, win->events[W_CLOSE].data);
		return TRUE;
	}
	return FALSE;
}

static void on_destroy(GtkWidget *widget, w_win_t win)
{
	win->visible = false;

	// Fire W_DESTROY event (e.g. menu_destroy_cb for menu popups)
	if(win->events[W_DESTROY].cb)
	{
		W_EVENT e = {0};
		e.type = W_DESTROY;
		win->events[W_DESTROY].cb(win, &e, win->events[W_DESTROY].data);
	}
}

static void fill_output_from_monitor(w_output_t *out,w_win_t win)
{
	if(!out)
		return;

	GdkScreen *screen=gdk_display_get_default_screen(gdk_display);
	int monitor=0;
	if(win)
		monitor=gdk_screen_get_monitor_at_window(screen,gtk_widget_get_window(win->widget));
	if(gdk_screen_get_n_monitors(screen)==0)
	{
		out->name=0;
		out->x=0;
		out->y=0;
		out->width=1920;
		out->height=1080;
		out->scale=1;
		out->done=true;
		return;
	}
	// gtk have scaled for us, just treat it as scale=1
	int scale=1;
	//if(p_gdk_screen_get_monitor_scale_factor)
	//	scale=p_gdk_screen_get_monitor_scale_factor(screen,monitor);

	GdkRectangle rc;
	gdk_screen_get_monitor_geometry(screen,monitor,&rc);

	out->name=monitor;
	out->x = rc.x;
	out->y = rc.y;
	out->width = rc.width/scale;
	out->height = rc.height/scale;
	out->scale = scale;
	out->done = true;
}

static guint screen_changed_timer=0;
static gboolean on_screen_size_changed_next(void *unused)
{
	w_workarea_update(w_win_get_output(NULL));
	screen_changed_timer=0;
	W_EVENT e = {0};
	e.type = W_CONFIG;
	root_config_event.cb(NULL,&e,root_config_event.data);
	return G_SOURCE_REMOVE;
}

static void on_screen_size_changed(void *unused)
{
	if(!root_config_event.cb)
		return;
	if(screen_changed_timer)
		g_source_remove(screen_changed_timer);
	screen_changed_timer=g_timeout_add(100,(GSourceFunc)on_screen_size_changed_next,NULL);
}

static int w_workarea_init(int *workarea);
int w_set_options(const W_OPTIONS *options)
{
	w_input_hook=options->input_hook;
	w_workarea_init(options->workarea);
	return 0;
}

static void on_get_select_cb(GtkClipboard* clipboard,const gchar* text)
{
	if(!text || !text[0] || strlen(text)>=sizeof(w_clipboard_text))
	{
		w_clipboard_text[0]=0;
	}
	else
	{
		strcpy(w_clipboard_text,text);
	}
}

static void on_clipboard_owner_change (GtkClipboard* clipboard)
{
	gtk_clipboard_request_text(clipboard,(GtkClipboardTextReceivedFunc)on_get_select_cb,NULL);
}

int w_init(const char *id)
{
	if(w_initialized)
		return 0;

	w_app_id = id ? id : "wui";
	(void)w_app_id;

	g_setenv("GTK_CSD","0",TRUE);
	g_setenv("GDK_BACKEND", "x11", TRUE);
	gtk_init(NULL, NULL);

	gdk_display = gdk_display_get_default();

	// Verify we're using X11 backend
	if(!GDK_IS_X11_DISPLAY(gdk_display))
	{
		fprintf(stderr, "[wui-gtk3-x11] not using X11 backend, initialization failed\n");
		return -1;
	}

	w_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	g_signal_connect(w_clipboard,"owner-change",G_CALLBACK(on_clipboard_owner_change),NULL);

	p_gdk_screen_get_monitor_scale_factor=dlsym(NULL,"gdk_screen_get_monitor_scale_factor");
	p_gtk_widget_get_scale_factor=dlsym(NULL,"gtk_widget_get_scale_factor");

	w_workarea_update(w_win_get_output(NULL));
	
	GtkSettings *settings = gtk_settings_get_default();
	g_object_set(settings, "gtk-dialogs-use-header", TRUE, NULL);
	g_object_set(settings, "gtk-enable-animations", FALSE, NULL);

	GdkScreen *screen = gdk_screen_get_default();
	g_signal_connect(G_OBJECT(screen),"size-changed",
		G_CALLBACK(on_screen_size_changed),NULL);
	g_signal_connect(G_OBJECT(screen),"notify::resolution",
		G_CALLBACK(on_screen_size_changed),NULL);

	w_initialized = true;

	return 0;
}

int w_quit(void)
{
	gtk_main_quit();
	return 0;
}

int w_loop(void)
{
	gtk_main();
	return 0;
}

static gboolean on_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	w_win_t win = data;
	if(win->decorated)
		w_win_get_pos(win,&win->x,&win->y);
	return false;
}

w_win_t w_win_create(const char *title, const char *role, void *data)
{
	w_win_t win = (w_win_t)l_new0(struct w_window);

	win->widget = gtk_window_new(GTK_WINDOW_POPUP);
	win->data = data;
	win->scale = 1.0;
	win->visible = false;
	win->decorated = true;

	gtk_window_set_title(GTK_WINDOW(win->widget), title ? title : "");
	gtk_window_set_resizable(GTK_WINDOW(win->widget),FALSE);
	if(role && !strstr(role,"decorated"))
	{
		gtk_window_set_decorated(GTK_WINDOW(win->widget), FALSE);
		win->decorated = false;
	}
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win->widget), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(win->widget), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(win->widget), TRUE);
	gtk_window_set_modal(GTK_WINDOW(win->widget),FALSE);
	gtk_window_set_accept_focus(GTK_WINDOW(win->widget), FALSE);
	if(strstr(role,"center"))
		gtk_window_set_position(GTK_WINDOW(win->widget),GTK_WIN_POS_CENTER);

	gtk_widget_set_app_paintable(win->widget, TRUE);
	GdkScreen *screen = gdk_screen_get_default();
	GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
	if(visual)
		gtk_widget_set_visual(win->widget, visual);

	gtk_widget_realize(win->widget);

	gdk_window_set_functions(gtk_widget_get_window(win->widget),GDK_FUNC_MOVE|GDK_FUNC_MINIMIZE|GDK_FUNC_CLOSE);

	gdk_window_set_events(gtk_widget_get_window(win->widget),
		gdk_window_get_events(gtk_widget_get_window(win->widget)) |
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
		GDK_SMOOTH_SCROLL_MASK | GDK_STRUCTURE_MASK);

	g_signal_connect(win->widget, "draw", G_CALLBACK(on_draw), win);
	g_signal_connect(win->widget, "button-press-event", G_CALLBACK(on_button_press), win);
	g_signal_connect(win->widget, "button-release-event", G_CALLBACK(on_button_release), win);
	g_signal_connect(win->widget, "motion-notify-event", G_CALLBACK(on_motion_notify), win);
	g_signal_connect(win->widget, "enter-notify-event", G_CALLBACK(on_enter_notify), win);
	g_signal_connect(win->widget, "leave-notify-event", G_CALLBACK(on_leave_notify), win);
	g_signal_connect(win->widget, "scroll-event", G_CALLBACK(on_scroll_event), win);
	g_signal_connect(win->widget, "delete-event", G_CALLBACK(on_delete_event), win);
	g_signal_connect(win->widget, "destroy", G_CALLBACK(on_destroy), win);

	if(win->decorated)
	{
		g_signal_connect(win->widget,"configure-event",G_CALLBACK(on_configure_event),win);
	}

	return win;
}

void *w_win_get_data(w_win_t win)
{
	return win ? win->data : NULL;
}

int w_win_set_title(w_win_t win, const char *title)
{
	if(!win)
		return -1;
	gtk_window_set_title(GTK_WINDOW(win->widget), title ? title : "");
	return 0;
}

int w_win_destroy(w_win_t win)
{
	if(!win)
		return -1;
	gtk_widget_destroy(win->widget);
	l_free(win->surface_data);
	l_free(win);
	return 0;
}

double w_win_get_scale(w_win_t win)
{
	if(!p_gtk_widget_get_scale_factor)
		return 1.0;
	if(!win)
	{
		GdkScreen *screen=gdk_screen_get_default();
		return p_gdk_screen_get_monitor_scale_factor(screen,0);
	}
	return p_gtk_widget_get_scale_factor(win->widget);
}

int w_win_move(w_win_t win, int x, int y)
{
	if(!win)
		return -1;
	gtk_window_move(GTK_WINDOW(win->widget), x, y);
	win->x=x;
	win->y=y;
	return 0;
}

int w_win_get_pos(w_win_t win,int *x,int *y)
{
	gtk_window_get_position(GTK_WINDOW(win->widget),x,y);
	return 0;
}

int w_win_resize_internal(w_win_t win, int w, int h)
{
	if(!win)
		return -1;
	gtk_widget_set_size_request(win->widget,w,h);
	return 0;
}

int w_win_resize(w_win_t win, int w, int h)
{
	if(!win)
		return -1;
	gtk_widget_set_size_request(win->widget,w,h);
	return 0;
}

int w_win_get_size(w_win_t win, int *w, int *h)
{
	if(!win)
		return -1;
	gtk_window_get_size(GTK_WINDOW(win->widget), w, h);
	return 0;
}

int w_win_redraw(w_win_t win)
{
	if(!win)
		return -1;
	gtk_widget_queue_draw(win->widget);
	return 0;
}

int w_win_connect(w_win_t win, int event, w_cb_t cb, void *data)
{
	if(win && event >= 0 && event < W_EVENT_MAX)
	{
		win->events[event].cb = cb;
		win->events[event].data = data;
		return 0;
	}
	if(!win && event==W_CONFIG)
	{
		root_config_event.cb=cb;
		root_config_event.data=data;
		return 0;
	}
	return -1;
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
	gtk_widget_show(win->widget);
	win->visible = true;
	// when win hide, gtk not update the window real pos, so move it again here
	w_win_move(win,win->x,win->y);
	return 0;
}

int w_win_hide(w_win_t win)
{
	if(!win)
		return -1;
	if(win->decorated)
		w_win_get_pos(win,&win->x,&win->y);
	gtk_widget_hide(win->widget);
	win->visible = false;
	return 0;
}

int w_win_set_input_region(w_win_t win, cairo_region_t *region)
{
	if(!win)
		return -1;
	gtk_widget_input_shape_combine_region(win->widget, region);
	return 0;
}

#define W_MENU_COLUMN2		1
#define W_MENU_COLUMN2_MIN	10

static void menu_on_done(GtkMenuShell *menushell,void *user_data)
{
	gtk_widget_destroy(GTK_WIDGET(menushell));
}

// Recursively count ALL items (including separators, sections expanding) in a model.
static int menu_count_items(GMenuModel *model, bool *is_first)
{
	int n = g_menu_model_get_n_items(model);
	int total = 0;

	for(int i = 0; i < n; i++)
	{
		GMenuModel *section = g_menu_model_get_item_link(model, i, G_MENU_LINK_SECTION);
		if(section)
		{
			if(!*is_first)
				total++;		// section separator
			total += menu_count_items(section, is_first);
			g_object_unref(section);
			*is_first = false;
			continue;
		}

		total++;
		*is_first = false;
	}
	return total;
}

// Build GtkMenuItem widgets from a GMenuModel and place them in `menu` via
// gtk_menu_attach().  When `use_column2` is true and total >= W_MENU_COLUMN2_MIN,
// items are split: first split_at go to column 0, remainder to column 1.
// `io_idx` / `io_row` track the flat position and per-column row counters.
static void menu_build_items(GMenuModel *model, GSimpleActionGroup *actions,
	GtkWidget *menu, bool use_column2, int split_at,
	int *io_idx, int io_row[2])
{
	int n = g_menu_model_get_n_items(model);
	bool first = true;

	for(int i = 0; i < n; i++)
	{
		// --- Section link: recurse into it ---
		GMenuModel *section = g_menu_model_get_item_link(model, i, G_MENU_LINK_SECTION);
		if(section)
		{
			if(!first)
			{
				GtkWidget *sep = gtk_separator_menu_item_new();
				int col = use_column2 ? (*io_idx < split_at ? 0 : 1) : 0;
				gtk_menu_attach(GTK_MENU(menu), sep, col, col + 1,
					io_row[col], io_row[col] + 1);
				gtk_widget_show(sep);
				io_row[col]++;
				(*io_idx)++;
			}
			menu_build_items(section, actions, menu, use_column2, split_at,
				io_idx, io_row);
			g_object_unref(section);
			first = false;
			continue;
		}

		first = false;

		// --- Read label ---
		GVariant *label_var = g_menu_model_get_item_attribute_value(model, i,
			G_MENU_ATTRIBUTE_LABEL, G_VARIANT_TYPE_STRING);
		const char *label = label_var ? g_variant_get_string(label_var, NULL) : NULL;

		// Empty / no label → separator
		if(!label || label[0] == '\0')
		{
			GtkWidget *sep = gtk_separator_menu_item_new();
			int col = use_column2 ? (*io_idx < split_at ? 0 : 1) : 0;
			gtk_menu_attach(GTK_MENU(menu), sep, col, col + 1,
				io_row[col], io_row[col] + 1);
			gtk_widget_show(sep);
			io_row[col]++;
			(*io_idx)++;
			if(label_var) g_variant_unref(label_var);
			continue;
		}

		// --- Submenu link ---
		GMenuModel *sub_model = g_menu_model_get_item_link(model, i, G_MENU_LINK_SUBMENU);
		if(sub_model)
		{
			GtkWidget *item = gtk_menu_item_new_with_label(label);

			// Count sub-items and decide its own two-column mode
			bool sub_first = true;
			int sub_total = menu_count_items(sub_model, &sub_first);
			bool sub_col2 = use_column2 && sub_total >= W_MENU_COLUMN2_MIN;
			int sub_split = sub_col2 ? (sub_total + 1) / 2 : 0;

			GtkWidget *submenu = gtk_menu_new();
			int sub_idx = 0;
			int sub_row[2] = {0, 0};
			menu_build_items(sub_model, actions, submenu, sub_col2, sub_split,
				&sub_idx, sub_row);

			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

			int col = use_column2 ? (*io_idx < split_at ? 0 : 1) : 0;
			gtk_menu_attach(GTK_MENU(menu), item, col, col + 1,
				io_row[col], io_row[col] + 1);
			gtk_widget_show(submenu);
			gtk_widget_show(item);
			io_row[col]++;
			(*io_idx)++;
			g_object_unref(sub_model);
			if(label_var) g_variant_unref(label_var);
			continue;
		}

		// --- Leaf item: read action & target ---
		GVariant *action_var = g_menu_model_get_item_attribute_value(model, i,
			G_MENU_ATTRIBUTE_ACTION, G_VARIANT_TYPE_STRING);
		const char *action_name = action_var ? g_variant_get_string(action_var, NULL) : NULL;

		GVariant *target_var = g_menu_model_get_item_attribute_value(model, i,
			G_MENU_ATTRIBUTE_TARGET, NULL);

		// Determine check/radio from action state
		bool is_check = false;
		bool is_checked = false;

		if(actions && action_name)
		{
			const char *act = action_name;
			if(strncmp(act, "app.", 4) == 0)
				act += 4;

			GAction *action = g_action_map_lookup_action(G_ACTION_MAP(actions), act);
			if(action)
			{
				const GVariantType *st = g_action_get_state_type(action);
				GVariant *state = g_action_get_state(action);
				if(state)
				{
					if(st && g_variant_type_equal(st, G_VARIANT_TYPE_BOOLEAN))
					{
						is_check = true;
						is_checked = g_variant_get_boolean(state);
					}
					else if(target_var)
					{
						is_check = true;
						is_checked = g_variant_equal(state, target_var);
					}
					g_variant_unref(state);
				}
			}
		}

		GtkWidget *item;
		if(is_check)
		{
			item = gtk_check_menu_item_new_with_label(label);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), is_checked);
		}
		else
		{
			item = gtk_menu_item_new_with_label(label);
		}

		if(action_name)
		{
			gtk_actionable_set_action_name(GTK_ACTIONABLE(item), action_name);
			if(target_var)
				gtk_actionable_set_action_target_value(GTK_ACTIONABLE(item), target_var);
		}

		int col = use_column2 ? (*io_idx < split_at ? 0 : 1) : 0;
		gtk_menu_attach(GTK_MENU(menu), item, col, col + 1,
			io_row[col], io_row[col] + 1);
		gtk_widget_show(item);
		io_row[col]++;
		(*io_idx)++;

		if(label_var) g_variant_unref(label_var);
		if(action_var) g_variant_unref(action_var);
		if(target_var) g_variant_unref(target_var);
	}
}

int w_win_popup_menu(w_win_t win, GMenu *menu, GSimpleActionGroup *actions, int flags)
{
	if(!win || !menu)
		return -1;

	// Count total flat items
	bool first = true;
	int total = menu_count_items(G_MENU_MODEL(menu), &first);

	bool use_column2 = (flags & W_MENU_COLUMN2) && total >= W_MENU_COLUMN2_MIN;
	int split_at = use_column2 ? (total + 1) / 2 : 0;

	GtkWidget *widget = gtk_menu_new();
	int idx = 0;
	int row[2] = {0, 0};
	menu_build_items(G_MENU_MODEL(menu), actions, widget,
		use_column2, split_at, &idx, row);

	if(actions)
	{
		gtk_widget_insert_action_group(widget, "app", G_ACTION_GROUP(actions));
		g_object_unref(actions);
	}
	g_signal_connect(G_OBJECT(widget),"selection-done",(void*)menu_on_done,NULL);
	gtk_menu_popup(GTK_MENU(widget),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
	g_object_unref(menu);
	return 0;
}

int w_win_set_capture(w_win_t win, bool capture)
{
	if(!win)
		return -1;

	if(capture)
		gtk_grab_add(win->widget);
	else
		gtk_grab_remove(win->widget);

	return 0;
}

int w_win_set_cursor(w_win_t win, int cursor)
{
	if(!win)
		return -1;

	GdkCursorType type;
	switch(cursor)
	{
		case W_CURSOR_DEFAULT: type = GDK_LEFT_PTR; break;
		case W_CURSOR_POINTER: type = GDK_HAND1; break;
		case W_CURSOR_MOVE: type = GDK_FLEUR; break;
		default: type = GDK_LEFT_PTR; break;
	}

	GdkCursor *gdk_cursor = gdk_cursor_new(type);
	gdk_window_set_cursor(gtk_widget_get_window(win->widget), gdk_cursor);
	g_object_unref(gdk_cursor);

	return 0;
}

int w_win_bell(w_win_t win)
{
	if(!win)
		return -1;
	GdkWindow *window = gtk_widget_get_window(win->widget);
	if(window)
		gdk_window_beep(window);
	return 0;
}

char *w_clipboard_get_text(void)
{
	if(!w_clipboard)
		return NULL;
	if(!w_clipboard_text[0])
		return NULL;
	return l_strdup(w_clipboard_text);
}

int w_clipboard_set_text(const char *text)
{
	if(!w_clipboard || !text)
		return -1;
	gtk_clipboard_set_text(w_clipboard, text, -1);
	return 0;
}

// Clipboard get callback: called when paste target requests the MIME data
static void on_clipboard_get(GtkClipboard *clipboard, GtkSelectionData *selection_data,
                             guint info, gpointer user_data_or_owner)
{
	if(!w_clipboard_mime_data || !w_clipboard_mime_type || w_clipboard_mime_size <= 0)
		return;

	gtk_selection_data_set(selection_data,
		gdk_atom_intern(w_clipboard_mime_type, FALSE),
		8, (guchar*)w_clipboard_mime_data, w_clipboard_mime_size);
}

// Clipboard clear callback: called when clipboard ownership is lost
static void on_clipboard_clear(GtkClipboard *clipboard, gpointer user_data_or_owner)
{
	g_free(w_clipboard_mime_data);
	w_clipboard_mime_data = NULL;
	w_clipboard_mime_size = 0;
	g_free(w_clipboard_mime_type);
	w_clipboard_mime_type = NULL;
}

int w_clipboard_set_mime(const char *mime, const void *data, int size)
{
	if(!w_clipboard || !mime || !data || size <= 0)
		return -1;

	// Allocate new data locally first (don't touch globals yet)
	char *new_data = g_memdup2(data, size);
	if(!new_data)
		return -1;
	char *new_type = g_strdup(mime);
	if(!new_type)
	{
		g_free(new_data);
		return -1;
	}

	// Register target with GTK clipboard FIRST.
	// This triggers the old clear_func which frees old globals --- safe
	// because we haven't put new data into globals yet.
	GtkTargetEntry targets[] = {
		{ (char*)mime, 0, 0 }
	};

	if(!gtk_clipboard_set_with_data(w_clipboard, targets, G_N_ELEMENTS(targets),
		on_clipboard_get, on_clipboard_clear, NULL))
	{
		g_free(new_data);
		g_free(new_type);
		// Old globals were already freed by clear_func; ensure NULL
		w_clipboard_mime_data = NULL;
		w_clipboard_mime_size = 0;
		w_clipboard_mime_type = NULL;
		return -1;
	}

	if(w_clipboard_mime_type)
		g_free(w_clipboard_mime_type);
	if(w_clipboard_mime_data)
		g_free(w_clipboard_mime_data);

	// Success --- old globals freed by clear_func. Now store new data.
	w_clipboard_mime_data = new_data;
	w_clipboard_mime_size = size;
	w_clipboard_mime_type = new_type;

	return 0;
}

// Wayland stubs for X11 backend
int w_wayland_set_serial(uint32_t serial)
{
	return -1;
}

void *w_wayland_get_surface(w_win_t win)
{
	return NULL;
}

void *w_wayland_get_interface(const char *interface)
{
	return NULL;
}

void *w_wayland_get_display(void)
{
	return NULL;
}

bool w_wayland_has_interface(const char *interface, uint32_t version, uint32_t *name)
{
	return false;
}

// Output APIs
w_output_t *w_win_get_output(w_win_t win)
{
	if(!gdk_display)
		return NULL;
	fill_output_from_monitor(&current_output,win);
	return &current_output;
}

#include "wui-workarea.c"

// Get workarea for window's monitor
int w_win_get_workarea(w_win_t win, int *x, int *y, int *w, int *h)
{
	w_output_t *output = w_win_get_output(win);
	return w_workarea_fallback(x, y, w, h, output);
}


int w_win_get_dpi(w_win_t win)
{
	GdkScreen *scr=win?gtk_widget_get_screen(win->widget):gdk_screen_get_default();
	int dpi=(int)gdk_screen_get_resolution(scr);
	if(dpi<=0) dpi=96;
	return dpi;
}

static void on_dialog_mapped(GtkWidget *widget, gpointer user_data) {
    // 窗口已经完全显示，此时请求焦点万无一失
    guint32 event_time = gtk_get_current_event_time();
    if (event_time == 0) {
        event_time = GDK_CURRENT_TIME;
    }
    gtk_window_present_with_time(GTK_WINDOW(widget), event_time);
}

int w_win_alert(w_win_t win,const char *title,const char *text)
{
	// Don't use the input window as parent — it may be hidden
	// (undecorated popup) and a transient dialog for a hidden
	// parent can flash and immediately disappear on some WMs.
	GtkWidget *dlg=gtk_message_dialog_new(
		NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		"%s",
		text);
	g_signal_connect(dlg, "map", G_CALLBACK(on_dialog_mapped), NULL);
	gtk_window_set_title(GTK_WINDOW(dlg),title);
	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
	return 0;
}

#include "wui-toast.c"

int w_win_tran(w_win_t win,int tran)
{
	if(tran<0)
	{
		double val=gtk_window_get_opacity(GTK_WINDOW(win->widget));
		return (int)round((1-val)*255);
	}
	double val=(255.0-tran)/255.0;
	gtk_widget_set_opacity(win->widget,val);
	return 0;
}

int w_win_get_output_size(w_win_t win,int *w,int *h)
{
	w_output_t *o=w_win_get_output(win);
	if(w)
		*w=(int)(o->width);
	if(h)
		*h=(int)(o->height);
	return 0;
}

int w_win_preferred_size(w_win_t win,int *w,int *h)
{
	return 0;
}

int w_get_n_outputs(void)
{
	GdkScreen *screen=gdk_display_get_default_screen(gdk_display);
	return gdk_screen_get_n_monitors(screen);
}

int w_win_effect(w_win_t win,int effect)
{
	return -1;
}
