#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#if GTK_CHECK_VERSION(3,0,0)
#include <gdk/gdkwayland.h>
#endif
#include <cairo.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <math.h>

#include "common.h"

#include "ybutton.h"

#include "ui.h"
#include "translate.h"

#include "ltricky.h"

#ifdef CFG_XIM_YBUS
#include "ybus.h"
#include "ybus-ibus.h"
#endif

#define FIX_CAIRO_LINETO

typedef struct ui_menu{
	LKeyFile *config;
	GtkWidget *root;
	int mb;
	int base;
	int count;
	char *cmd[64];
}ui_menu_t;

static ui_menu_t *ui_build_menu(LKeyFile *kf);

static void ui_clean(void);
static int ui_loop(void);
static int ui_main_update(UI_MAIN *param);
static void *ui_main_win(void);
static int ui_input_update(UI_INPUT *param);
static int ui_input_redraw(void);
static int ui_button_update(int id,UI_BUTTON *param);
static int ui_button_show(int id,int show);
static int ui_main_show(int show);
static int ui_input_show(int show);
static int ui_input_move(int off,int *x,int *y);
static void ui_tray_update(UI_TRAY *param);
static void ui_tray_status(int which);
static void ui_tray_tooltip(const char *tip);
static char *ui_get_select(int (*)(const char*));
static void ui_update_menu(void);
static void ui_skin_path(const char *p);
static void ui_cfg_ctrl(char *name,...);
static void ui_show_message(const char *s);
static void ui_show_image(char *name,char *file,int top,int tran);
static int ui_button_label(int id,const char *text);

#include "ui-common.c"
#include "ui-timer.c"

static int MainWin_over;

static GtkStatusIcon *StatusIcon;
static GdkPixbuf *IconPixbuf[2];
static int IconSelected;
static GtkStatusIcon *(*my_status_icon_new_from_pixbuf)(GdkPixbuf *pixbuf);
static void (*my_status_icon_set_from_pixbuf)(GtkStatusIcon *status_icon,GdkPixbuf *pixbuf);
static void (*my_status_icon_set_tooltip)(GtkStatusIcon *status_icon,const char *tooltip);
static void (*my_status_icon_set_title)(GtkStatusIcon *status_icon,const gchar *title);
static void (*my_status_icon_position_menu)(GtkMenu *menu,gint *x,gint *y,gboolean *push_in,gpointer user_data);
static void (*my_ca_gtk_play_for_widget)(GtkWidget *,uint32_t id,...);
static void (*my_gdk_window_beep)(GdkWindow *window);

static GtkWidget *ImageWin;
static UI_IMAGE ImageWin_bg;

#if 0
static GtkWidget *cfg_menu;
static GtkWidget *im_list;
static GtkWidget *im_manage;
static GtkWidget *im_help;
#endif

static int im_pipe[2];

#if 0
static void ui_popup_menu(void)
{
	if(!cfg_menu) return;
	ui_update_menu();
	gtk_menu_popup(GTK_MENU(cfg_menu),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
}
#endif

static gboolean im_pipe_cb(GIOChannel *source,GIOCondition condition,gpointer data)
{
	unsigned char c;
	int ret;
	ret=read(im_pipe[0],&c,1);
	if(ret==1)
	{
		y_im_request(c);
	}
	return TRUE;
}

static void (*set_opacity)(GdkWindow *window,gdouble opacity);
static gboolean (*is_composited)(GtkWidget *widget);

static void load_sound_system()
{
	my_gdk_window_beep=dlsym(NULL,"gdk_window_beep");
#if !GTK_CHECK_VERSION(3,0,0)
	my_ca_gtk_play_for_widget=dlsym(dlopen("libcanberra-gtk.so.0",RTLD_LAZY),"ca_gtk_play_for_widget");
#else
	my_ca_gtk_play_for_widget=dlsym(dlopen("libcanberra-gtk3.so.0",RTLD_LAZY),"ca_gtk_play_for_widget");
#endif
}

#if 0
static gboolean CompMgrExist(void)
{
	Display *dpy=GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
	Atom a= XInternAtom (dpy, "_NET_WM_CM_S0", False);
	Window mgr = XGetSelectionOwner(dpy, a);
	return mgr?TRUE:FALSE;
}
#endif

void YongSetXErrorHandler(void);
int ui_init(void)
{
	GtkSettings*p;
	GIOChannel *chn;

	if(getenv("WAYLAND_DISPLAY") && getenv("DISPLAY") && !getenv("GDK_BACKEND"))
		setenv("GDK_BACKEND","x11",1);
	if(getenv("GDK_SCALE"))
	{
		double scale1=strtod(getenv("GDK_SCALE"),NULL);
		double scale2=1;
		if(getenv("GDK_DPI_SCALE"))
			scale2=strtod(getenv("GDK_DPI_SCALE"),NULL);
		char temp[64];
		sprintf(temp,"%.1f",scale1*scale2);
		setenv("GDK_DPI_SCALE",temp,1);
	}
	setenv("GDK_SCALE","1",1);
	{
		const char *temp=y_im_get_config_data("main","scale");
		if(temp)
		{
			setenv("GDK_DPI_SCALE",temp,1);
		}
	}

#if !GTK_CHECK_VERSION(3,0,0)
	gtk_set_locale();
#endif
	gtk_init(NULL,NULL);

	set_opacity=dlsym(NULL,"gdk_window_set_opacity");
	is_composited=dlsym(NULL,"gtk_widget_is_composited");
	load_sound_system();
		
	pipe(im_pipe);
	chn=g_io_channel_unix_new(im_pipe[0]);
	guint src=g_io_add_watch(chn,G_IO_IN,im_pipe_cb,NULL);
	g_source_set_name_by_id(src,"yong-comm");
	p=gtk_settings_get_default();
	if(p)
	{
		gtk_settings_set_string_property(p,"gtk-im-module","gtk-im-context-simple",0);
		gtk_settings_set_long_property(p,"gtk-show-input-method-menu",0,0);
	}

	//if(CompMgrExist())
#if !GTK_CHECK_VERSION(3,0,0)
	{
		GdkScreen *screen = gdk_screen_get_default();
		GdkColormap *colormap = gdk_screen_get_rgba_colormap(screen);
		if(colormap)
		{
			gtk_widget_push_colormap(colormap);
			gtk_widget_set_default_colormap(colormap);
		}
	}
#endif
	YongSetXErrorHandler();
	
	{
		GtkWidget *w;
		int dpi;
		w=gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_widget_destroy(w);
		dpi=(int)gdk_screen_get_resolution(gdk_screen_get_default());
		if(dpi>96)
			ui_scale=dpi/96.0;
		const char *temp=y_im_get_config_data("main","scale");
		if(temp)
		{
			ui_scale=strtod(temp,NULL);
		}
	}
	
#if GTK_CHECK_VERSION(3,0,0)
	{
		GdkDisplay *dpy=gdk_display_get_default();
		if(!GDK_IS_X11_DISPLAY(dpy))
		{
			// TODO: set surface to compositor's input method system
		}
	}
#endif

	return 0;
}

int ui_loop(void)
{
	gtk_main();
	return 0;
}

static void get_workarea (int *x, int *y, int *width, int *height)
{
	GdkAtom net_current_desktop_atom = gdk_atom_intern ("_NET_CURRENT_DESKTOP", TRUE);;
	GdkAtom net_workarea_atom = gdk_atom_intern ("_NET_WORKAREA", TRUE);
	GdkWindow *root_window = gdk_get_default_root_window ();
	GdkAtom atom_ret;
	gint format, length;
	guint current_desktop = 0;
	guchar *data;

	*x = 0;
	*y = 0;
	*width = gdk_screen_width();
	*height = gdk_screen_height();

	if (net_current_desktop_atom != GDK_NONE)
	{
		gboolean found = gdk_property_get (root_window,
			net_current_desktop_atom, GDK_NONE, 0, G_MAXLONG, FALSE,
			&atom_ret, &format, &length, &data);
		if (found && format == 32 && length / sizeof(glong) > 0)
		current_desktop = ((glong*)data)[0];
		if (found)
			g_free (data);
	}

	if (net_workarea_atom != GDK_NONE)
	{
		gboolean found = gdk_property_get (root_window,
			net_workarea_atom, GDK_NONE, 0, G_MAXLONG, FALSE,
			&atom_ret, &format, &length, &data);
		if (found && format == 32 && length / sizeof(glong) >= (current_desktop + 1) * 4)
		{
			*x      = ((glong*)data)[current_desktop * 4];
			*y      = ((glong*)data)[current_desktop * 4 + 1];
			*width  = ((glong*)data)[current_desktop * 4 + 2];
			*height = ((glong*)data)[current_desktop * 4 + 3];
		}
		if (found)
			g_free (data);
	}
}

static gboolean main_motion_cb (GtkWidget *window,GdkEventMotion *event,gpointer user_data)
{
	UI_EVENT ue;
	gint pos_x, pos_y;
	if ((event->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) != 0 &&
        	MainWin_Drag)
	{
		gtk_window_get_position (GTK_WINDOW (window), &pos_x, &pos_y);
		gtk_window_move (GTK_WINDOW (MainWin), 
			pos_x + ((gint) event->x_root - MainWin_Drag_X),
			pos_y + ((gint) event->y_root - MainWin_Drag_Y));

		MainWin_Drag_X = (gint) event->x_root;
		MainWin_Drag_Y = (gint) event->y_root;

		return TRUE;
	}
	ue.x=(int)event->x;
	ue.y=(int)event->y;
	ue.event=UI_EVENT_MOVE;
	ui_button_event(MainWin,&ue,NULL);
	return FALSE;
}

static gboolean main_click_cb (GtkWidget *window,GdkEventButton *event,gpointer user_data)
{
	gint click=GPOINTER_TO_INT(user_data);
	GdkCursor *cursor;
	UI_EVENT ue;

	if(event->type!=GDK_BUTTON_PRESS && event->type!=GDK_BUTTON_RELEASE)
		return TRUE;

	if(click==1 && event->button==2)
	{
		YongReloadAll();
		return TRUE;
	}
	
	ue.x=(int)event->x;
	ue.y=(int)event->y;
	if(event->button==1)
	{
		if(click==0)
			ue.event=UI_EVENT_DOWN;
		else
			ue.event=UI_EVENT_UP;
		ue.which=UI_BUTTON_LEFT;
		int ret=ui_button_event(MainWin,&ue,NULL);
		if(ret) return 1;
	}
	else if(event->button==3)
	{
		ue.event=UI_EVENT_UP;
		ue.which=UI_BUTTON_RIGHT;
		int ret=ui_button_event(MainWin,&ue,NULL);
		if(ret) return 1;
	}	
	
	else if(click==1 && event->button==3)
	{
		CONNECT_ID *id=y_xim_get_connect();
		if(!id || (id && !id->focus))
			YongShowMain(0);
		return True;
	}
	
	if(event->button==1 && click==0 && !MainWin_Drag)
	{
#if 0//GTK_CHECK_VERSION(3,0,0)
		gdk_window_begin_move_drag(gtk_widget_get_window(window),event->button,event->x_root,event->y_root,event->time);
#else
		cursor = gdk_cursor_new (GDK_FLEUR);   
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
		gdk_device_grab(gdk_event_get_device((GdkEvent*)event),
				gtk_widget_get_window(window),GDK_OWNERSHIP_NONE,TRUE,
				(GdkEventMask)(GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK),cursor,event->time);
		g_object_unref (cursor);
#else
		gdk_pointer_grab (window->window, TRUE,
			(GdkEventMask)(GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK),
			NULL, cursor, event->time);
		gdk_cursor_unref (cursor);
#endif
		MainWin_Drag=TRUE;
		MainWin_Drag_X=event->x_root;
		MainWin_Drag_Y=event->y_root;
#endif
		return TRUE;
	}
	else if(event->button==1 && click==1 && MainWin_Drag)
	{
		int w,h;
		int scr_w,scr_h;
		gtk_window_get_position(GTK_WINDOW(window), &MainWin_X, &MainWin_Y);
		MainWin_Drag=FALSE;
#if GTK_CHECK_VERSION(3,0,0)
		gdk_device_ungrab(gdk_event_get_device((GdkEvent*)event),event->time);
#else
		gdk_pointer_ungrab(event->time);
#endif
		if(MainWin_X<0) MainWin_X=0;
		if(MainWin_Y<0) MainWin_Y=0;
		w=MainWin_W;h=MainWin_H;
		scr_w=gdk_screen_width();
		scr_h=gdk_screen_height();
		if(MainWin_X+w>scr_w) MainWin_X=scr_w-w;
		if(MainWin_Y+h>scr_h) MainWin_Y=scr_h-h;
		gtk_window_move(GTK_WINDOW(window),MainWin_X,MainWin_Y);
		return TRUE;
	}
	return FALSE;
}

void ui_win_tran(GdkWindow *win,int tran)
{
	if(set_opacity)
		set_opacity(win,(gdouble)(255.0-tran)/255.0);
}

static gboolean on_main_draw(GtkWidget *window,cairo_t *cr)
{
	ui_draw_main_win(cr);
	return TRUE;
}

#if !GTK_CHECK_VERSION(3,0,0)
static gboolean main_expose(GtkWidget *window,GdkEventExpose *event)
{
#ifdef GSEAL_ENABLE
	cairo_t *cr=gdk_cairo_create(gtk_widget_get_window(window));
#else
	cairo_t *cr=gdk_cairo_create(window->window);
#endif
	on_main_draw(window,cr);
	cairo_destroy(cr);
	return TRUE;
}
#endif

static gboolean main_enter_leave_notify(GtkWidget *window,GdkEventCrossing *event)
{
	int tran;
	tran=MainWin_tran;
	
	if(!MainWin_auto_tran)
		return TRUE;

	if(event->type==GDK_ENTER_NOTIFY ||
		(event->x>=0 && event->x<MainWin_W && event->y>=0 && event->y<MainWin_H))
	{
		//printf("enter\n");
		MainWin_over=1;
	}
	else
	{
		//printf("leave\n");
		MainWin_over=0;
		
		UI_EVENT ue;
		ue.event=UI_EVENT_LEAVE;
		ui_button_event(MainWin,&ue,NULL);
	}
	if(MainWin_auto_tran && !MainWin_over)
		tran=255-(255-tran)*2/3;
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
	ui_win_tran(gtk_widget_get_window(MainWin),tran);
#else
	ui_win_tran(MainWin->window,tran);
#endif

	return TRUE;
}

static UI_REGION create_rgn(GdkPixbuf *);
int ui_main_update(UI_MAIN *param)
{
	int tran;
	if(!MainWin)
	{
		MainWin = gtk_window_new(GTK_WINDOW_POPUP);
		//MainWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		assert(MainWin);
#if GTK_CHECK_VERSION(3,0,0)
		{
			GdkScreen *screen = gdk_screen_get_default();
			GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
			if(visual)
			{
				gtk_widget_set_visual(MainWin,visual);
			}
		}
#endif
		gtk_window_set_title(GTK_WINDOW(MainWin),"main");
		gtk_window_set_decorated(GTK_WINDOW(MainWin),FALSE);
		gtk_window_set_accept_focus(GTK_WINDOW(MainWin),FALSE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(MainWin),TRUE);
		gtk_window_set_skip_pager_hint(GTK_WINDOW(MainWin),TRUE);
		gtk_window_set_keep_above(GTK_WINDOW(MainWin),TRUE);
		//gtk_window_set_modal(GTK_WINDOW(MainWin),FALSE);
		gtk_widget_realize(MainWin);
		
#if !GTK_CHECK_VERSION(3,0,0)
		gdk_window_set_events(MainWin->window,
				gdk_window_get_events(MainWin->window) |
				GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|
				GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_POINTER_MOTION_MASK);
		g_signal_connect(G_OBJECT(MainWin),"expose-event",
			G_CALLBACK(main_expose),0);
#else
		gdk_window_set_events(gtk_widget_get_window(MainWin),
				gdk_window_get_events(gtk_widget_get_window(MainWin)) |
				GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|
				GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_POINTER_MOTION_MASK);
		g_signal_connect(G_OBJECT(MainWin),"draw",
			G_CALLBACK(on_main_draw),0);
#endif
		g_signal_connect(G_OBJECT(MainWin),"enter-notify-event",
			G_CALLBACK(main_enter_leave_notify),0);
		g_signal_connect(G_OBJECT(MainWin),"leave-notify-event",
			G_CALLBACK(main_enter_leave_notify),0);

		g_signal_connect (G_OBJECT(MainWin), "button-press-event",
			G_CALLBACK(main_click_cb),GINT_TO_POINTER (0));
		g_signal_connect (G_OBJECT(MainWin), "button-release-event",
			G_CALLBACK (main_click_cb),GINT_TO_POINTER (1));
		g_signal_connect (G_OBJECT(MainWin), "motion-notify-event",
			G_CALLBACK(main_motion_cb),NULL);
	}

	if(MainWin_bg)
	{
		ui_image_free(MainWin_bg);
		MainWin_bg=0;
	}
	if(!param->bg)
	{
		gtk_widget_hide(GTK_WIDGET(MainWin));
		return 0;
	}
	MainTheme.scale=param->scale;
	MainTheme.line_width=param->line_width;
	MainTheme.move_style=param->move_style;
	if(param->bg[0]=='#')
	{
		MainWin_bgc=ui_color_parse(param->bg);
		MainWin_border=ui_color_parse(param->border);
		MainWin_W=param->rc.w;MainWin_H=param->rc.h;
		if(param->scale!=1 && ui_scale!=1)
		{
			MainWin_W=(int)(MainWin_W*ui_scale);
			MainWin_H=(int)(MainWin_H*ui_scale);
		}
#if GTK_CHECK_VERSION(3,0,0)
		gtk_widget_shape_combine_region(MainWin,NULL);
#else
		gtk_widget_shape_combine_mask(MainWin,NULL,0,0);
#endif
	}
	else
	{
		MainWin_bg=ui_image_load(param->bg);
		if(!MainWin_bg) return -1;
		if(param->scale!=1 && param->force_scale)
		{
			MainWin_bg=ui_image_load_scale(param->bg,ui_scale,param->rc.w,param->rc.h);
			MainWin_W=param->rc.w;MainWin_H=param->rc.h;
			if(param->scale!=1 && ui_scale!=1)
			{
				MainWin_W=(int)round(MainWin_W*ui_scale);
				MainWin_H=(int)round(MainWin_H*ui_scale);
			}
		}
		else
		{
			MainTheme.scale=1;
			MainWin_W = gdk_pixbuf_get_width(MainWin_bg);
			MainWin_H = gdk_pixbuf_get_height(MainWin_bg);
		}
		if(!is_composited || !is_composited(MainWin))
		{
#if !GTK_CHECK_VERSION(3,0,0)
			GdkBitmap *bitmap;
			gdk_pixbuf_render_pixmap_and_mask(MainWin_bg, 0, &bitmap, 80);
			gtk_widget_shape_combine_mask(MainWin,bitmap,0,0);
			g_object_unref(bitmap);
#else
			UI_REGION r=create_rgn(MainWin_bg);
			gtk_widget_shape_combine_region(MainWin,r);
			ui_region_destroy(r);
#endif
		}
		else
		{
			//gdk_pixbuf_render_pixmap_and_mask(MainWin_bg, 0, &bitmap, 1);
			//gtk_widget_input_shape_combine_mask(MainWin,bitmap,0,0);
			//g_object_unref(bitmap);
#if !GTK_CHECK_VERSION(3,0,0)
			gtk_widget_shape_combine_mask(MainWin,NULL,0,0);
#else
			gtk_widget_shape_combine_region(MainWin,NULL);
#endif
		}
	}
	gtk_window_resize(GTK_WINDOW(MainWin),MainWin_W,MainWin_H);
	gtk_widget_set_size_request(GTK_WIDGET(MainWin),MainWin_W,MainWin_H);
	MainWin_move=param->move;
	MainWin_X=param->rc.x;MainWin_Y=param->rc.y;
	MainWin_tran=param->tran;
	MainWin_auto_tran=param->auto_tran;
	tran=MainWin_tran;
	if(MainWin_auto_tran && !MainWin_over)
		tran=255-(255-tran)*2/3;
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
	ui_win_tran(gtk_widget_get_window(MainWin),tran);
#else
	ui_win_tran(MainWin->window,tran);
#endif
	ui_main_show(-1);
	return 0;
}

#if 1//!GTK_CHECK_VERSION(3,0,0)

static gboolean input_motion_cb (GtkWidget *window,GdkEventMotion *event,gpointer user_data)
{
	gint pos_x, pos_y;
	if ((event->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) != 0 &&
        	InputWin_Drag)
	{
		gtk_window_get_position (GTK_WINDOW (window), &pos_x, &pos_y);
		gtk_window_move (GTK_WINDOW (InputWin), 
			pos_x + ((gint) event->x_root - InputWin_Drag_X),
			pos_y + ((gint) event->y_root - InputWin_Drag_Y));

		InputWin_Drag_X = (gint) event->x_root;
		InputWin_Drag_Y = (gint) event->y_root;

		return TRUE;
	}
	return FALSE;
}
#endif

static gboolean input_click_cb (GtkWidget *window,GdkEventButton *event,gpointer user_data)
{
	gint click=GPOINTER_TO_INT(user_data);
	static gulong motion_handler;
	GdkCursor *cursor;
	
	if(event->button!=1 && event->button!=3) return TRUE;

	if(click==0 && event->button==1)
	{
		if(InputWin_Drag) return FALSE;
		if(event->x >= InputTheme.CandX && event->y>= InputTheme.CandY)
		{
			EXTRA_IM *eim=CURRENT_EIM();
			if(eim)
			{
				int count=eim->CandWordCount;
				double x=event->x;
				if(count && x>=im.CandPosX[0] && x<im.CandPosX[3*count])
				{
					return TRUE;
				}
			}
		}
#if 0//GTK_CHECK_VERSION(3,0,0)
		gdk_window_begin_move_drag(gtk_widget_get_window(InputWin),event->button,event->x_root,event->y_root,event->time);
		InputWin_Drag_X=event->x_root;
		InputWin_Drag_Y=event->y_root;
#else
		motion_handler = g_signal_connect (G_OBJECT(window), "motion-notify-event",
			G_CALLBACK(input_motion_cb),NULL);
		cursor = gdk_cursor_new (GDK_FLEUR);
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
		gdk_device_grab(gdk_event_get_device((GdkEvent*)event),
				gtk_widget_get_window(window),GDK_OWNERSHIP_NONE,TRUE,
				(GdkEventMask)(GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK),cursor,event->time);
		g_object_unref (cursor);
#else
		gdk_pointer_grab (window->window, TRUE,
			(GdkEventMask)(GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK),
			NULL, cursor, event->time);
		gdk_cursor_unref (cursor);
#endif
		InputWin_Drag=TRUE;
		InputWin_Drag_X=event->x_root;
		InputWin_Drag_Y=event->y_root;
#endif
		return TRUE;
	}
	else if(click==1)
	{
		int w,h;
		int scr_w,scr_h;
		CONNECT_ID *id;
		if(!InputWin_Drag)
		{
			int i,count;
			EXTRA_IM *eim=CURRENT_EIM();
			double x=event->x;
			double y=event->y;
			if(x < InputTheme.CandX || y< InputTheme.CandY)
				return TRUE;
			if(!eim) return TRUE;
			count=eim->CandWordCount;
			if(!count) return TRUE;
			for(i=0;i<count;i++)
			{
				double *pos=im.CandPosX+i*3;
				if(InputTheme.line==2)
				{
					pos=im.CandPosY+i*3;
					x=y;
				}
				if(x >= pos[0] && ((i<count-1 && x<pos[3]-InputTheme.space)
					|| (i==count-1 && x<pos[3])))
				{
					char *s=eim->GetCandWord(i);
					if(s)
					{
						y_xim_send_string(s2t_conv(s));
						//if(event->button==1)
							YongResetIM();
					}
					else
					{
						char *p=eim->CodeInput;
						//p=y_im_key_desc_translate(p,eim->CandTable[eim->SelectIndex]);
						//y_im_str_encode(p,im.CodeInput,DONT_ESCAPE);
						y_im_key_desc_translate(p,0,eim->CandTable[eim->SelectIndex],im.CodeInput,MAX_CODE_LEN+1);
						y_im_str_encode(eim->StringGet,im.StringGet,0);
						//YongDrawInput();
						y_ui_input_draw();
					}
					break;					
				}
			}
			return TRUE;
		}
		g_signal_handler_disconnect (G_OBJECT (window), motion_handler);
		gtk_window_get_position(GTK_WINDOW(window), &InputWin_X, &InputWin_Y);
		InputWin_Drag=FALSE;
#if GTK_CHECK_VERSION(3,0,0)
		gdk_device_ungrab(gdk_event_get_device((GdkEvent*)event),event->time);
#else
		gdk_pointer_ungrab(event->time);
#endif
		if(InputWin_X<0) InputWin_X=0;
		if(InputWin_Y<0) InputWin_Y=0;
		gtk_window_get_size(GTK_WINDOW(InputWin),&w,&h);
		scr_w=gdk_screen_width();
		scr_h=gdk_screen_height();
		if(InputWin_X+w>scr_w) InputWin_X=scr_w-w;
		if(InputWin_Y+h>scr_h) InputWin_Y=scr_h-h;
		gtk_window_move(GTK_WINDOW(window),InputWin_X,InputWin_Y);
		//id=YongGetConnect();
		id=y_xim_get_connect();
		if(id)
		{
			id->x=(unsigned short)InputWin_X;
			id->y=(unsigned short)InputWin_Y;
		}
		
		return TRUE;
	}
	return FALSE;
}

static gboolean on_input_draw(GtkWidget *window,cairo_t *cr)
{
	ui_draw_input_win(cr);
	return TRUE;
}

#if !GTK_CHECK_VERSION(3,0,0)
static gboolean input_expose(GtkWidget *window,GdkEventExpose *event)
{
#ifdef GSEAL_ENABLE
	cairo_t *cr=gdk_cairo_create(gtk_widget_get_window(window));
#else
	cairo_t *cr=gdk_cairo_create(window->window);
#endif
	on_input_draw(window,cr);
	cairo_destroy(cr);
	return TRUE;
}
#endif

static guchar get_pixel_alpha(GdkPixbuf *pixbuf,int x,int y)
{
	int width, height, rowstride, n_channels;
	guchar *pixels, *p;

	n_channels = gdk_pixbuf_get_n_channels (pixbuf);

	assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
	assert (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);
	assert (gdk_pixbuf_get_has_alpha (pixbuf));
	assert (n_channels == 4);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	assert (x >= 0 && x < width);
	assert (y >= 0 && y < height);

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	p = pixels + y * rowstride + x * n_channels;
	return p[3];
}

static UI_REGION create_rgn(GdkPixbuf *p)
{
	guchar a;
	int i,j;
	int w,h;
	UI_REGION rgn;
	GdkRectangle rc;
	if(!gdk_pixbuf_get_has_alpha(p))
		return NULL;
	w = gdk_pixbuf_get_width (p);
	h = gdk_pixbuf_get_height (p);
#if GTK_CHECK_VERSION(3,0,0)
	rgn=cairo_region_create();
#else
	rgn=gdk_region_new();
#endif
	for(j=0;j<h;j++)
	{
		rc.x=0;
		rc.y=j;
		rc.width=0;
		rc.height=1;
		for(i=0;i<w;i++)
		{
			a=get_pixel_alpha(p,i,j);
			if(a<80)
			{
				if(rc.width)
				{
#if GTK_CHECK_VERSION(3,0,0)
					cairo_region_union_rectangle(rgn,&rc);
#else
					gdk_region_union_with_rect(rgn,&rc);
#endif
					rc.width=0;
				}
				continue;
			}
			if(!rc.width)
			{
				rc.x=i;
				rc.width=1;
			}
			else
			{
				rc.width++;
			}
		}
		if(rc.width)
		{
#if GTK_CHECK_VERSION(3,0,0)
			cairo_region_union_rectangle(rgn,&rc);
#else
			gdk_region_union_with_rect(rgn,&rc);
#endif
		}
	}
	return rgn;
}

static void set_rgn(void)
{

	UI_REGION u,t;
	if(!InputTheme.rgn[0] && !InputTheme.rgn[2])
		return;
#if GTK_CHECK_VERSION(3,0,0)
	u=cairo_region_create();
#else
	u=gdk_region_new();
#endif
	if(InputTheme.rgn[0])
	{
#if GTK_CHECK_VERSION(3,0,0)
		cairo_region_union(u,InputTheme.rgn[0]);
#else
		gdk_region_union(u,InputTheme.rgn[0]);
#endif
	}
	InputTheme.clip.width=InputTheme.RealWidth-InputTheme.Left-InputTheme.Right;
#if GTK_CHECK_VERSION(3,0,0)
	cairo_region_union_rectangle(u,&InputTheme.clip);
#else
	gdk_region_union_with_rect(u,&InputTheme.clip);
#endif
	if(InputTheme.rgn[2])
	{
#if GTK_CHECK_VERSION(3,0,0)
		t=cairo_region_copy(InputTheme.rgn[2]);
		cairo_region_translate(t,InputTheme.RealWidth-InputTheme.Right,0);
		cairo_region_union(u,t);
#else
		t=gdk_region_copy(InputTheme.rgn[2]);
		gdk_region_offset(t,InputTheme.RealWidth-InputTheme.Right,0);
		gdk_region_union(u,t);
#endif
		ui_region_destroy(t);
	}
#if GTK_CHECK_VERSION(3,0,0)
	gdk_window_shape_combine_region(gtk_widget_get_window(InputWin),u,0,0);
#else
	gdk_window_shape_combine_region(InputWin->window,u,0,0);
#endif
	ui_region_destroy(u);
}

static UI_IMAGE ui_input_bg_adjust(UI_IMAGE bg,int cand_max,int bottom)
{
	UI_IMAGE res,tmp;
	int cand;
	int w,h,w0,h0;
	double scale;
	
	if(InputTheme.line!=2)
		return bg;
	if(InputTheme.Top==InputTheme.Bottom)
		return bg;
	if(!bottom) bottom=InputTheme.Bottom;
	cand=y_im_get_config_int("IM","cand");
		
	ui_image_size(bg,&w0,&h0);

	ui_text_size(NULL,InputTheme.layout," ",&w,&h);
	w=w0;
	h=cand*h+(cand-1)*InputTheme.space+bottom+InputTheme.CandY;
	if(h<InputTheme.Top+InputTheme.Bottom)
		h=InputTheme.Top+InputTheme.Bottom;
		
	if(InputTheme.mHeight>h)
		InputTheme.mHeight=h;

	scale=(double)(h-InputTheme.Bottom-InputTheme.Top)/(double)(h0-InputTheme.Bottom-InputTheme.Top);
	res=gdk_pixbuf_new(GDK_COLORSPACE_RGB,
			gdk_pixbuf_get_has_alpha(bg),
			gdk_pixbuf_get_bits_per_sample(bg),
			w,h);
	gdk_pixbuf_copy_area(bg,0,0,w,InputTheme.Top,res,0,0);
	tmp=ui_image_part(bg,0,InputTheme.Top,w,h0-InputTheme.Bottom-InputTheme.Top);
	gdk_pixbuf_scale(tmp,res,0,InputTheme.Top,
			w,h-InputTheme.Bottom-InputTheme.Top,
			0,InputTheme.Top,1.0,scale,
			GDK_INTERP_BILINEAR);
	ui_image_free(tmp);
	gdk_pixbuf_copy_area(bg,0,h0-InputTheme.Bottom,w,InputTheme.Bottom,
			res,0,h-InputTheme.Bottom);
			
	ui_image_free(bg);
	return res;
}

int ui_input_update(UI_INPUT *param)
{
	char *tmp;
	GdkPixbuf *bg;
	int bg_w,bg_h;
	int i;
	GdkWindow *window;

	if(!InputWin)
	{
		InputWin = gtk_window_new(GTK_WINDOW_POPUP);
		//InputWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		assert(InputWin);
#if GTK_CHECK_VERSION(3,0,0)
		{
			GdkScreen *screen = gdk_screen_get_default();
			GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
			if(visual)
				gtk_widget_set_visual(InputWin,visual);
		}
#endif
		gtk_window_set_title(GTK_WINDOW(InputWin),"input");
		gtk_window_set_decorated(GTK_WINDOW(InputWin),FALSE);
		gtk_window_set_accept_focus(GTK_WINDOW(InputWin),FALSE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(InputWin),TRUE);
		gtk_window_set_skip_pager_hint(GTK_WINDOW(InputWin),TRUE);
		gtk_window_set_keep_above(GTK_WINDOW(InputWin),TRUE);
		//gtk_window_set_modal(GTK_WINDOW(InputWin),FALSE);
		gtk_widget_realize(InputWin);
		

#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
		window=gtk_widget_get_window(InputWin);
#else
		window=InputWin->window;
#endif
		gdk_window_set_events(window,
				gdk_window_get_events(window) |
				GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|
				GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_POINTER_MOTION_MASK);

		g_signal_connect (G_OBJECT(InputWin), "button-press-event",
			G_CALLBACK(input_click_cb),GINT_TO_POINTER (0));
		g_signal_connect (G_OBJECT(InputWin), "button-release-event",
			G_CALLBACK(input_click_cb),GINT_TO_POINTER (1));
#if !GTK_CHECK_VERSION(3,0,0)
		g_signal_connect(G_OBJECT(InputWin),"expose-event",
			G_CALLBACK(input_expose),0);
#else
		g_signal_connect(G_OBJECT(InputWin),"draw",
			G_CALLBACK(on_input_draw),0);
#endif
	}
	else
	{
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
		window=gtk_widget_get_window(InputWin);
#else
		window=InputWin->window;
#endif
		gdk_window_shape_combine_region(window,NULL,0,0);
	}

	if(InputTheme.layout)
	{
		ui_font_free(InputTheme.layout);
		InputTheme.layout=NULL;
	}
	for(i=0;i<3;i++)
	{
		if(InputTheme.bg[i])
		{
			g_object_unref(InputTheme.bg[i]);
			InputTheme.bg[i]=NULL;
		}
		if(InputTheme.rgn[i])
		{
			//gdk_region_destroy(InputTheme.rgn[i]);
			ui_region_destroy(InputTheme.rgn[i]);
			InputTheme.rgn[i]=NULL;
		}
	}
	InputTheme.line=param->line;
	InputTheme.caret=param->caret;
	InputTheme.page=param->page;
	InputTheme.noshow=param->noshow;
	InputTheme.root=param->root;
	InputTheme.space=param->space;
	InputTheme.no=param->no;
	InputTheme.strip=param->strip;
	InputTheme.x=param->x;
	InputTheme.y=param->y;

	InputTheme.mWidth=param->mw;
	InputTheme.mHeight=param->mh;
	
	InputTheme.CodeX=param->code.x;
	InputTheme.CodeY=param->code.y;
	InputTheme.CandX=param->cand.x;
	InputTheme.CandY=param->cand.y;
	InputTheme.OffX=param->off.x;
	InputTheme.OffY=param->off.y;
	
	InputTheme.scale=param->scale;
	
	if(InputTheme.scale==1 && ui_scale!=1)
	{
		double temp=ui_scale;
		ui_scale=1;
		InputTheme.layout=ui_font_parse(InputWin,param->font);
		ui_scale=temp;
	}
	else
	{
		InputTheme.layout=ui_font_parse(InputWin,param->font);
	}
	
	InputTheme.line_width=param->line_width;
	memcpy(InputTheme.pad,param->pad,sizeof(param->pad));

	tmp=param->bg[0];
	assert(tmp!=NULL);
	if(tmp[0]=='#')
	{
		InputTheme.bg_color=ui_color_parse(tmp);
		tmp=param->border;
		InputTheme.border=ui_color_parse(tmp);
		InputTheme.Width=param->w;
		InputTheme.Height=param->h;
		InputTheme.RealHeight=InputTheme.Height;
		InputTheme.RealWidth=InputTheme.Width;
		InputTheme.Left=3;
		InputTheme.Right=3;
		InputTheme.WorkLeft=InputTheme.Left;
		InputTheme.WorkRight=InputTheme.Right;
		if(param->sep)
			InputTheme.sep=ui_color_parse(param->sep);
		else
			InputTheme.sep=InputTheme.border;
		
		if(InputTheme.scale!=1 && ui_scale>1)
		{
			InputTheme.space=(int)round(ui_scale*InputTheme.space);
			InputTheme.CodeX=(int)round(ui_scale*InputTheme.CodeX);
			InputTheme.CodeY=(int)round(ui_scale*InputTheme.CodeY);
			InputTheme.CandX=(int)round(ui_scale*InputTheme.CandX);
			
			int h2=InputTheme.Height/2;
			int pad=InputTheme.CandY-h2;
			h2+=(InputTheme.CodeY-param->code.y)*2;
			InputTheme.RealHeight=InputTheme.Height=2*h2;
			InputTheme.CandY=h2+(int)round(pad*ui_scale);

			InputTheme.pad[0]*=ui_scale;
			InputTheme.pad[1]*=ui_scale;
			InputTheme.pad[2]*=ui_scale;
			InputTheme.pad[3]*=ui_scale;
		}
	}
	else
	{
		if(param->sep)
			InputTheme.sep=ui_color_parse(param->sep);
		else
			InputTheme.sep=(UI_COLOR){0,0,0,0};
		InputTheme.Left=param->left;;
		InputTheme.Right=param->right;
		InputTheme.Top=param->top;
		InputTheme.Bottom=param->bottom;
		if(param->scale!=1 && param->force_scale)
		{
			bg=ui_image_load_scale(tmp,ui_scale,param->w,param->h);
			bg_w=gdk_pixbuf_get_width(bg);
			bg_h=gdk_pixbuf_get_height(bg);
			if(bg_w!=param->w)
			{
				// adjust all size here
				double scale=bg_w*1.0L/param->w;
				InputTheme.line_width=(int)round(scale*InputTheme.line_width);
				InputTheme.Width=(int)round(scale*InputTheme.Width);
				InputTheme.Height=(int)round(scale*InputTheme.Height);
				InputTheme.mWidth=(int)round(scale*InputTheme.mWidth);
				InputTheme.mHeight=(int)round(scale*InputTheme.mHeight);
				InputTheme.Left=(int)round(scale*InputTheme.Left);
				InputTheme.Right=(int)round(scale*InputTheme.Right);
				InputTheme.Top=(int)round(scale*InputTheme.Top);
				InputTheme.Bottom=(int)round(scale*InputTheme.Bottom);

				InputTheme.space=(int)round(scale*InputTheme.space);
				InputTheme.CodeX=(int)round(scale*InputTheme.CodeX);
				InputTheme.CodeY=(int)round(scale*InputTheme.CodeY);
				InputTheme.CandX=(int)round(scale*InputTheme.CandX);
				InputTheme.CandY=(int)round(scale*InputTheme.CandY);

				param->work_left=(int)round(scale*param->work_left);
				param->work_right=(int)round(scale*param->work_right);

				if(scale!=1 && !y_im_has_config("input","font"))
				{
					ui_font_free(InputTheme.layout);
					double temp=ui_scale;
					ui_scale=scale;
					InputTheme.layout=ui_font_parse(InputWin,param->font);
					ui_scale=temp;
				}

				InputTheme.pad[0]*=scale;
				InputTheme.pad[1]*=scale;
				InputTheme.pad[2]*=scale;
				InputTheme.pad[3]*=scale;
			}
		}
		else
		{
			bg=ui_image_load(tmp);
		}
		bg=ui_input_bg_adjust(bg,param->cand_max,param->work_bottom);

		bg_w=gdk_pixbuf_get_width(bg);
		bg_h=gdk_pixbuf_get_height(bg);

		InputTheme.RealHeight=bg_h;
	
		if(param->work_left>0 || param->work_right>0)
		{
			InputTheme.WorkLeft=param->work_left;
			InputTheme.WorkRight=param->work_right;
		}
		else
		{
			InputTheme.WorkLeft=InputTheme.Left;
			InputTheme.WorkRight=InputTheme.Right;
		}

		InputTheme.Width=bg_w;
		InputTheme.Height=bg_h;

		if(!InputTheme.Left && !InputTheme.Right)
		{
			InputTheme.bg[1]=bg;
		}
		else
		{
			gboolean comp=is_composited && is_composited(InputWin);
			if(InputTheme.Left)
			{
				InputTheme.bg[0]=gdk_pixbuf_new_subpixbuf(bg,0,0,InputTheme.Left,bg_h);
				if(!comp) InputTheme.rgn[0]=create_rgn(InputTheme.bg[0]);
			}
			InputTheme.bg[1]=gdk_pixbuf_new_subpixbuf(bg,InputTheme.Left,0,
				bg_w-InputTheme.Left-InputTheme.Right,bg_h);
			if(!comp) InputTheme.rgn[1]=create_rgn(InputTheme.bg[1]);
			if(InputTheme.rgn[1])
			{
#if GTK_CHECK_VERSION(3,0,0)
				cairo_region_translate(InputTheme.rgn[1],InputTheme.Left,0);
				cairo_region_get_extents(InputTheme.rgn[1],&InputTheme.clip);
#else
				gdk_region_offset(InputTheme.rgn[1],InputTheme.Left,0);
				gdk_region_get_clipbox(InputTheme.rgn[1],&InputTheme.clip);
#endif
				ui_region_destroy(InputTheme.rgn[1]);
				InputTheme.rgn[1]=0;
			}
			if(InputTheme.Right)
			{
				InputTheme.bg[2]=gdk_pixbuf_new_subpixbuf(bg,bg_w-InputTheme.Right,
					0,InputTheme.Right,bg_h);
				if(!comp) InputTheme.rgn[2]=create_rgn(InputTheme.bg[2]);
			}
			g_object_unref(bg);
		}
	}

	InputTheme.bg_first=ui_color_parse(param->bg[1]?param->bg[1]:"#00FFFFFF");

	InputTheme.text[0]=ui_color_parse(param->text[0]);
	InputTheme.text[1]=ui_color_parse(param->text[1]);
	InputTheme.text[2]=ui_color_parse(param->text[2]);
	InputTheme.text[3]=ui_color_parse(param->text[3]);
	InputTheme.text[4]=ui_color_parse(param->text[4]);
	InputTheme.text[5]=ui_color_parse(param->text[5]);
	InputTheme.text[6]=ui_color_parse(param->text[6]);

	InputTheme.RealWidth=2*InputTheme.RealHeight;
	InputTheme.MaxHeight=InputTheme.RealHeight;

	if(InputTheme.line==1 && !InputTheme.CandY)
		InputTheme.CandY=InputTheme.CodeY;
		
	if(!InputTheme.bg[1])
	{
		char temp[8];
		int cy;
		int get_text_width(const char *s,PangoLayout *layout,int *height);
		y_im_str_encode("测",temp,0);
		get_text_width(temp,InputTheme.layout,&cy);
		if(InputTheme.line==0 || InputTheme.line==2)
		{
			int pad=InputTheme.CandY-InputTheme.Height/2;
			if(InputTheme.CodeY<0 && cy>InputTheme.Height-2*InputTheme.CandY)
			{
				InputTheme.Height=InputTheme.CandY*2+cy;
				InputTheme.RealHeight=InputTheme.Height;
			}
			else if(InputTheme.CodeY+cy>InputTheme.Height/2-pad)
			{
				InputTheme.CandY=InputTheme.CodeY+cy+2*pad;
				InputTheme.Height=InputTheme.CandY+cy+InputTheme.CodeY;
				InputTheme.RealHeight=InputTheme.Height;
			}
		}
		else if(InputTheme.line==1)
		{
			if(2*InputTheme.CodeY+cy>InputTheme.Height)
			{
				InputTheme.Height=2*InputTheme.CodeY+cy;
				InputTheme.RealHeight=InputTheme.Height;
			}
		}
	}
	ui_win_tran(window,param->tran);
	if(InputTheme.noshow==2)
		y_ui_input_draw();
	
	return 0;
}

int ui_input_redraw(void)
{
	gtk_widget_queue_draw(InputWin);
	return 0;
}

int ui_input_move(int off,int *x,int *y)
{
	int scr_w,scr_h;
	int height=InputTheme.RealHeight;
	
	if(!InputTheme.bg[1])
		height=InputTheme.MaxHeight;

	if(*x==POSITION_ORIG)
	{
		int w,h;
		get_workarea(x,y,&w,&h);
		*y=*y+h-height;
		
		if(InputTheme.y!=0)
		{
			if(InputTheme.y==-1 && InputTheme.x==1)
			{
				*y=(h-height)/2;
			}
			else if(InputTheme.y!=-1)
			{
				*x=InputTheme.x;*y=InputTheme.y;
			}
		}
	}

	scr_w=gdk_screen_width();
	scr_h=gdk_screen_height();
	if(*y>=scr_h || *x>=2*scr_w)
	{
		/* xpos may big than scr_w, so use 2*scr_w */
		/* found bad pos is always just the ypos */
		return -1;
	}

	if(off)
	{
		if(*y+height+InputTheme.OffY<=scr_h)
			*y+=InputTheme.OffY;
		else *y-=height+18+30;
		*x+=InputTheme.OffX;
	}
	if(*x<0) *x=0;
	else if(*x+InputTheme.RealWidth>scr_w)
		*x=scr_w-InputTheme.RealWidth;

	if(*y<0) *y=0;
	else if(*y+height>scr_h)
		*y=scr_h-height-18-30;
	gtk_window_move(GTK_WINDOW(InputWin),*x,*y);
	InputWin_X=*x;
	InputWin_Y=*y;

	return 0;
}

int get_text_width(const char *s,PangoLayout *layout,int *height)
{
	int ret;
	if(!layout) layout=InputTheme.layout;
	pango_layout_set_text(layout,s,-1);
	pango_layout_get_pixel_size(layout,&ret,height);
	return ret;
}

int YongCodeWidth(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	int ret;
	
	if(InputTheme.CodeY<0)
	{
		im.CodePos[0]=im.CodePos[1]=im.CodePos[2]=InputTheme.CodeX;
		return 0;
	}

	im.CodePos[0]=InputTheme.CodeX;
	if(eim && eim->StringGet[0])
	{
		ret=get_text_width(im.StringGet,InputTheme.layout,NULL);
		im.CodePos[1]=im.CodePos[0]+ret;
	}
	else
	{
		im.CodePos[1]=InputTheme.CodeX;
	}

	ret=get_text_width(im.CodeInput,InputTheme.layout,&im.cursor_h);
	im.CodePos[3]=im.CodePos[1]+ret;
	if(!eim || eim->CaretPos==-1 || !im.CodeInput[eim->CaretPos])
	{
		im.CodePos[2]=im.CodePos[1]+ret;
	}
	else
	{
		char tmp=im.CodeInput[eim->CaretPos];
		im.CodeInput[eim->CaretPos]=0;
		im.CodePos[2]=get_text_width(im.CodeInput,InputTheme.layout,NULL)+im.CodePos[1];
		im.CodeInput[eim->CaretPos]=tmp;
	}
	return (int)im.CodePos[3]+InputTheme.CodeX-InputTheme.WorkLeft;
}

int YongPageWidth(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	int ret=0;
	if(/*!im.EnglishMode && */eim && eim->CandPageCount>1)
	{
		int h;
		sprintf(im.Page,"%d/%d",eim->CurCandPage+1,eim->CandPageCount);
		im.PageLen=get_text_width(im.Page,InputTheme.layout,&h);
		im.PagePosY=InputTheme.CodeY;
		ret=im.PageLen;
	}
	return ret;
}

int YongCandWidth(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	int i,count;
	double cur=0;
	int cur_y;
	double *width,*height;

	if(!eim) return 0;
	count=eim->CandWordCount;

	width=im.CandPosX+count*3;
	height=im.CandPosY+count*3;

	cur=InputTheme.CandX;
	cur_y=InputTheme.CandY;
	*width=cur;*height=cur_y;
	for(i=0;i<count;i++)
	{
		double *pos;
		int h,h1,h2,h3=0;

		pos=im.CandPosX+i*3;

		pos[0]=cur;
		if(InputTheme.no==0)
		{
			cur+=get_text_width(YongGetSelectNumber(i),InputTheme.layout,&h1);
		}
		else
		{
			h1=0;
		}

		pos[1]=cur;
		cur+=get_text_width(im.CandTable[i],NULL,&h2);
		h=MAX(h1,h2);		

		pos[2]=cur;
		if(im.Hint)
		{
			//char *t=eim->CodeTips[i];
			char *t=im.CodeTips[i];
			if(t && *t)
			{
				cur+=get_text_width(t,NULL,&h3);
				h=MAX(h,h3);
			}
		}
		*width=MAX(*width,cur);
		im.CandWidth[i]=cur-pos[0];
		im.CandHeight[i]=h;
		if(i!=count-1)
			cur+=InputTheme.space;

		pos=im.CandPosY+i*3;
		pos[0]=cur_y+(h-h1+1)/2;
		pos[1]=cur_y+(h-h2+1)/2;
		pos[2]=cur_y+(h-h3+1)/2;
		if(InputTheme.line==2)
		{
			if(i==count-1)
			{
				//*height+=h+InputTheme.CodeY;
				*height+=h;
			}
			else
			{
				*height+=h+InputTheme.space;
			}
			cur_y+=h+InputTheme.space;

			cur=InputTheme.CandX;
		}
	}
	return (int)*width;
}

int YongDrawInput(void)
{
	int TempWidth,DeltaWidth;
	int TempHeight,DeltaHeight;
	int CodeWidth,CandWidth=0,PageWidth=0;
	EXTRA_IM *eim=CURRENT_EIM();
	int count=0;

	if(!InputWin) return 0;

	if(!im.CodeInputEngine[0] && (!eim || !eim->StringGet[0]) && InputTheme.noshow!=2)
	{
		YongShowInput(0);
		return 0;
	}
	if(InputTheme.CodeY<0 && (!eim || eim->CandWordCount<=0))
	{
		YongShowInput(0);
		return 0;
	}
	if(InputTheme.onspot && InputTheme.line==1 && im.Preedit==1)
		CodeWidth=0;
	else
		CodeWidth=YongCodeWidth();
	if(InputTheme.page)
		PageWidth=YongPageWidth();
	if(eim) count=eim->CandWordCount;
	if(count)
	{
		int i;
		for(i=0;i<count;i++)
		{
			char *p=im.CandTable[i];
			int len;
			if(eim->WorkMode==EIM_WM_ASSIST)
				len=0;
			y_im_key_desc_translate(eim->CodeTips[i],len,eim->CandTable[i],
					im.CodeTips[i],MAX_TIPS_LEN+1);
			if(eim->WorkMode==EIM_WM_QUERY)
			{
				char *s=eim->CandTable[i];
				y_im_key_desc_translate(s,0,eim->CodeInput,p,MAX_TIPS_LEN+1);
			}
			else
			{
				const char *s=s2t_conv(eim->CandTable[i]);
				y_im_disp_cand(s,p,(InputTheme.strip>>(16*(i==eim->SelectIndex)+0))&0xff,
					(InputTheme.strip>>(16*(i==eim->SelectIndex)+8))&0xff);
			}
		}
		CandWidth=YongCandWidth();
	}
	if(InputTheme.line==0)
	{
		if(PageWidth)
			im.PagePosX=MAX(CandWidth-PageWidth,CodeWidth+InputTheme.space);
		CodeWidth+=InputTheme.CodeX-InputTheme.WorkLeft;
		if(PageWidth)
			CodeWidth+=InputTheme.space+PageWidth;
		CandWidth+=InputTheme.CandX-InputTheme.WorkLeft;
		TempWidth=MAX(CodeWidth,CandWidth);
		TempHeight=InputTheme.RealHeight;
	}
	else if(InputTheme.line==1)
	{
		int pad=InputTheme.CodeX-InputTheme.WorkLeft;
		TempWidth=CodeWidth+CandWidth+pad;
		if(PageWidth)
		{
			im.PagePosX=TempWidth+InputTheme.space-pad;
			TempWidth+=InputTheme.space+PageWidth;			
		}
		if(eim)
		{
			int i,count;
			count=eim->CandWordCount;
			for(i=0;i<=count;i++)
			{
				double *pos=im.CandPosX+i*3;
				pos[0]+=CodeWidth;
				pos[1]+=CodeWidth;
				pos[2]+=CodeWidth;
			}
		}
		TempHeight=InputTheme.RealHeight;
	}
	else
	{
		double *pos=im.CandPosY+count*3;
		int pad=InputTheme.CodeX-InputTheme.WorkLeft;
		CodeWidth+=pad;
		if(PageWidth)
			CodeWidth+=InputTheme.space+PageWidth;
		CandWidth+=InputTheme.CandX-InputTheme.WorkLeft;
		TempWidth=MAX(CodeWidth,CandWidth);
		if(InputTheme.CodeY>=0)
			TempHeight=(int)pos[0]+InputTheme.CodeY;
		else
			TempHeight=(int)pos[0]+InputTheme.CandY;
		if(TempHeight<InputTheme.Height)
			TempHeight=InputTheme.Height;
		if(InputTheme.bg[1])
			TempHeight=InputTheme.RealHeight;
	}
	TempWidth+=InputTheme.WorkRight;
	if(InputTheme.line!=2 && !InputTheme.mWidth && TempWidth<InputTheme.RealHeight*2)
		TempWidth=InputTheme.RealHeight*2;
	if(TempWidth<InputTheme.mWidth)
		TempWidth=InputTheme.mWidth;
	if(InputTheme.line==2 && TempHeight<InputTheme.mHeight)
		TempHeight=InputTheme.mHeight;
	DeltaWidth=TempWidth-InputTheme.RealWidth;
	DeltaHeight=TempHeight-InputTheme.RealHeight;
	if(DeltaWidth || DeltaHeight)
	{
		InputTheme.RealWidth=TempWidth;
		InputTheme.RealHeight=TempHeight;
#ifdef G_OS_WIN32
		MoveWindow(InputWin,InputWin_X,InputWin_Y,
			InputTheme.RealWidth,InputTheme.RealHeight,TRUE);
#else
		gtk_window_resize(GTK_WINDOW(InputWin),InputTheme.RealWidth,InputTheme.RealHeight);
#endif
	}
	if(PageWidth && InputTheme.line==2)
	{
			im.PagePosX=InputTheme.RealWidth-InputTheme.WorkRight-
				(InputTheme.CodeX-InputTheme.WorkLeft)-PageWidth;
	}
	InputTheme.MaxHeight=MAX(InputTheme.MaxHeight,InputTheme.RealHeight);
	set_rgn();
	YongMoveInput(POSITION_ORIG,POSITION_ORIG);
	ybus_ibus_input_draw(InputTheme.line);
	YongShowInput(1);

#ifdef G_OS_WIN32
	InvalidateRect(InputWin,0,FALSE);
#else
	gtk_widget_queue_draw(InputWin);
#endif
	/* show at preedit area */
	if(((eim && !eim->CandWordCount) || im.Preedit==1) && im.CodeInput[0])
	{
		if(im.StringGetEngine[0] || (eim && eim->CaretPos>=0 && eim->CaretPos<eim->CodeLen))
		{
#ifdef _WIN32
			wchar_t temp[MAX_CAND_LEN+1];
			wcscpy(temp,(wchar_t*)im.StringGet);
			if(eim && eim->CaretPos>=0 && eim->CaretPos<eim->CodeLen)
			{
				wcsncat(temp,(wchar_t*)im.CodeInput,eim->CaretPos);
				wcscpy(temp+wcslen((wchar_t*)im.StringGet)+eim->CaretPos,L"|");
				wcscat(temp,(wchar_t*)im.CodeInput+eim->CaretPos);
			}
			else
			{
				wcscat(temp,(wchar_t*)im.CodeInput);
			}
#else
			uint8_t temp[MAX_CAND_LEN+1];
			strcpy((char*)temp,im.StringGet);
			if(eim && eim->CaretPos>=0 && eim->CaretPos<eim->CodeLen)
			{
				l_utf8_strncpy(temp+strlen((char*)temp),(uint8_t*)im.CodeInput,eim->CaretPos);
				strcat((char*)temp,"|");
				strcat((char*)temp,(char*)l_utf8_offset((uint8_t*)im.CodeInput,eim->CaretPos));
			}
			else
			{
				strcat((char*)temp,im.CodeInput);
			}
#endif
			//YongPreeditDraw((char*)temp,-1);
			y_xim_preedit_draw((char*)temp,-1);
		}
		else
		{
			y_xim_preedit_draw((char*)im.CodeInput,-1);
		}
	}
	else if(im.Preedit==0 && eim && eim->CandWordCount)
	{
		y_xim_preedit_draw(im.CandTable[eim->SelectIndex],-1);
	}
	return 0;
}

void *ui_main_win(void)
{
	if(!MainWin)
		return NULL;
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
	return gtk_widget_get_window(MainWin);
#else
	return MainWin->window;
#endif
}

int ui_main_show(int show)
{
	if(show && MainWin_X==0 && MainWin_Y<=0)
	{
		gint w,h;
		gint wa_x,wa_y,wa_w,wa_h;
		w=MainWin_W;h=MainWin_H;
		get_workarea(&wa_x,&wa_y,&wa_w,&wa_h);
		gtk_window_move(GTK_WINDOW(MainWin),wa_x+wa_w-w,wa_y+wa_h-h);
	}
	else if(show && MainWin_X==1 && MainWin_Y==-1)
	{
		gint w,h;
		MainWin_Y=0;
		w=MainWin_W;h=MainWin_H;
		MainWin_X=(gdk_screen_width()-w)/2;
		gtk_window_move(GTK_WINDOW(MainWin),MainWin_X,MainWin_Y);
		(void)h;
	}
	else if(show && MainWin_X==2 && MainWin_Y==-1)
	{
		gint w,h;
		gint wa_x,wa_y,wa_w,wa_h;
		w=MainWin_W;h=MainWin_H;
		get_workarea(&wa_x,&wa_y,&wa_w,&wa_h);
		gtk_window_move(GTK_WINDOW(MainWin),wa_x,wa_y+wa_h-h);
		(void)w;
	}
	else if(show)
	{
		gtk_window_move(GTK_WINDOW(MainWin),MainWin_X,MainWin_Y);
	}
	if(show>0)
	{
#if GTK_CHECK_VERSION(3,0,0)
		//gtk_window_resize(GTK_WINDOW(MainWin),MainWin_W,MainWin_H);
#endif
		gtk_widget_show(MainWin);
	}
	else if(show==0)
	{
		gtk_widget_hide(MainWin);
	}
	return 0;
}

static void *ui_input_win(void)
{
	if(!InputWin)
		return NULL;
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
	return gtk_widget_get_window(InputWin);
#else
	return InputWin->window;
#endif
}

int ui_input_show(int show)
{
	if(show)
	{
		if(ybus_ibus_input_draw(InputTheme.line)==0)
			return 0;
		// 在这移动窗口是为了修复gtk_widget_hide之后可能自动修改窗口位置的bug
		gtk_window_move(GTK_WINDOW(InputWin),InputWin_X,InputWin_Y);
#if GTK_CHECK_VERSION(3,0,0)
		// gtk3中有可能出现不显示的问题，怀疑是隐藏之后重新显示窗口大小被设成0了
		//gtk_window_resize(GTK_WINDOW(InputWin),InputTheme.RealWidth,InputTheme.RealHeight);
#endif
		gtk_widget_show(InputWin);
	}
	else
	{
		ybus_ibus_input_hide();	
		gtk_widget_hide(InputWin);
	}
	return 0;
}

static void on_status_popup_menu(GtkStatusIcon *icon,
	guint button,guint time,gpointer data)
{
#if 0
	if(!cfg_menu) return;
	ui_update_menu();
	gtk_menu_popup(GTK_MENU(cfg_menu),NULL,NULL,my_status_icon_position_menu,icon,button,time);
#else
	LKeyFile *kf=y_im_get_menu_config();
	ui_menu_t *m;
	m=ui_build_menu(kf);
	gtk_menu_popup(GTK_MENU(m->root),NULL,NULL,my_status_icon_position_menu,icon,button,time);
#endif
}

static void on_status_activate(void)
{
	y_xim_enable(-1);
}

static int ui_image_get_path(const char *file,char *out,int size)
{
	if(!file || !file[0])
	{
		out[0]=0;
		return 0;
	}
	snprintf(out,size,"%s/%s/%s",y_im_get_path("HOME"),skin_path,file);
	if(l_file_exists(out))
		return 0;
	snprintf(out,size,"%s/skin/%s",y_im_get_path("HOME"),file);
	if(l_file_exists(out))
		return 0;
	snprintf(out,size,"%s/%s/%s",y_im_get_path("DATA"),skin_path,file);
	if(l_file_exists(out))
		return 0;
	snprintf(out,size,"%s/skin/%s",y_im_get_path("DATA"),file);
	if(l_file_exists(out))
		return 0;
	return -1;
}

void ui_tray_update(UI_TRAY *param)
{
	int enable;
	int i;

	{
		int ret;
		char icon1[256],icon2[256];
		ret=ui_image_get_path(param->icon[0],icon1,sizeof(icon1));
		ret|=ui_image_get_path(param->icon[1],icon2,sizeof(icon2));
		if(ret==0)
		{
			if(icon1 && strstr(icon1,".zip/"))
			{
				snprintf(icon1,sizeof(icon1),"%s/skin/%s",y_im_get_path("DATA"),"tray1.png");
				snprintf(icon2,sizeof(icon2),"%s/skin/%s",y_im_get_path("DATA"),"tray2.png");
			}
			ybus_wm_icon(icon1,icon2);
		}
	}
	
	if(!my_status_icon_new_from_pixbuf)
	{
		my_status_icon_new_from_pixbuf=dlsym(NULL,"gtk_status_icon_new_from_pixbuf");
		if(!my_status_icon_new_from_pixbuf)
			return;
		my_status_icon_set_from_pixbuf=dlsym(NULL,"gtk_status_icon_set_from_pixbuf");
#if GTK_CHECK_VERSION(3,0,0)
		my_status_icon_set_tooltip=dlsym(NULL,"gtk_status_icon_set_tooltip_text");
#else
		my_status_icon_set_tooltip=dlsym(NULL,"gtk_status_icon_set_tooltip");
#endif
		my_status_icon_set_title=dlsym(NULL,"gtk_status_icon_set_title");
		my_status_icon_position_menu=dlsym(NULL,"gtk_status_icon_position_menu");
	}
	for(i=0;i<2;i++)
	{
		if(!IconPixbuf[i]) continue;
		g_object_unref(G_OBJECT(IconPixbuf[i]));
		IconPixbuf[i]=0;
	}
	enable=param->enable;
	if(!enable)
	{
		if(StatusIcon)
		{
			g_object_unref(G_OBJECT(StatusIcon));
			StatusIcon=NULL;
		}
		return;
	}
	if(!param->icon[0])
		return;
	for(i=0;i<2;i++)
	{
		if(param->icon[i])
		{
			IconPixbuf[i]=ui_image_load(param->icon[i]);
		}
	}
	if(StatusIcon)
	{
		ui_tray_status(IconSelected);
	}
	else
	{
		StatusIcon=my_status_icon_new_from_pixbuf(IconPixbuf[IconSelected]);
		g_signal_connect(G_OBJECT(StatusIcon),"popup-menu",
			G_CALLBACK(on_status_popup_menu),NULL);
		g_signal_connect(G_OBJECT(StatusIcon),"activate",
			G_CALLBACK(on_status_activate),NULL);
	}
}

void ui_tray_tooltip(const char *tip)
{
	char tip2[32];
	if(!StatusIcon) return;
	y_im_str_encode(tip,tip2,0);
	my_status_icon_set_tooltip(StatusIcon,tip2);
	if(my_status_icon_set_title)
		my_status_icon_set_title(StatusIcon,tip2);
}

void ui_tray_status(int which)
{
	if(which!=1 && which!=0)
		return;
	if(IconSelected==which)
		return;
	IconSelected=which;
	if(!StatusIcon) return;
	if(IconPixbuf[IconSelected])
	{
		my_status_icon_set_from_pixbuf(StatusIcon,IconPixbuf[IconSelected]);
	}
}

static gboolean YongSendFile_real(char *fn)
{
	if(strstr(fn,".txt"))
	{
		FILE *fp;
		fp=y_im_open_file(fn,"rb");
		if(fp)
		{
			char temp[4096];
			int len;
			len=fread(temp,1,sizeof(temp)-1,fp);
			temp[len]=0;
			fclose(fp);
			if(len>0)
			{
				GtkClipboard *cb;
				gchar *utf8;
				cb=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
				if(cb!=NULL)
				{
					utf8=g_malloc(len*2+1);
					y_im_str_encode(temp,utf8,0);
					gtk_clipboard_set_text(cb,utf8,-1);
					g_free(utf8);
					y_xim_forward_key(CTRL_V,1);
				}
			}
		}
	}
	else
	{
		GdkPixbuf *pb;		
		pb=ui_image_load(fn);
		if(pb)
		{
			GtkClipboard *cb;
			cb=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
			if(cb!=NULL)
			{
				gtk_clipboard_set_image(cb,pb);
				g_object_unref(pb);
				y_xim_forward_key(CTRL_V,1);
			}
		}
	}
	g_free(fn);
	return FALSE;
}

void YongSendFile(const char *fn)
{
	int len;
	char *utf8;
	if(!strcmp(y_xim_get_name(),"fbterm"))
		return;
	len=strlen(fn);
	utf8=g_malloc(len*2+1);
	y_im_str_encode(fn,utf8,0);
	g_idle_add_full(G_PRIORITY_DEFAULT,(GSourceFunc)YongSendFile_real,utf8,g_free);
}

static gboolean YongSendClipboard_real(char *utf8)
{
	GtkClipboard *cb;
	cb=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	if(cb!=NULL)
		gtk_clipboard_set_text(cb,utf8,-1);
	y_xim_forward_key(CTRL_V,1);
	return FALSE;
}

void YongSendClipboard(const char *s)
{
	gchar *utf8;
	int len;
	
	if(!strcmp(y_xim_get_name(),"fbterm"))
		return;

	len=strlen(s);
	utf8=g_malloc(len*2+1);
	y_im_str_encode(s,utf8,0);
	g_idle_add_full(G_PRIORITY_DEFAULT,(GSourceFunc)YongSendClipboard_real,utf8,g_free);
}

static void on_ui_get_select_cb(GtkClipboard *clipboard,const char*text,int(*cb)(const char*))
{
	char phrase[128];
	int ret;
	if(!text || strpbrk(text,"\r\n\t\v\b") || strlen(text)>50)
	{
		ret=cb(NULL);
	}
	else
	{
		y_im_str_encode_r(text,phrase);
		ret=cb(phrase);
	}
	if(ret==IMR_DISPLAY)
	{
		EXTRA_IM *eim=im.eim;
		y_im_key_desc_translate(eim->CodeInput,0,eim->CandTable[eim->SelectIndex],(char*)im.CodeInput,MAX_CODE_LEN+1);
		y_im_str_encode(s2t_conv(eim->StringGet),im.StringGet,0);
		y_ui_input_draw();
	}
}

char *ui_get_select(int (*cb)(const char *))
{
	GtkClipboard *clipboard;
	clipboard=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	if(!clipboard)
	{
		printf("yong: get clipboard fail\n");
		return NULL;
	}
	gtk_clipboard_request_text(clipboard,(GtkClipboardTextReceivedFunc)on_ui_get_select_cb,cb);
	return NULL;
}

static int menu_disable;
static void menu_activate(GtkMenuItem *item,gpointer data)
{
	int id=GPOINTER_TO_INT(data);
	if(id==im.Index) return;
	if(menu_disable) return;
	YongSwitchIM(id);
}

void ui_show_message(const char *s)
{
	GtkWidget *dlg;
	char temp[2048*3];
	
	y_im_str_encode(s,temp,0);
	
	dlg=gtk_message_dialog_new(
		GTK_WINDOW(MainWin),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		temp);
	gtk_window_set_position(GTK_WINDOW(dlg),GTK_WIN_POS_CENTER);
	y_im_str_encode(YT("Yong输入法"),temp,0);
	gtk_window_set_title(GTK_WINDOW(dlg),temp);
	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
}

#if 1//!GTK_CHECK_VERSION(3,0,0)
static UI_COLOR ui_sys_color(GtkWidget *w,int index)
{
	if(index==0)
		return ui_color_parse("#FFFFFF");
	else if(index==1)
		return ui_color_parse("#59b6e7");
	else
		return ui_color_parse("#0182e5");
}
#else
static UI_COLOR ui_sys_color(GtkWidget *w,int index)
{
	UI_COLOR c;
	GdkRGBA gc;
	GtkStyleContext *ctx=gtk_widget_get_style_context(w);
	if(index==0)
		gtk_style_context_get_background_color(ctx,GTK_STATE_NORMAL,&gc);
	else if(index==1)
		gtk_style_context_get_background_color(ctx,GTK_STATE_ACTIVE,&gc);
	else
		gtk_style_context_get_color(ctx,GTK_STATE_NORMAL,&gc);
	c.a=255;
	c.r=(uint8_t)(gc.red*255);
	c.g=(uint8_t)(gc.green*255);
	c.b=(uint8_t)(gc.blue*255);
	return c;
}
#endif

static gboolean on_tip_draw(GtkWidget *window,cairo_t *cr)
{
	UI_COLOR color;
	int w,h;
	const char *text;
	
	text=g_object_get_data(G_OBJECT(window),"tip");
	
	gtk_widget_get_size_request(window,&w,&h);
	
	cairo_set_antialias(cr,CAIRO_ANTIALIAS_NONE);
	cairo_set_line_width(cr,1.0);

	//color=ui_color_parse("#FFFFFF");
	color=ui_sys_color(window,0);
	ui_fill_rect(cr,0,0,w,h,color);
	//color=ui_color_parse("#CBCAE6");
	color=ui_sys_color(window,1);
	ui_draw_rect(cr,0,0,w,h,color,MainTheme.line_width);

	//color=ui_color_parse("#0042C8");
	color=ui_sys_color(window,2);
	if(text) ui_draw_text(cr,InputTheme.layout,5,5,text,color);
	
	return TRUE;
}

#if !GTK_CHECK_VERSION(3,0,0)
static gboolean tip_expose(GtkWidget *window,GdkEventExpose *event)
{
#ifdef GSEAL_ENABLE
	cairo_t *cr=gdk_cairo_create(gtk_widget_get_window(window));
#else
	cairo_t *cr=gdk_cairo_create(window->window);
#endif
	on_tip_draw(window,cr);
	cairo_destroy(cr);
	return TRUE;
}
#endif

static guint tip_timer_id;
static gboolean on_tip_timeout(GtkWidget *tip)
{
	tip_timer_id=0;
	g_object_set_data(G_OBJECT(tip),"tip",NULL);
	gtk_widget_hide(tip);
	return FALSE;
}

void ui_show_tip(const char *fmt,...)
{
	static GtkWidget *tip;
	int x,y,w,h;
	char gb[128];
	char text[128];
	va_list ap;
	
	if(!fmt || !fmt[0])
		return;
	text[0]=0;
	va_start(ap,fmt);
	vsnprintf(gb,sizeof(gb),YT(fmt),ap);
	va_end(ap);
	y_im_str_encode(gb,text+strlen(text),0);
	
	if(!tip)
	{
		tip=(GtkWidget*)gtk_window_new(GTK_WINDOW_POPUP);
		gtk_window_set_type_hint (GTK_WINDOW (tip), GDK_WINDOW_TYPE_HINT_TOOLTIP);
		gtk_widget_set_name (tip, "gtk-tooltip");
#if !GTK_CHECK_VERSION(3,0,0)
		g_signal_connect(G_OBJECT(tip),"expose-event",
			G_CALLBACK(tip_expose),0);
#else
		g_signal_connect(G_OBJECT(tip),"draw",
			G_CALLBACK(on_tip_draw),0);
#endif
	}
	g_object_set_data_full(G_OBJECT(tip),"tip",g_strdup(text),g_free);
	ui_text_size(NULL,InputTheme.layout,text,&w,&h);
	w+=10;h+=10;
	
	CONNECT_ID *id=y_xim_get_connect();
	if(id && id->x>0 && id->y>0 && id->x!=POSITION_ORIG && id->y!=POSITION_ORIG)
	{
		x=id->x;
		y=id->y;
	}
	else
	{
		x=(gdk_screen_width()-w)/2;
		y=(gdk_screen_height()-h)/2;
	}
	gtk_window_move(GTK_WINDOW(tip),x,y);
	gtk_window_resize(GTK_WINDOW(tip),w,h);
	gtk_widget_set_size_request(tip,w,h);
	gtk_widget_queue_draw(tip);
	gtk_widget_show(tip);
	if(tip_timer_id>0)
		g_source_remove(tip_timer_id);
	tip_timer_id=g_timeout_add(1000,(GSourceFunc)on_tip_timeout,tip);
}

#if 0
static void speed_stat(void)
{
	GtkWidget *dlg;
	char temp[1500];
	
	y_im_str_encode(y_im_speed_stat(),temp,0);
	
	dlg=gtk_message_dialog_new(
		GTK_WINDOW(MainWin),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		temp);
	gtk_window_set_title(GTK_WINDOW(dlg),YT("Yong输入法"));
	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
}

static void do_optimize(void)
{
	int ret;
	void *out=NULL;
	ret=y_im_run_tool("tool_save_user",0,0);
	if(ret!=0) return;
	ret=y_im_run_tool("tool_get_file","main",&out);
	if(ret!=0 || !out) return;
	y_im_backup_file(out,".bak");
	y_im_run_tool("tool_optimize",0,0);
	ui_show_message(YT("完成"));
}

static void do_merge_user(void)
{
	int ret;
	void *out=NULL;
	ret=y_im_run_tool("tool_save_user",0,0);
	if(ret!=0) return;
	ret=y_im_run_tool("tool_get_file","main",&out);
	if(ret!=0 || !out) return;
	y_im_backup_file(out,".bak");
	ret=y_im_run_tool("tool_get_file","user",&out);
	if(ret!=0 || !out) return;
	y_im_backup_file(out,".bak");
	ret=y_im_run_tool("tool_merge_user",0,0);
	if(ret!=0) return;
	y_im_remove_file(out);
	YongReloadAll();
	ui_show_message(YT("完成"));
}

static void do_edit_main(void)
{
	int ret;
	void *out=NULL;
	char *ed;
	char temp[256];
	ed=y_im_get_config_string("table","edit");
	if(!ed) return;
	ret=y_im_run_tool("tool_get_file","main",&out);
	if(ret!=0 || !out)
	{
		g_free(ed);
		return;
	}
	out=y_im_auto_path(out);
	sprintf(temp,"%s %s",ed,(char*)out);
	y_im_run_helper(temp,out,YongReloadAll);
	g_free(ed);
	g_free(out);
}
#endif

#if 0
static const char *get_gb18030_support()
{
	char temp[8];
	temp[0]=0;
	l_gb_to_utf8("\x98\x39\x9f\x38",temp,sizeof(temp));
	if(temp[0])
		return "GB18030";
	l_gb_to_utf8("\x98\x35\xf7\x38",temp,sizeof(temp));
	if(temp[0])
		return "CJK-C";
	l_gb_to_utf8("\x95\x32\x82\x36",temp,sizeof(temp));
	if(temp[0])
		return "CJK-B";
	l_gb_to_utf8("\x81\x30\x81\x30",temp,sizeof(temp));
	if(temp[0])
		return "CJK-A";
	l_gb_to_utf8("镕",temp,sizeof(temp));
	if(temp[0])
		return "GBK";
	l_gb_to_utf8("的",temp,sizeof(temp));
	if(temp[0])
		return "GB2312";
	return "NOTHING";
}
#endif

static void show_system_info(void)
{
	char temp[1024];
	int pos=0;
	const char *p;
	p=getenv("LANG");if(!p) p="";
	pos+=sprintf(temp+pos,"LANG=%s\n",p);
	p=getenv("LC_CTYPE");if(!p) p="";
	pos+=sprintf(temp+pos,"LC_CTYPE=%s\n",p);
	p=getenv("XMODIFIERS");if(!p) p="";
	pos+=sprintf(temp+pos,"XMODIFIERS=%s\n",p);
	p=getenv("GTK_IM_MODULE");if(!p) p="";
	pos+=sprintf(temp+pos,"GTK_IM_MODULE=%s\n",p);
	p=getenv("QT_IM_MODULE");if(!p) p="";
	/*pos+=*/sprintf(temp+pos,"QT_IM_MODULE=%s\n",p);
	//p=get_gb18030_support();
	//pos+=sprintf(temp+pos,"SUPPORT=%s\n",p);
	ui_show_message(temp);
}

#if 0
static GtkWidget *im_manage_menu(void)
{
	char *engine;
	GtkWidget *pop,*item;
	char keymap[128];
	char name[128];
	
	pop=gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(im_manage),pop);
	
	y_im_str_encode(YT("输入统计"),name,0);
	item=gtk_menu_item_new_with_label(name);
	g_signal_connect(G_OBJECT(item),"activate",
		G_CALLBACK(speed_stat),NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
	gtk_widget_show(item);
	
	y_im_str_encode(YT("系统信息"),name,0);
	item=gtk_menu_item_new_with_label(name);
	g_signal_connect(G_OBJECT(item),"activate",
		G_CALLBACK(show_system_info),NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
	gtk_widget_show(item);
	
	if(0==y_im_get_keymap(keymap,128))
	{
		item=gtk_menu_item_new_with_label(keymap);
		g_signal_connect(G_OBJECT(item),"activate",
			G_CALLBACK(y_im_show_keymap),NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
		gtk_widget_show(item);
	}
	
	engine=y_im_get_current_engine();
	if(engine && !strcmp(engine,"libmb.so"))
	{
		y_im_str_encode(YT("码表优化"),name,0);
		item=gtk_menu_item_new_with_label(name);
		g_signal_connect(G_OBJECT(item),"activate",
			G_CALLBACK(do_optimize),NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
		gtk_widget_show(item);
		y_im_str_encode(YT("合并用户码表"),name,0);
		item=gtk_menu_item_new_with_label(name);
		g_signal_connect(G_OBJECT(item),"activate",
			G_CALLBACK(do_merge_user),NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
		gtk_widget_show(item);
		if(y_im_has_config("table","edit"))
		{
			y_im_str_encode(YT("编辑码表"),name,0);
			item=gtk_menu_item_new_with_label(name);
			g_signal_connect(G_OBJECT(item),"activate",
				G_CALLBACK(do_edit_main),NULL);
			gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
			gtk_widget_show(item);
		}
	}
	
	return pop;
}
#endif

#if 0
static void im_show_help_main(void)
{
	y_im_show_help("main");
}

static void im_show_help_cur(void)
{
	char item[64];
	if(!y_im_get_current(item,sizeof(item)))
		y_im_show_help(item);
}
#endif

#if 0
static GtkWidget *im_help_menu(GtkWidget *parent)
{
	GtkWidget *pop,*item;
	char desc[128];
	char cur[64];
	
	pop=gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent),pop);
	
	if(!y_im_help_desc("main",desc,128))
	{
		item=gtk_menu_item_new_with_label(desc);
		g_signal_connect(G_OBJECT(item),"activate",
			G_CALLBACK(im_show_help_main),0);
		gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
		gtk_widget_show(item);
	}
	if(!y_im_get_current(cur,64) && !y_im_help_desc(cur,desc,128))
	{
		item=gtk_menu_item_new_with_label(desc);
		g_signal_connect(G_OBJECT(item),"activate",
			G_CALLBACK(im_show_help_cur),0);
		gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
		gtk_widget_show(item);
	}
	
	y_im_str_encode(YT("关于"),desc,0);
	item=gtk_menu_item_new_with_label(desc);
	g_signal_connect(G_OBJECT(item),"activate",
		G_CALLBACK(y_im_about_self),NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(pop),item);
	gtk_widget_show(item);
	
	return pop;
}
#endif

static void menu_set_default(void)
{
	y_im_set_default(im.Index);
}

void ui_update_menu(void)
{
}


static GtkWidget *im_list_menu(void)
{
	GtkWidget *menu,*item=NULL;
	int i;
	char name[128];	
	
	menu=gtk_menu_new();
	menu_disable=1;	
	
	for(i=0;i<32;i++)
	{
		char *tmp;
		tmp=y_im_get_im_name(i);
		if(!tmp) break;
		l_gb_to_utf8(tmp,name,sizeof(name));
		l_free(tmp);
		if(!item)			
			item=gtk_radio_menu_item_new_with_label(NULL,name);
		else
			item=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item),name);
		g_signal_connect(G_OBJECT(item),"activate",
			G_CALLBACK(menu_activate),GINT_TO_POINTER(i));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),item);
		gtk_widget_show(item);
		if(im.Index==i)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),TRUE);
	}
	item=gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),item);
	gtk_widget_show(item);
	y_im_str_encode(YT("默认"),name,0);
	item=gtk_menu_item_new_with_label(name);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),item);
	gtk_widget_show(item);
	g_signal_connect(G_OBJECT(item),"activate",
		G_CALLBACK(menu_set_default),GINT_TO_POINTER(i));

	menu_disable=0;

	return menu;
}

static void ui_menu_on_cmd(GtkMenuItem *item,gpointer user_data)
{
	ui_menu_t *m;
	int i;
	//printf("active\n");
	m=g_object_get_data(G_OBJECT(item),"m");
	i=GPOINTER_TO_INT(user_data);
	y_im_handle_menu(m->cmd[i]);
}

static void ui_menu_free(ui_menu_t *m);

static void  ui_menu_on_done(GtkMenuShell *menushell,ui_menu_t *m)
{
	//printf("hide\n");
	ui_menu_free(m);
}

static void ui_add_menu(ui_menu_t *m,GtkWidget *parent,const char *group)
{
	char *child,*name;
	char temp[128];
	GtkWidget *item;
	
	if(!strcmp(group,"-"))
	{
		item=gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(parent),item);
		gtk_widget_show(item);
		return;
	}
	name=l_key_file_get_string(m->config,group,"name");
	if(name)
	{
		char gb[128];
		l_utf8_to_gb(name,gb,sizeof(gb));
		l_gb_to_utf8(YT(gb),temp,sizeof(temp));
		l_free(name);
	}
	else
	{
		temp[0]=0;
	}
	child=l_key_file_get_string(m->config,group,"child");
	if(child)
	{
		char **list;
		int i;
		GtkWidget *me=gtk_menu_new();
		if(parent)
		{
			item=gtk_menu_item_new_with_label(temp);
			gtk_menu_shell_append(GTK_MENU_SHELL(parent),item);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),me);
			gtk_widget_show(item);
		}
		list=l_strsplit(child,' ');
		l_free(child);
		for(i=0;list[i]!=NULL;i++)
		{
			ui_add_menu(m,me,list[i]);
		}
		l_strfreev(list);
		if(!parent)
		{
			m->root=me;
			g_object_ref_sink(m->root);
			g_signal_connect(G_OBJECT(me),"selection-done",
						G_CALLBACK(ui_menu_on_done),m);
		}
	}
	else
	{
		char *exec;
		if(m->count>=64)
			return;
		exec=l_key_file_get_string(m->config,group,"exec");
		if(!exec)
			return;
		if(!m->mb && (!strcmp(exec,"$MBO") || !strcmp(exec,"$MBM") || 
						!strcmp(exec,"$MBEDIT")))
		{
			l_free(exec);
			return;
		}
		if(!strcmp(exec,"$OUTPUT"))
		{
			l_free(exec);
			return;
		}
		if(!strcmp(exec,"$MBEDIT") && !y_im_has_config("table","edit"))
		{
			l_free(exec);
			return;
		}
		if(!strncmp(exec,"$GO(yong-config ",16) && !l_file_exists("/usr/bin/yong-config"))
		{
			l_free(exec);
			return;
		}
		if(!strcmp(exec,"$IMLIST"))
		{
			l_free(exec);
			int ybus_ibus_use_ibus_menu(void);
			if(!ybus_ibus_use_ibus_menu())
			{
				GtkWidget *me;
				me=im_list_menu();
				item=gtk_menu_item_new_with_label(temp);
				gtk_menu_shell_append(GTK_MENU_SHELL(parent),item);
				gtk_widget_show(item);
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),me);
			}
		}
		else
		{
			if(!strcmp(exec,"$KEYMAP"))
			{
				char keymap[128];
				if(0!=y_im_get_keymap(keymap,128))
				{
					l_free(exec);
					return;
				}
				snprintf(temp,sizeof(temp),"%s",keymap);
			}
			else if(!strcmp(exec,"$HELP(main)"))
			{
				char desc[128];
				if(y_im_help_desc("main",desc,128)!=0)
				{
					l_free(exec);
					return;
				}
				snprintf(temp,sizeof(temp),"%s",desc);
			}
			else if(!strcmp(exec,"$HELP(?)"))
			{
				char item[64];
				char desc[128];
				if(y_im_get_current(item,64) || y_im_help_desc(item,desc,128))
				{
					l_free(exec);
					return;
				}
				snprintf(temp,sizeof(temp),"%s",desc);
			}
			item=gtk_menu_item_new_with_label(temp);
			gtk_menu_shell_append(GTK_MENU_SHELL(parent),item);
			gtk_widget_show(item);
			if(!strcmp(exec,"$EXIT"))
			{
				g_signal_connect(G_OBJECT(item),"activate",
						G_CALLBACK(gtk_main_quit),NULL);
			}
			else if(!strcmp(exec,"$SYSINFO"))
			{
				g_signal_connect(G_OBJECT(item),"activate",
						G_CALLBACK(show_system_info),NULL);
			}
			else
			{
				g_object_set_data(G_OBJECT(item),"m",m);
				g_signal_connect(G_OBJECT(item),"activate",
						G_CALLBACK(ui_menu_on_cmd),GINT_TO_POINTER(m->count));
			}
			m->cmd[m->count++]=exec;
		}
	}
}

static ui_menu_t *ui_build_menu(LKeyFile *kf)
{
	ui_menu_t *m;
	char *engine;
		
	m=l_new0(ui_menu_t);
	m->config=kf;
	m->base=1500;
	engine=y_im_get_current_engine();
	if(engine && !strcmp(engine,"libmb.so"))
		m->mb=1;
	ui_add_menu(m,m->root,"root");
	return m;
}

static void ui_menu_free(ui_menu_t *m)
{
	int i;
	if(!m)
		return;
	for(i=0;i<m->count;i++)
		l_free(m->cmd[i]);
	if(m->root)
	{
		gtk_widget_destroy(m->root);
		g_object_unref(m->root);
	}
	l_free(m);
}

static void ui_popup_menu(void)
{
	LKeyFile *kf=y_im_get_menu_config();
	ui_menu_t *m;
	m=ui_build_menu(kf);
	gtk_menu_popup(GTK_MENU(m->root),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
}

static int ui_calc_with_metrics(const char *s)
{
	char expr[256];
	int pos=0,i,c;
	GdkScreen *scr=gdk_screen_get_default();	
	gint wa_x=0,wa_y=0,wa_w=0,wa_h=0;
		
	for(i=0;(c=s[i])!='\0';i++)
	{
		if(pos>200)
			return -1;
		if(c=='S')
		{
			switch(s[i+1]){
			case 'W':
				pos+=sprintf(expr+pos,"%d",gdk_screen_get_width(scr));
				i++;
				break;
			case 'w':
				if(wa_w==0)
					get_workarea(&wa_x,&wa_y,&wa_w,&wa_h);
				pos+=sprintf(expr+pos,"%d",wa_w);
				i++;
				break;
			case 'H':
				pos+=sprintf(expr+pos,"%d",gdk_screen_get_height(scr));
				i++;
				break;
			case 'h':
				if(wa_h==0)
					get_workarea(&wa_x,&wa_y,&wa_w,&wa_h);
				pos+=sprintf(expr+pos,"%d",wa_h);
				i++;
				break;
			default:
				return -1;
			}
		}
		else
		{
			expr[pos++]=c;
		}
	}
	expr[pos]=0;
	LVariant v=l_expr_calc(expr);
	if(v.type==L_TYPE_INT)
		return v.v_int;
	else if(v.type==L_TYPE_FLOAT)
		return v.v_float;
	else
		return -1;
}

void ui_cfg_ctrl(char *name,...)
{
	va_list ap;
	va_start(ap,name);
	if(!strcmp(name,"strip"))
	{
		InputTheme.strip=va_arg(ap,int);
	}
	else if(!strcmp(name,"calc"))
	{
		const char *s=va_arg(ap,const char*);
		int *res=va_arg(ap,int*);
		if(res)
			*res=ui_calc_with_metrics(s);
	}
	else if(!strcmp(name,"onspot"))
	{
		InputTheme.onspot=va_arg(ap,int);
	}
	va_end(ap);
}

void ui_clean(void)
{
	int i;

	/* clean input window */
	for(i=0;i<3;i++)
	{
		if(InputTheme.bg[i])
		{
			g_object_unref(InputTheme.bg[i]);
			InputTheme.bg[i]=NULL;
		}
		if(InputTheme.rgn[i])
		{
			//gdk_region_destroy(InputTheme.rgn[i]);
			ui_region_destroy(InputTheme.rgn[i]);
			InputTheme.rgn[i]=NULL;
		}
	}
	
	gtk_widget_destroy(InputWin);
	gtk_widget_destroy(MainWin);
}

static gboolean on_image_win_draw(GtkWidget *widget,cairo_t *cr)
{
	ui_image_draw(cr,ImageWin_bg,0,0);
	return TRUE;
}

#if !GTK_CHECK_VERSION(3,0,0)
static gboolean image_win_expose(GtkWidget *widget)
{
#ifdef GSEAL_ENABLE
	cairo_t *cr=gdk_cairo_create(gtk_widget_get_window(widget));
#else
	cairo_t *cr=gdk_cairo_create(widget->window);
#endif
	on_image_win_draw(widget,cr);
	cairo_destroy(cr);
	return TRUE;
}
#endif

void ui_show_image(char *name,char *file,int top,int tran)
{
	GdkWindow *window;
	static char *image_file;
	if(!ImageWin)
	{
		ImageWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		//gtk_window_set_modal(GTK_WINDOW(ImageWin),FALSE);
		gtk_window_set_position(GTK_WINDOW(ImageWin),GTK_WIN_POS_CENTER);
		gtk_window_set_accept_focus(GTK_WINDOW(ImageWin),FALSE);
		g_signal_connect(ImageWin,"delete-event",G_CALLBACK(gtk_widget_hide_on_delete),NULL);
#if !GTK_CHECK_VERSION(3,0,0)
		g_signal_connect(ImageWin,"expose-event",G_CALLBACK(image_win_expose),NULL);
#else
		g_signal_connect(ImageWin,"draw",G_CALLBACK(on_image_win_draw),NULL);
#endif
	}
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
	if(!gtk_widget_get_realized(ImageWin))
#else
	if(!GTK_WIDGET_REALIZED(ImageWin))
#endif
	{
		gtk_widget_realize(ImageWin);
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
		window=gtk_widget_get_window(ImageWin);
#else
		window=ImageWin->window;
#endif
		gdk_window_set_functions(window,GDK_FUNC_MOVE|GDK_FUNC_MINIMIZE|GDK_FUNC_CLOSE);
	}
	else
	{
#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
		window=gtk_widget_get_window(ImageWin);
#else
		window=ImageWin->window;
#endif
	}
	if(gdk_window_is_visible(window))
	{
		gtk_widget_hide(ImageWin);
		return;
	}
	gtk_window_set_title(GTK_WINDOW(ImageWin),name);
	gtk_window_set_keep_above(GTK_WINDOW(ImageWin),top);
	ui_win_tran(window,tran);
	if(!image_file || strcmp(image_file,file))
	{
		free(image_file);
		image_file=strdup(file);
		if(ImageWin_bg) ui_image_free(ImageWin_bg);
		ImageWin_bg=ui_image_load(image_file);
		if(ImageWin_bg)
		{
			int width,height;
			ui_image_size(ImageWin_bg,&width,&height);
			//gtk_widget_set_size_request(ImageWin,width,height);
			gtk_window_resize(GTK_WINDOW(ImageWin),width,height);
		}
	}
	gtk_widget_show(ImageWin);
	gtk_window_deiconify(GTK_WINDOW(ImageWin));
}

static gboolean ca_gtk_play(const gchar *id)
{
	my_ca_gtk_play_for_widget(InputWin,1,"event.id",id,NULL);
	return TRUE;
}

static void ui_beep(int c)
{
	if(my_ca_gtk_play_for_widget)
	{
		const gchar *id="window-attention";
		if(c==YONG_BEEP_MULTI)
			id="window-question";
		if(ca_gtk_play(id))
			return;
	}
#if GTK_CHECK_VERSION(3,0,0)
	gdk_window_beep(gtk_widget_get_window(InputWin));
#else
	gdk_window_beep(InputWin->window);
#endif
}

static int ui_request(int cmd)
{
	unsigned char tmp=(unsigned char)cmd;
	write(im_pipe[1],&tmp,1);
	return 0;
}

static double ui_get_scale(void)
{
	return ui_scale;
}

void ui_setup_default(Y_UI *p)
{
	p->init=ui_init;
	p->loop=ui_loop;
	p->clean=ui_clean;
	
	p->main_update=ui_main_update;
	p->main_win=ui_main_win;
	p->main_show=ui_main_show;
	
	p->input_win=ui_input_win;
	p->input_update=ui_input_update;
	p->input_draw=YongDrawInput;
	p->input_redraw=ui_input_redraw;
	p->input_show=ui_input_show;
	p->input_move=ui_input_move;
	
	p->button_update=ui_button_update;
	p->button_show=ui_button_show;
	p->button_label=ui_button_label;
	
	p->tray_update=ui_tray_update;
	p->tray_status=ui_tray_status;
	p->tray_tooltip=ui_tray_tooltip;
	
	p->get_select=ui_get_select;
	
	p->update_menu=ui_update_menu;
	p->skin_path=ui_skin_path;
	p->cfg_ctrl=ui_cfg_ctrl;
	p->show_message=ui_show_message;
	p->show_image=ui_show_image;
	p->show_tip=ui_show_tip;
	
	p->beep=ui_beep;
	p->request=ui_request;
	
	p->get_scale=ui_get_scale;
	
	p->timer_add=ui_timer_add;
	p->timer_del=ui_timer_del;
	p->idle_add=ui_idle_add;
	p->idle_del=ui_idle_del;
}
