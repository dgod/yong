#include <string.h>
#include <gtk/gtk.h>
#include <pango/pango.h>

#include "ybutton.h"

#define ACTIVATE_TIMEOUT 250

enum{
	PRESSED,
	RELEASED,
	CLICKED,
	ENTER,
	LEAVE,
	ACTIVATE,
	LAST_SIGNAL
};

#define GTK_YBUTTON_GET_PRIVATE(o)       (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_YBUTTON, GtkYButtonPrivate))

typedef struct _GtkYButtonPrivate{
	guint has_grab;
	guint32 grab_time;
}GtkYButtonPrivate;

static void gtk_ybutton_destroy(GtkObject *object);
static void gtk_ybutton_realize(GtkWidget *widget);
static void gtk_ybutton_unrealize(GtkWidget *widget);
static void gtk_ybutton_map(GtkWidget *widget);
static void gtk_ybutton_unmap(GtkWidget *widget);
static void gtk_ybutton_size_request(GtkWidget *widget,GtkRequisition *requisition);
static void gtk_ybutton_size_allocate(GtkWidget *widget,GtkAllocation *allocation);
static gint gtk_ybutton_expose(GtkWidget *widget,GdkEventExpose *event);
static gint gtk_ybutton_button_press(GtkWidget *widget,GdkEventButton *event);
static gint gtk_ybutton_button_release(GtkWidget *widget,GdkEventButton *event);
static gint gtk_ybutton_grab_broken(GtkWidget *widget,GdkEventGrabBroken *event);
static gint gtk_ybutton_key_release(GtkWidget *widget,GdkEventKey *event);
static gint gtk_ybutton_enter_notify(GtkWidget *widget,GdkEventCrossing *event);
static gint gtk_ybutton_leave_notify(GtkWidget *widget,GdkEventCrossing *event);
static void gtk_real_ybutton_pressed(GtkYButton *button);
static void gtk_real_ybutton_released(GtkYButton *button);
static void gtk_real_ybutton_activate(GtkYButton *button);
static void gtk_ybutton_update_state(GtkYButton  *button);
static void gtk_ybutton_finish_activate(GtkYButton *button,gboolean do_it);
static GObject*	gtk_ybutton_constructor(GType type,guint n,GObjectConstructParam *params);
static void gtk_ybutton_state_changed(GtkWidget *widget,GtkStateType prev);
static void gtk_ybutton_grab_notify(GtkWidget *widget,gboolean grabbed);

static guint button_signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkYButton, gtk_ybutton, GTK_TYPE_WIDGET)

#ifndef gtk_marshal_VOID__VOID
#define gtk_marshal_VOID__VOID g_cclosure_marshal_VOID__VOID
#endif

static void gtk_ybutton_class_init (GtkYButtonClass *klass)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	gobject_class = G_OBJECT_CLASS (klass);
	object_class = (GtkObjectClass*) klass;
	widget_class = (GtkWidgetClass*) klass;
  
	gobject_class->constructor = gtk_ybutton_constructor;
	object_class->destroy = gtk_ybutton_destroy;

	widget_class->realize = gtk_ybutton_realize;
	widget_class->unrealize = gtk_ybutton_unrealize;
	widget_class->map = gtk_ybutton_map;
	widget_class->unmap = gtk_ybutton_unmap;
	widget_class->size_request = gtk_ybutton_size_request;
	widget_class->size_allocate = gtk_ybutton_size_allocate;
	widget_class->expose_event = gtk_ybutton_expose;
	widget_class->button_press_event = gtk_ybutton_button_press;
	widget_class->button_release_event = gtk_ybutton_button_release;
	widget_class->grab_broken_event = gtk_ybutton_grab_broken;
	widget_class->key_release_event = gtk_ybutton_key_release;
	widget_class->enter_notify_event = gtk_ybutton_enter_notify;
	widget_class->leave_notify_event = gtk_ybutton_leave_notify;
	widget_class->state_changed = gtk_ybutton_state_changed;
	widget_class->grab_notify = gtk_ybutton_grab_notify;

	klass->pressed = gtk_real_ybutton_pressed;
	klass->released = gtk_real_ybutton_released;
	klass->clicked = NULL;
	klass->enter = gtk_ybutton_update_state;
	klass->leave = gtk_ybutton_update_state;
	klass->activate = gtk_real_ybutton_activate;

	button_signals[PRESSED] =
		g_signal_new ("pressed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GtkYButtonClass, pressed),
		NULL, NULL,
		gtk_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	button_signals[RELEASED] =
    		g_signal_new ("released",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GtkYButtonClass, released),
		NULL, NULL,
		gtk_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	button_signals[CLICKED] =
		g_signal_new ("clicked",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (GtkYButtonClass, clicked),
		NULL, NULL,
		gtk_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	button_signals[ENTER] =
		g_signal_new ("enter",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GtkYButtonClass, enter),
		NULL, NULL,
		gtk_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	button_signals[LEAVE] =
		g_signal_new ("leave",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GtkYButtonClass, leave),
		NULL, NULL,
		gtk_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	button_signals[ACTIVATE] =
		g_signal_new ("activate",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (GtkYButtonClass, activate),
		NULL, NULL,
		gtk_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	widget_class->activate_signal = button_signals[ACTIVATE];

	g_type_class_add_private(gobject_class, sizeof(GtkYButtonPrivate));
}

static void gtk_ybutton_init(GtkYButton *button)
{
#ifndef GSEAL_ENABLE
	GTK_WIDGET_SET_FLAGS(button, GTK_NO_WINDOW);
#else
	gtk_widget_set_has_window(GTK_WIDGET(button),FALSE);
#endif
	button->in_button = FALSE;
	button->button_down = FALSE;
	button->depressed = FALSE;
	button->depress_on_activate = TRUE;
	button->layout=NULL;
	button->bg[0]=NULL;
	button->bg[1]=NULL;
	button->bg[2]=NULL;
}

static void gtk_ybutton_destroy(GtkObject *object)
{  
	GtkYButton *button=GTK_YBUTTON(object);
	int i;
	if (button->layout)
	{
    	g_object_unref (button->layout);
		button->layout=0;
	}
	for(i=0;i<3;i++)
	if(button->bg[i])
	{
		g_object_unref(button->bg[i]);
		button->bg[i]=NULL;
	}
	(* GTK_OBJECT_CLASS (gtk_ybutton_parent_class)->destroy)(object);
}

static GObject *gtk_ybutton_constructor (GType type,guint n,GObjectConstructParam *params)
{
	return (* G_OBJECT_CLASS(gtk_ybutton_parent_class)->constructor)(type,n,params);
}

GtkWidget *gtk_ybutton_new(void)
{
	return g_object_new(GTK_TYPE_YBUTTON, NULL);
}

void gtk_ybutton_pressed(GtkYButton *button)
{
	if(!GTK_IS_YBUTTON (button)) return;
	g_signal_emit(button, button_signals[PRESSED], 0);
}

void gtk_ybutton_released(GtkYButton *button)
{
	if(!GTK_IS_YBUTTON (button)) return;
	g_signal_emit(button, button_signals[RELEASED], 0);
}

void gtk_ybutton_clicked(GtkYButton *button)
{
	if(!GTK_IS_YBUTTON (button)) return;
	g_signal_emit(button, button_signals[CLICKED], 0);
}

void gtk_ybutton_enter(GtkYButton *button)
{
	if(!GTK_IS_YBUTTON (button)) return;
	g_signal_emit(button, button_signals[ENTER], 0);
}

void gtk_ybutton_leave(GtkYButton *button)
{
	if(!GTK_IS_YBUTTON (button)) return;
	g_signal_emit(button, button_signals[LEAVE], 0);
}

static void gtk_ybutton_realize (GtkWidget *widget)
{
	GtkAllocation allocation;
	GtkYButton *button;
	GdkWindowAttr attr;
	GdkWindow *window;

	button = GTK_YBUTTON (widget);
#ifdef GSEAL_ENABLE
	gtk_widget_set_realized(widget,GTK_REALIZED);
#else
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
#endif

#ifdef GSEAL_ENABLE
	gtk_widget_get_allocation(widget,&allocation);
#else
	allocation=widget->allocation;
#endif

	attr.window_type = GDK_WINDOW_CHILD;
	attr.x = allocation.x;
	attr.y = allocation.y;
	attr.width = allocation.width;
	attr.height = allocation.height;
	attr.wclass = GDK_INPUT_ONLY;
	attr.event_mask = gtk_widget_get_events (widget);
	attr.event_mask |= (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

	window=gtk_widget_get_parent_window(widget);
#ifdef GSEAL_ENABLE
	gtk_widget_set_window(widget,window);
#else
	widget->window = window;
#endif
	g_object_ref (window);
  
	button->event_window = gdk_window_new(window,&attr,GDK_WA_X|GDK_WA_Y);
	gdk_window_set_user_data (button->event_window, button);

#ifdef GSEAL_ENABLE
	gtk_widget_style_attach(widget);
#else
	widget->style = gtk_style_attach(widget->style,window);
#endif
}

static void gtk_ybutton_unrealize (GtkWidget *widget)
{
	GtkYButton *button = GTK_YBUTTON (widget);

	if(button->activate_timeout)
		gtk_ybutton_finish_activate(button, FALSE);
	if(button->event_window)
	{
		gdk_window_set_user_data(button->event_window, NULL);
		gdk_window_destroy(button->event_window);
		button->event_window = NULL;
	}
	GTK_WIDGET_CLASS(gtk_ybutton_parent_class)->unrealize(widget);
}

static void gtk_ybutton_map(GtkWidget *widget)
{
	GtkYButton *button = GTK_YBUTTON (widget);
  
	GTK_WIDGET_CLASS(gtk_ybutton_parent_class)->map(widget);
	if(button->event_window)
		gdk_window_show(button->event_window);
}

static void gtk_ybutton_unmap (GtkWidget *widget)
{
	GtkYButton *button = GTK_YBUTTON (widget);
    
	if(button->event_window)
		gdk_window_hide(button->event_window);
	GTK_WIDGET_CLASS(gtk_ybutton_parent_class)->unmap(widget);
}

static void gtk_ybutton_size_request (GtkWidget *widget,GtkRequisition *requisition)
{
	GtkAllocation allocation;
#ifdef GSEAL_ENABLE
	gtk_widget_get_allocation(widget,&allocation);
#else
	allocation=widget->allocation;
#endif
	requisition->width = allocation.width;
	requisition->height = allocation.height;
}

static void gtk_ybutton_size_allocate (GtkWidget *widget,GtkAllocation *allocation)
{
	GtkYButton *button=GTK_YBUTTON (widget);

#ifdef GSEAL_ENABLE
	gtk_widget_set_allocation(widget,allocation);
#else
	widget->allocation=*allocation;
#endif
#ifdef GSEAL_ENABLE
	if(!gtk_widget_get_realized(widget)) return;
#else
	if(!GTK_WIDGET_REALIZED(widget)) return;
#endif
	gdk_window_move_resize(button->event_window,
		    allocation->x,allocation->y,
		    allocation->width,allocation->height);
}

static gboolean gtk_ybutton_expose (GtkWidget *widget,GdkEventExpose *event)
{
	GtkYButton *btn=GTK_YBUTTON(widget);
	GdkGC *gc;
	GdkPixbuf *pixbuf;
	GtkStyle *style;
	GtkStateType state;
	GdkWindow *window;
	GtkAllocation allocation;

#ifndef GSEAL_ENABLE
	if(!GTK_WIDGET_DRAWABLE (widget)) return FALSE;
#else
	if(!gtk_widget_is_drawable(widget)) return FALSE;
#endif
#ifdef GSEAL_ENABLE
	style=gtk_widget_get_style(widget);
	state=gtk_widget_get_state(widget);
	window=gtk_widget_get_window(widget);
	gtk_widget_get_allocation(widget,&allocation);
#else
	style=widget->style;
	state=GTK_WIDGET_STATE(widget);
	window=widget->window;
	allocation=widget->allocation;
#endif
		
	if(state==GTK_STATE_ACTIVE)
		pixbuf=btn->bg[YBUTTON_DOWN];
	else if(state==GTK_STATE_PRELIGHT)
		pixbuf=btn->bg[YBUTTON_OVER];
	else
		pixbuf=btn->bg[YBUTTON_NORMAL];
	if(!pixbuf) pixbuf=btn->bg[YBUTTON_NORMAL];
	
	gc=gdk_gc_new(window);
	
	if(pixbuf)
	{
		gdk_draw_pixbuf(window,gc,pixbuf,
			0,0,allocation.x,allocation.y,-1,-1, GDK_RGB_DITHER_NONE,0,0);
	}
		
	if(btn->layout)
	{
		gint x,y,w,h;
		pango_layout_get_pixel_size(btn->layout,&w,&h);
		x=allocation.x+(allocation.width-w)/2;
		y=allocation.y+(allocation.height-h)/2;
		gdk_draw_layout_with_colors(window,gc,
			x, y, btn->layout,&style->fg[GTK_STATE_NORMAL],0);
	}

	g_object_unref(gc);
	
	return FALSE;
}

static gboolean gtk_ybutton_button_press(GtkWidget *widget,GdkEventButton *event)
{
	if(event->type == GDK_BUTTON_PRESS && event->button == 1)
		gtk_ybutton_pressed(GTK_YBUTTON(widget));
	return TRUE;
}

static gboolean gtk_ybutton_button_release(GtkWidget *widget,GdkEventButton *event)
{
	if(event->button == 1)
		gtk_ybutton_released(GTK_YBUTTON(widget));
	return TRUE;
}

static gboolean gtk_ybutton_grab_broken(GtkWidget *widget,GdkEventGrabBroken *event)
{
	GtkYButton *button = GTK_YBUTTON (widget);
	gboolean save_in;
  
	if(button->button_down)
	{
		save_in = button->in_button;
		button->in_button = FALSE;
		gtk_ybutton_released (button);
		if (save_in != button->in_button)
		{
			button->in_button = save_in;
			gtk_ybutton_update_state (button);
		}
	}
	return TRUE;
}

static gboolean gtk_ybutton_key_release(GtkWidget *widget,GdkEventKey *event)
{
	GtkYButton *button = GTK_YBUTTON (widget);

	if(button->activate_timeout)
	{
		gtk_ybutton_finish_activate (button, TRUE);
		return TRUE;
	}
	else if(GTK_WIDGET_CLASS (gtk_ybutton_parent_class)->key_release_event)
		return GTK_WIDGET_CLASS (gtk_ybutton_parent_class)->key_release_event (widget, event);
	else return FALSE;
}

static gboolean gtk_ybutton_enter_notify(GtkWidget *widget,GdkEventCrossing *event)
{
	GtkYButton *button;
	GtkWidget *event_widget;

	button = GTK_YBUTTON (widget);
	event_widget = gtk_get_event_widget ((GdkEvent*) event);

	if ((event_widget == widget) &&  (event->detail != GDK_NOTIFY_INFERIOR))
	{
		button->in_button = TRUE;
		gtk_ybutton_enter (button);
	}
	return FALSE;
}

static gboolean gtk_ybutton_leave_notify (GtkWidget *widget,GdkEventCrossing *event)
{
	GtkYButton *button;
	GtkWidget *event_widget;

	button = GTK_YBUTTON (widget);
	event_widget = gtk_get_event_widget ((GdkEvent*) event);

	if((event_widget == widget) && (event->detail != GDK_NOTIFY_INFERIOR))
	{
		button->in_button = FALSE;
		gtk_ybutton_leave (button);
	}
	return FALSE;
}

static void gtk_real_ybutton_pressed (GtkYButton *button)
{
	if (button->activate_timeout)
		return;
	button->button_down = TRUE;
	gtk_ybutton_update_state (button);
}

static void gtk_real_ybutton_released(GtkYButton *button)
{
	if(!button->button_down) return;

	button->button_down = FALSE;
	if (button->activate_timeout) return;
	if (button->in_button)
		gtk_ybutton_clicked (button);
	gtk_ybutton_update_state (button);
}

static gboolean button_activate_timeout(gpointer data)
{
	gtk_ybutton_finish_activate (data, TRUE);
	return FALSE;
}

static void gtk_real_ybutton_activate(GtkYButton *button)
{
	GtkWidget *widget = GTK_WIDGET (button);
	guint32 time;

#ifdef GSEAL_ENABLE
	if(!gtk_widget_get_realized(widget)) return;
#else
	if(!GTK_WIDGET_REALIZED(button)) return;
#endif

	if (!button->activate_timeout)
	{
		time = gtk_get_current_event_time ();
		if (gdk_keyboard_grab (button->event_window, TRUE, time) == GDK_GRAB_SUCCESS)
		{
			GtkYButtonPrivate *priv=GTK_YBUTTON_GET_PRIVATE (button);;
			priv->has_grab = TRUE;
			priv->grab_time = time;
		}
		gtk_grab_add (widget);
		button->activate_timeout=g_timeout_add (ACTIVATE_TIMEOUT,
				button_activate_timeout,button);
		button->button_down = TRUE;
		gtk_ybutton_update_state (button);
		gtk_widget_queue_draw (widget);
	}
}

static void gtk_ybutton_finish_activate (GtkYButton *button,gboolean do_it)
{
	GtkWidget *widget = GTK_WIDGET(button);
	GtkYButtonPrivate *priv;
  
	priv = GTK_YBUTTON_GET_PRIVATE(button);

	g_source_remove (button->activate_timeout);
	button->activate_timeout = 0;

	if (priv->has_grab)
		gdk_display_keyboard_ungrab (gtk_widget_get_display (widget),priv->grab_time);
	gtk_grab_remove (widget);

	button->button_down = FALSE;

	gtk_ybutton_update_state(button);
	gtk_widget_queue_draw(GTK_WIDGET (button));

	if(do_it) gtk_ybutton_clicked(button);
}

static void gtk_ybutton_update_state(GtkYButton *button)
{
	gboolean depressed;
	GtkStateType new_state;

	if(button->activate_timeout)
		depressed = button->depress_on_activate;
	else
		depressed = button->in_button && button->button_down;

	if(button->in_button && (!button->button_down || !depressed))
		new_state = GTK_STATE_PRELIGHT;
	else
		new_state = depressed ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL;
	gtk_widget_set_state (GTK_WIDGET (button), new_state);
}

static void gtk_ybutton_state_changed (GtkWidget *widget,GtkStateType prev)
{
	GtkYButton *button = GTK_YBUTTON (widget);

#ifdef GSEAL_ENABLE
	if(!gtk_widget_is_sensitive(widget))
#else
	if(!GTK_WIDGET_IS_SENSITIVE (widget))
#endif
	{
		button->in_button = FALSE;
		gtk_real_ybutton_released (button);
	}
}

static void gtk_ybutton_grab_notify(GtkWidget *widget,gboolean grabbed)
{
	GtkYButton *button=GTK_YBUTTON(widget);

	if(!grabbed)
	{
		gboolean save_in = button->in_button;
		button->in_button = FALSE; 
		gtk_real_ybutton_released(button);
		if (save_in != button->in_button)
		{
			button->in_button = save_in;
			gtk_ybutton_update_state (button);
		}
	}
}

void gtk_ybutton_text(GtkYButton *button,char *text)
{
	if(!text)
	{
		if(button->layout)
		{
			g_object_unref(button->layout);
			button->layout=0;
		}
		return;
	}
	if(!button->layout)
	{
		button->layout = gtk_widget_create_pango_layout(GTK_WIDGET(button),text);
		pango_layout_set_alignment(button->layout,PANGO_ALIGN_CENTER);
	}

	pango_layout_set_text(button->layout,text,-1);
}

void gtk_ybutton_bg(GtkYButton *button,int state,GdkPixbuf *pixbuf)
{
	if(button->bg[state])
		g_object_unref(button->bg[state]);
	button->bg[state]=pixbuf;
	if(pixbuf && state==YBUTTON_NORMAL)
	{
		int width = gdk_pixbuf_get_width(pixbuf);
		int height = gdk_pixbuf_get_height(pixbuf);
		gtk_widget_set_size_request(GTK_WIDGET(button),width,height);
	}
	if(pixbuf) g_object_ref(pixbuf);
}
