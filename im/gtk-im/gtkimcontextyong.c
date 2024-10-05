#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <dlfcn.h>
#include "gtkimcontextyong.h"
#include "lcall.h"
#include "yong.h"
#include "ltime.h"
#include "ltricky.h"

#define YONG_IGNORED_MASK		(1<<25)

enum{
	APP_NORMAL=0,
	APP_MOZILLA,
	APP_GEANY,
	APP_GEDIT,
	APP_SUBLIME,
	APP_CHROME,
	APP_GVIM,
	APP_DOUBLECMD,
	APP_ELECTRON,
	APP_YONG,
};

struct _GtkIMContextYong{
	GtkIMContext object;
	
	guint id;
#if GTK_CHECK_VERSION(3,92,0)
	GtkWidget *client_window;
#else
	GdkWindow *client_window;
#endif

	guint has_focus:1;
	guint use_preedit:1;
	guint app_type:4;
	guint is_wayland:1;
	
	gboolean skip_cursor;
	GdkRectangle cursor_area;	
	gchar *preedit_string;
};

static void gtk_im_context_yong_class_init         (GtkIMContextYongClass  *class);
static void gtk_im_context_yong_class_fini         (GtkIMContextYongClass  *class);
static void gtk_im_context_yong_init               (GtkIMContextYong       *ctx);
static void gtk_im_context_yong_finalize           (GObject               *obj);
#if GTK_CHECK_VERSION(3,92,0)
static void gtk_im_context_yong_set_client_window  (GtkIMContext          *context,
						       GtkWidget             *client_window);
#else
static void gtk_im_context_yong_set_client_window  (GtkIMContext          *context,
						       GdkWindow             *client_window);
#endif
static gboolean gtk_im_context_yong_filter_keypress    (GtkIMContext          *context,
						       GdkEventKey           *key);
static void gtk_im_context_yong_reset              (GtkIMContext          *context);
static void gtk_im_context_yong_focus_in           (GtkIMContext          *context);
static void gtk_im_context_yong_focus_out          (GtkIMContext          *context);
static void gtk_im_context_yong_set_cursor_location (GtkIMContext          *context,
                                                       GdkRectangle             *area);
static void gtk_im_context_yong_set_use_preedit    (GtkIMContext          *context,
                                                       gboolean               use_preedit);
static void gtk_im_context_yong_get_preedit_string (GtkIMContext          *context,
                                                       gchar                **str,
                                                       PangoAttrList        **attrs,
                                                       gint                  *cursor_pos);

static int client_dispatch(const char *name,LCallBuf *buf);
static void client_connect(void);
static void client_set_cursor_location(guint id,const GdkRectangle *area);
static void client_set_cursor_location_relative(guint id,const GdkRectangle *area);
static gboolean client_input_key(guint id,int key,guint32 time);
static gboolean client_input_key_async(guint id,int key,guint32 time);
static void client_focus_in(guint id);
static void client_focus_out(guint id);
static void client_enable(guint id);
static void client_add_ic(guint id);
static void client_del_ic(guint id);

static int GetKey(guint KeyCode,guint KeyState);
static void ForwardKey(GtkIMContextYong *ctx,int key);

static gint key_snooper_cb(GtkWidget *widget,GdkEventKey *event,gpointer user_data);

static GObjectClass *parent_class;
GType gtk_type_im_context_yong = 0;

static guint _signal_commit_id = 0;
static guint _signal_preedit_changed_id = 0;
static guint _signal_preedit_start_id = 0;
static guint _signal_preedit_end_id = 0;

static guint _app_type;
static gboolean _enable;
static GSList *_ctx_list;
static guint _ctx_id;
static GtkIMContextYong *_focus_ctx;
static int _trigger;

#if GTK_CHECK_VERSION(3,0,0)
static gboolean _electron_retrun;
static char _electron_commit_text[2];
#endif

static guint _key_snooper_id;

#if GTK_CHECK_VERSION(3,0,0)
static gint (*p_gdk_window_get_scale_factor)(GdkWindow *window);
#endif

void gtk_im_context_yong_register_type (GTypeModule *type_module)
{
  const GTypeInfo im_context_yong_info =
  {
    sizeof (GtkIMContextYongClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_im_context_yong_class_init,
    (GClassFinalizeFunc) gtk_im_context_yong_class_fini,
    NULL,
    sizeof (GtkIMContextYong),
    0,
    (GInstanceInitFunc) gtk_im_context_yong_init,
  };

  gtk_type_im_context_yong =
    g_type_module_register_type (type_module,
				 GTK_TYPE_IM_CONTEXT,
				 "GtkIMContextYong",
				 &im_context_yong_info, 0);
}

static void gtk_im_context_yong_class_fini(GtkIMContextYongClass *class)
{
	if(_key_snooper_id != 0)
	{
		gtk_key_snooper_remove (_key_snooper_id);
		_key_snooper_id = 0;
	}
}

GtkIMContext *gtk_im_context_yong_new (void)
{
	GObject *obj = g_object_new (GTK_TYPE_IM_CONTEXT_YONG, NULL);
	return GTK_IM_CONTEXT (obj);
}

static int check_app_type(void)
{
	int pid = getpid();
	static const char *moz[]={"firefox", "thunderbird","seamonkey"};
#if GTK_CHECK_VERSION(3,0,0)
	static const char *electron[]={"electron","obsidian"};
#endif
	char tstr0[64];
	char exec[256];
	int i;
	int type=APP_NORMAL;
	sprintf(tstr0, "/proc/%d/exe", pid);
	if ((i=readlink(tstr0, exec, sizeof(exec))) > 0)
	{
		exec[i]=0;
		char *prog=strrchr(exec,'/');
		if(prog!=NULL)
		{
			prog++;
		}
		for(i=0; i < sizeof(moz)/sizeof(moz[0]); i++)
		{
			if(!strstr(exec, moz[i]))
				continue;
			type=APP_MOZILLA;
			goto out;
		}
#if GTK_CHECK_VERSION(3,0,0)
		for(i=0; i < sizeof(electron)/sizeof(electron[0]); i++)
		{
			if(prog && strcmp(prog, electron[i]))
				continue;
			type=APP_ELECTRON;
			goto out;
		}
#endif
		if(strstr(exec,"geany"))
		{
			type=APP_GEANY;
			goto out;
		}
		else if(strstr(exec,"gedit"))
		{
			type=APP_GEDIT;
			goto out;
		}
		else if(strstr(exec,"sublime"))
		{
			type=APP_SUBLIME;
			goto out;
		}
		else if(strstr(exec,"chromium-browser") || strstr(exec,"chrome")
			||strstr(exec,"opera") || strstr(exec,"vivaldi"))
		{
			type=APP_CHROME;
			goto out;
		}
		else if(strstr(exec,"gvim"))
		{
			type=APP_GVIM;
			goto out;
		}
		else if(strstr(exec,"doublecmd"))
		{
			type=APP_DOUBLECMD;
			goto out;
		}
		else if(prog && !strcmp(prog,"yong-gtk3"))
		{
			type=APP_YONG;
			goto out;
		}
	}
out:
	return type;
}

static void gtk_im_context_yong_class_init (GtkIMContextYongClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  im_context_class->set_client_window = gtk_im_context_yong_set_client_window;
  im_context_class->filter_keypress = gtk_im_context_yong_filter_keypress;
  im_context_class->reset = gtk_im_context_yong_reset;
  im_context_class->get_preedit_string = gtk_im_context_yong_get_preedit_string;
  im_context_class->focus_in = gtk_im_context_yong_focus_in;
  im_context_class->focus_out = gtk_im_context_yong_focus_out;
  im_context_class->set_cursor_location = gtk_im_context_yong_set_cursor_location;
  im_context_class->set_use_preedit = gtk_im_context_yong_set_use_preedit;
  gobject_class->finalize = gtk_im_context_yong_finalize;
  
  _signal_commit_id =g_signal_lookup ("commit", G_TYPE_FROM_CLASS (class));
  _signal_preedit_changed_id =g_signal_lookup ("preedit-changed", G_TYPE_FROM_CLASS (class));
  _signal_preedit_start_id =g_signal_lookup ("preedit-start", G_TYPE_FROM_CLASS (class));
  _signal_preedit_end_id =g_signal_lookup ("preedit-end", G_TYPE_FROM_CLASS (class));
  
  _enable=0;
  _app_type=check_app_type();
  _trigger=CTRL_SPACE;
  _ctx_id=1;
  l_call_client_dispatch(client_dispatch);
  l_call_client_set_connect(client_connect);
 
  if (_key_snooper_id == 0 && (_app_type==APP_GEANY || _app_type==APP_GEDIT || _app_type==APP_SUBLIME || _app_type==APP_GVIM))
    _key_snooper_id=gtk_key_snooper_install(key_snooper_cb,NULL);

#if GTK_CHECK_VERSION(3,0,0)
  p_gdk_window_get_scale_factor=dlsym(NULL,"gdk_window_get_scale_factor");
#endif
}

#if GTK_CHECK_VERSION(3,0,0)
static int is_wayland(void)
{
	if(!getenv("WAYLAND_DISPLAY"))
		return 0;
	if(_app_type==APP_GVIM)
		return 0;
	char *s=getenv("GDK_BACKEND");
	if(!s)
		return 1;
	if(!strcmp(s,"x11"))
		return 0;
	return 1;
}
#else
static int is_wayland(void)
{
	return 0;
}
#endif

static void gtk_im_context_yong_init(GtkIMContextYong *ctx)
{	
	ctx->has_focus=0;
	ctx->use_preedit=1;
	ctx->is_wayland=is_wayland();
	ctx->id=_ctx_id++;
	ctx->cursor_area=(GdkRectangle){-1,-1,0,0};
	ctx->skip_cursor=FALSE;
	ctx->app_type=_app_type;
	
	_ctx_list=g_slist_prepend(_ctx_list,ctx);
	client_add_ic(ctx->id);
}

static void gtk_im_context_yong_finalize(GObject *obj)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(obj);

	_ctx_list=g_slist_remove(_ctx_list,ctx);
	
	if(ctx->preedit_string)
		g_free(ctx->preedit_string);
	if(ctx->client_window)
		g_object_unref(ctx->client_window);
		
	client_del_ic(ctx->id);
	
	G_OBJECT_CLASS(parent_class)->finalize (obj);
}

static gboolean _set_cursor_location_internal(GtkIMContextYong *ctx)
{
    GdkRectangle area;

    if(ctx->client_window == NULL)
        return FALSE;

    area = ctx->cursor_area;
    if (area.x == -1 && area.y == -1 && area.width == 0 && area.height == 0) {
#if GTK_CHECK_VERSION(3,92,0)
		area.x=0;
		area.y+=gdk_window_get_height (gtk_widget_get_window(ctx->client_window));
#elif GTK_CHECK_VERSION (2, 91, 0)
        area.x = 0;
        area.y += gdk_window_get_height (ctx->client_window);
#else
        gint w, h;
        gdk_drawable_get_size (ctx->client_window, &w, &h);
        area.y += h;
        area.x = 0;
#endif
    }

#if GTK_CHECK_VERSION(3,92,0)
	gdk_window_get_root_coords (gtk_widget_get_window(ctx->client_window),
                                area.x, area.y,
                                &area.x, &area.y);
#else
    gdk_window_get_root_coords (ctx->client_window,
                                area.x, area.y,
                                &area.x, &area.y);
#endif
#if GTK_CHECK_VERSION(3,92,0)
	if(ctx->client_window && p_gdk_window_get_scale_factor && ctx->app_type!=APP_CHROME)
	{
		gint scale=gtk_widget_get_scale_factor(ctx->client_window);
		if(scale!=1)
		{
			area.x*=scale;
			area.y*=scale;
			area.width*=scale;
			area.height*=scale;
		}
	}
#elif GTK_CHECK_VERSION(3,0,0)
	if(ctx->client_window && p_gdk_window_get_scale_factor && ctx->app_type!=APP_CHROME)
	{
		gint scale=p_gdk_window_get_scale_factor(ctx->client_window);
		if(scale!=1)
		{
			area.x*=scale;
			area.y*=scale;
			area.width*=scale;
			area.height*=scale;
		}
	}
#endif
	if(ctx->is_wayland)
		client_set_cursor_location_relative(ctx->id,&area);
	else
		client_set_cursor_location(ctx->id,&area);
	return FALSE;
}

#if GTK_CHECK_VERSION(3,92,0)
static void gtk_im_context_yong_set_client_window  (GtkIMContext          *context,
						       GtkWidget             *client_window)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(context);
	if(ctx->client_window)
	{
		g_object_unref(ctx->client_window);
		ctx->client_window=NULL;
	}
	if(client_window)
	{
		ctx->client_window=g_object_ref(client_window);
	}
}
#else
static void gtk_im_context_yong_set_client_window(GtkIMContext *context,GdkWindow *client_window)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(context);
	if(ctx->client_window)
	{
		g_object_unref(ctx->client_window);
		ctx->client_window=NULL;
	}
	if(client_window)
	{
		ctx->client_window=g_object_ref(client_window);
	}
}
#endif

static gint key_snooper_cb (GtkWidget *widget,GdkEventKey *event,gpointer user_data)
{
	GtkIMContextYong *ctx=_focus_ctx;
	if(!ctx)
		return FALSE;
	GdkModifierType state;
	state=event->state;
	if((state&YONG_IGNORED_MASK)!=0)
		return FALSE;
	if(ctx->app_type==APP_GEANY)
	{
		int release=(event->type == GDK_KEY_RELEASE);
		int key;
		int res=FALSE;
		if((state&GDK_CONTROL_MASK)==0 &&
					event->keyval!=GDK_KEY_Control_L &&
					event->keyval!=GDK_KEY_Control_R &&
					event->keyval!=GDK_KEY_Shift_L &&
					event->keyval!=GDK_KEY_Shift_R)
			return FALSE;
		key=GetKey(event->keyval,event->state);
		if(!key)
			return FALSE;
		if(release) key|=KEYM_UP;
		
		if(!_enable)
		{
			if((key&~KEYM_CAPS)==_trigger && !release)
			{
				client_enable(ctx->id);
				_enable=TRUE;
				res=TRUE;
			}
		}
		else
		{
			if(key==_trigger) l_call_client_connect();
			res=client_input_key(ctx->id,key,event->time);
		}
		
		return res;
	}
	if(ctx->app_type==APP_GEDIT)
	{
		int release=(event->type == GDK_KEY_RELEASE);
		int key;
		int res=FALSE;
		
		key=GetKey(event->keyval,event->state);
		if(!key || key!=YK_TAB)
			return FALSE;
		if(release) key|=KEYM_UP;
		if(_enable)
			res=client_input_key(ctx->id,key,event->time);
		return res;
		
	}
	if(ctx->app_type==APP_SUBLIME)
	{
		int release=(event->type == GDK_KEY_RELEASE);
		int key;
		int res=FALSE;
		
		key=GetKey(event->keyval,event->state);
		if(!key || (key!=YK_BACKSPACE && key!=YK_ENTER))
			return FALSE;
		if(release) key|=KEYM_UP;
		if(_enable)
			res=client_input_key(ctx->id,key,event->time);
		return res;	
	}
	if(ctx->app_type==APP_GVIM)
	{
		// gvim term mode not support input method
		int release=(event->type == GDK_KEY_RELEASE);
		int key;
		int res=FALSE;		
		key=GetKey(event->keyval,event->state);
		if(release) key|=KEYM_UP;
		if(!_enable)
		{
			if((key&~KEYM_CAPS)==_trigger && !release)
			{
				client_enable(ctx->id);
				_enable=TRUE;
				res=TRUE;
			}
		}
		else
		{
			res=client_input_key(ctx->id,key,event->time);
		}
		return res;
	}
	return FALSE;
}

static gboolean gtk_im_context_yong_filter_keypress(GtkIMContext *context,GdkEventKey *event)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(context);
	int release=(event->type == GDK_KEY_RELEASE);
	int key=0;
	gboolean res=FALSE;
	GdkModifierType state;
	guint keyval;
	guint32 event_time;
	state=event->state;
	keyval=event->keyval;
	event_time=event->time;
	key=GetKey(keyval,state);
	if(_app_type==APP_DOUBLECMD && !ctx->has_focus)
	{
		goto END;
	}
	if(!ctx->has_focus && !release && !(state&YONG_IGNORED_MASK))
	{
		gtk_im_context_yong_focus_in(context);
	}
	if(_key_snooper_id!=0)
	{
		goto END;
	}
	if(ctx->has_focus && !(state&YONG_IGNORED_MASK))
	{
		if(!key)
			return FALSE;
		if(release) key|=KEYM_UP;
		
		if(ctx->app_type==APP_GEANY)
		{
			if((key & KEYM_CTRL) || YK_CODE(key)==YK_LCTRL ||
					YK_CODE(key)==YK_RCTRL)
			{
				if(release) ctx->app_type=APP_NORMAL;
				return FALSE;
			}
			if(YK_CODE(key)==YK_LSHIFT || YK_CODE(key)==YK_RSHIFT)
			{
				if(release) ctx->app_type=APP_NORMAL;
				return FALSE;
			}
		}
		
		// if(key==_trigger && !release)
			// printf("trigger %p %d\n",context,_enable);

		if(!_enable)
		{
			if((key&~KEYM_CAPS)==_trigger && !release)
			{
				client_enable(ctx->id);
				_enable=TRUE;
				res=TRUE;
			}
		}
		else
		{
			if(key==_trigger) l_call_client_connect();
			// if(_app_type==APP_MOZILLA && (key=='\r' || key==(YK_ENTER|KEYM_UP)))
			// {
				// res=client_input_key_async(ctx->id,key,event_time);
			// }
#if GTK_CHECK_VERSION(3,0,0)
			if(ctx->app_type==APP_ELECTRON && (key=='\r' || key==(YK_ENTER|KEYM_UP)))
			{
				if(key=='\r')
				{
					_electron_retrun=TRUE;
					res=client_input_key(ctx->id,key,event_time);
					_electron_retrun=FALSE;
				}
				else
				{
					if(_electron_commit_text[0])
					{
						g_signal_emit(ctx,_signal_commit_id,0,_electron_commit_text);
						_electron_commit_text[0]=0;
					}
				}
				return res;
			}
			if(ctx->app_type==APP_YONG)
			{
				res=client_input_key_async(ctx->id,key,event_time);
			}
	#endif
			else
			{
				res=client_input_key(ctx->id,key,event_time);
				if(key==_trigger && !release && res==0)
				{
					client_enable(ctx->id);
					_enable=TRUE;
					res=TRUE;
				}
			}
		}
	}
END:
	if(res==FALSE && !release && !(KEYM_MASK&key&~(KEYM_SHIFT|KEYM_KEYPAD|KEYM_CAPS)))
	{
		gunichar ch;
		ch=gdk_keyval_to_unicode(keyval);
		if(ch!=0)
		{
			gchar buf[10];
			int len;
			len = g_unichar_to_utf8(ch,buf);
			if(len>0 && buf[0]>=0x20 && buf[0]<0x7f)
			{
				buf[len] = '\0';
				g_signal_emit(ctx,_signal_commit_id,0,buf);
				res=TRUE;
			}
		}
	}
	if(res==FALSE)
	{
		GtkIMContextClass *parent;
		parent=(GtkIMContextClass*)parent_class;
		if (parent->filter_keypress)
			return (*parent->filter_keypress) (context, event);
	}
	return res;
}

static void gtk_im_context_yong_reset(GtkIMContext *context)
{
}

static gboolean _focus_in_internal(GtkIMContextYong *ctx)
{
	if(!ctx->has_focus)
		return FALSE;
	_set_cursor_location_internal(ctx);
	client_focus_in(ctx->id);
	return FALSE;
}

static gboolean _focus_out_internal(GtkIMContextYong *ctx)
{
	if(ctx->has_focus)
		return FALSE;
	client_focus_out(ctx->id);
	return FALSE;
}

static void gtk_im_context_yong_focus_in(GtkIMContext *context)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(context);
	ctx->has_focus=1;
	_focus_ctx=ctx;
	
	//printf("focus in %p\n",context);
	
	if(ctx->app_type==APP_MOZILLA || ctx->app_type==APP_ELECTRON)
	{
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                     (GSourceFunc) _focus_in_internal,
                     g_object_ref (ctx),
                     (GDestroyNotify) g_object_unref);
	}
	else
	{
		_focus_in_internal(ctx);
	}

	if(_app_type==APP_MOZILLA && !ctx->preedit_string)
	{
		g_signal_emit(ctx,_signal_preedit_start_id,0);
		g_signal_emit(ctx,_signal_preedit_end_id,0);
	}
}

static void gtk_im_context_yong_focus_out(GtkIMContext *context)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(context);
	ctx->has_focus=0;
	if(_focus_ctx==ctx)
		_focus_ctx=NULL;
		
	//printf("focus out %p\n",context);
	
	if(ctx->app_type==APP_MOZILLA || ctx->app_type==APP_ELECTRON)
	{
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
					(GSourceFunc) _focus_out_internal,
					g_object_ref (ctx),
					(GDestroyNotify) g_object_unref);
	}
	else
	{
		_focus_out_internal(ctx);
	}
}

static void gtk_im_context_yong_set_cursor_location(GtkIMContext *context,GdkRectangle *area)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(context);
	//printf("set cursor location %d %d\n",area->x,area->y);
	if(area->x==ctx->cursor_area.x && area->y==ctx->cursor_area.y && 
			area->width==ctx->cursor_area.width &&
			area->height==ctx->cursor_area.height)
	{
		return;
	}
	if(ctx->skip_cursor)
		return;
	ctx->cursor_area=*area;
	_set_cursor_location_internal(ctx);
}

static void gtk_im_context_yong_set_use_preedit(GtkIMContext *context,gboolean use_preedit)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(context);
	ctx->use_preedit=use_preedit?1:0;
}

static void gtk_im_context_yong_get_preedit_string (GtkIMContext *context,gchar **str,PangoAttrList **attrs,gint *cursor_pos)
{
	GtkIMContextYong *ctx=GTK_IM_CONTEXT_YONG(context);
	PangoAttribute *attr;
	int pos;
	if(!str)
		return;
	if(ctx->preedit_string)
	{
		char *p;
		*str=g_strdup(ctx->preedit_string);
		if((p=strchr(*str,'|'))!=NULL)
		{
			pos=g_utf8_pointer_to_offset(*str,p);
			memcpy(p,p+1,strlen(p+1)+1);
		}
		else
		{
			pos=g_utf8_strlen(*str,-1);
		}
	}
	else
	{
		*str=g_strdup("");
		pos=0;
	}
	if(cursor_pos)
		*cursor_pos=pos;
	if(attrs)
	{
		attr=pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
		attr->start_index=0;
		attr->end_index=strlen(*str);
		*attrs = pango_attr_list_new ();
		pango_attr_list_change (*attrs, attr);
	}
}

void gtk_im_context_yong_shutdown(void)
{
	l_call_client_disconnect();
}

static void client_set_cursor_location(guint id,const GdkRectangle *area)
{
	l_call_client_call("cursor",NULL,"iiiii",id,area->x,area->y,area->width,area->height);
}

static void client_set_cursor_location_relative(guint id,const GdkRectangle *area)
{
	l_call_client_call("cursor",NULL,"iiiiii",id,area->x,area->y,area->width,area->height,1);
}

static gboolean client_input_key(guint id,int key,guint32 time)
{
	int ret,res;
	ret=l_call_client_call("input",&res,"iii",id,key,time);
	if(ret!=0)
		return FALSE;
	return res?TRUE:FALSE;
}

static gboolean client_input_key_async(guint id,int key,guint32 time)
{
	int ret;
	ret=l_call_client_call("input",NULL,"iii",id,key,time);
	if(ret!=0) return 0;
	return TRUE;
}

static void client_focus_in(guint id)
{
	l_call_client_call("focus_in",NULL,"i",id);
}

static void client_focus_out(guint id)
{
	l_call_client_call("focus_out",NULL,"i",id);
}

static void client_enable(guint id)
{
	l_call_client_call("enable",NULL,"i",id);
}

static void client_add_ic(guint id)
{
	l_call_client_call("add_ic",NULL,"i",id);
}

static void client_del_ic(guint id)
{
	l_call_client_call("del_ic",NULL,"i",id);
}

static GtkIMContextYong *find_context(guint id)
{
	GSList *p;
	for(p=_ctx_list;p!=NULL;p=p->next)
	{
		GtkIMContextYong *ctx=p->data;
		if(ctx->id==id)
			return ctx;
	}
	return NULL;
}

static void client_connect(void)
{
	GtkIMContextYong *ctx;
	GSList *p;
	if(!_ctx_list)
		return;
	for(p=_ctx_list;p!=NULL;p=p->next)
	{
		ctx=p->data;
		if(ctx->has_focus)
		{
			if(_enable)
				client_enable(ctx->id);
			_focus_in_internal(ctx);
			return;
		}
	}
	if(_enable)
	{
		ctx=_ctx_list->data;
		client_enable(ctx->id);
	}
}

static int client_dispatch(const char *name,LCallBuf *buf)
{
	if(!strcmp(name,"commit"))
	{
		guint id;
		int ret;
		char text[1024];
		GtkIMContextYong *ctx;
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		ret=l_call_buf_get_string(buf,text,sizeof(text));
		if(ret!=0) return -1;
		if(ctx->preedit_string)
		{
			g_free(ctx->preedit_string);
			ctx->preedit_string=NULL;
			ctx->skip_cursor=TRUE;
			g_signal_emit(ctx,_signal_preedit_changed_id,0);
			g_signal_emit(ctx,_signal_preedit_end_id,0);
			ctx->skip_cursor=FALSE;
			//printf("preedit end\n");
		}
		else
		{
#if GTK_CHECK_VERSION(3,0,0)
			if(ctx->app_type==APP_ELECTRON && _electron_retrun==TRUE && strlen(text)==1)
			{
				ctx->preedit_string=g_strdup(text);
				ctx->skip_cursor=TRUE;
				g_signal_emit(ctx,_signal_preedit_start_id,0);
				ctx->skip_cursor=FALSE;
				g_signal_emit(ctx,_signal_preedit_changed_id,0);
			}
#endif
		}
		g_signal_emit(ctx,_signal_commit_id,0,text);
	}
	else if(!strcmp(name,"preedit"))
	{
		guint id;
		int ret;
		GtkIMContextYong *ctx;
		char text[1024];
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		ret=l_call_buf_get_string(buf,text,sizeof(text));
		if(ret!=0) return -1;
		if(!ctx->use_preedit) return 0;
		if(ctx->preedit_string)
		{
			if(!text[0])
			{
				g_free(ctx->preedit_string);
				ctx->preedit_string=NULL;
				ctx->skip_cursor=TRUE;
				g_signal_emit(ctx,_signal_preedit_changed_id,0);
				g_signal_emit(ctx,_signal_preedit_end_id,0);
				ctx->skip_cursor=FALSE;
				//printf("preedit end\n");
			}
			else
			{
				g_free(ctx->preedit_string);
				ctx->preedit_string=g_strdup(text);
				g_signal_emit(ctx,_signal_preedit_changed_id,0);
				//printf("preedit change\n");
			}
		}
		else
		{
			ctx->preedit_string=g_strdup(text);
			ctx->skip_cursor=TRUE;
			g_signal_emit(ctx,_signal_preedit_start_id,0);
			ctx->skip_cursor=FALSE;
			g_signal_emit(ctx,_signal_preedit_changed_id,0);
			//printf("preedit start\n");
		}
	}
	else if(!strcmp(name,"preedit_clear"))
	{
		guint id;
		int ret;
		GtkIMContextYong *ctx;
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		if(!ctx->use_preedit) return 0;
		if(ctx->preedit_string)
		{
			g_free(ctx->preedit_string);
			ctx->preedit_string=NULL;
			
			g_signal_emit(ctx,_signal_preedit_changed_id,0);
			g_signal_emit(ctx,_signal_preedit_end_id,0);
			printf("preedit end\n");
		}
	}
	else if(!strcmp(name,"forward"))
	{
		guint id;
		int ret;
		GtkIMContextYong *ctx;
		int key;
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		ret=l_call_buf_get_val(buf,key);
		if(ret!=0) return -1;
		ForwardKey(ctx,key);
	}
	else if(!strcmp(name,"enable"))
	{
		_enable=TRUE;
	}
	else if(!strcmp(name,"disable"))
	{
		guint id;
		int ret;
		GtkIMContextYong *ctx;
		
		_enable=FALSE;

		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		if(ctx->preedit_string)
		{
			g_free(ctx->preedit_string);
			ctx->preedit_string=NULL;
			g_signal_emit(ctx,_signal_preedit_changed_id,0);
			g_signal_emit(ctx,_signal_preedit_end_id,0);
		}
	}
	else if(!strcmp(name,"trigger"))
	{
		int ret;
		ret=l_call_buf_get_val(buf,_trigger);
		if(ret!=0) return -1;
	}
	return 0;
}

static int GetKey(guint KeyCode,guint KeyState)
{
	int ret;
	int mask;

	switch(KeyCode){
	case GDK_KEY_ISO_Left_Tab:
		KeyCode=YK_TAB;
		break;
	}
	if (KeyState & GDK_MOD2_MASK)
	{
		if (KeyCode >= 0xffaa && KeyCode <= 0xffb9)
			KeyCode = "*+ -./0123456789"[KeyCode-0xffaa];
		else /* remove the mask for other keys */
			KeyState &=~ GDK_MOD2_MASK;
		/* bug: gtk will only send one release key at 0xffaa-0xffaf */
		/* other like xterm is just right, so don't deal it now */
	}
	if((KeyCode&0xff)<0x20 || (KeyCode&0xff)>=0x80)
		KeyCode=KeyCode&0xff;
	ret=KeyCode;

	if ((KeyState & GDK_CONTROL_MASK) && KeyCode!=YK_LCTRL && KeyCode!=YK_RCTRL)
		ret|=KEYM_CTRL;
	if ((KeyState & GDK_SHIFT_MASK) && KeyCode!=YK_LSHIFT && KeyCode!=YK_RSHIFT)
		ret|=KEYM_SHIFT;
	if ((KeyState & GDK_MOD1_MASK) && KeyCode!=YK_LALT && KeyCode!=YK_RALT)
		ret|=KEYM_ALT;
	if ((KeyState & GDK_MOD4_MASK) && KeyCode!=YK_LWIN && KeyCode!=YK_RWIN)
		ret|=KEYM_SUPER;
	if(KeyState & GDK_MOD2_MASK)
		ret|=KEYM_KEYPAD;
	if(KeyState & GDK_RELEASE_MASK)
		ret|=KEYM_UP;
	if(KeyState & GDK_LOCK_MASK)
		ret|=KEYM_CAPS;

	mask=ret&KEYM_MASK;
	if(mask && mask!=KEYM_SHIFT)
	{
		int code=YK_CODE(ret);
		if(code>='a' && code<='z')
			code=code-'a'+'A';
		ret=mask|code;
	}
	return ret;
}

static gboolean
_key_is_modifier (guint keyval)
{
  /* See gdkkeys-x11.c:_gdk_keymap_key_is_modifier() for how this
* really should be implemented */

    switch (keyval) {
#ifdef DEPRECATED_GDK_KEYSYMS
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Control_L:
    case GDK_Control_R:
    case GDK_Caps_Lock:
    case GDK_Shift_Lock:
    case GDK_Meta_L:
    case GDK_Meta_R:
    case GDK_Alt_L:
    case GDK_Alt_R:
    case GDK_Super_L:
    case GDK_Super_R:
    case GDK_Hyper_L:
    case GDK_Hyper_R:
    case GDK_ISO_Lock:
    case GDK_ISO_Level2_Latch:
    case GDK_ISO_Level3_Shift:
    case GDK_ISO_Level3_Latch:
    case GDK_ISO_Level3_Lock:
    case GDK_ISO_Level5_Shift:
    case GDK_ISO_Level5_Latch:
    case GDK_ISO_Level5_Lock:
    case GDK_ISO_Group_Shift:
    case GDK_ISO_Group_Latch:
    case GDK_ISO_Group_Lock:
        return TRUE;
#else
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
    case GDK_KEY_Caps_Lock:
    case GDK_KEY_Shift_Lock:
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:
    case GDK_KEY_Hyper_L:
    case GDK_KEY_Hyper_R:
    case GDK_KEY_ISO_Lock:
    case GDK_KEY_ISO_Level2_Latch:
    case GDK_KEY_ISO_Level3_Shift:
    case GDK_KEY_ISO_Level3_Latch:
    case GDK_KEY_ISO_Level3_Lock:
    case GDK_KEY_ISO_Level5_Shift:
    case GDK_KEY_ISO_Level5_Latch:
    case GDK_KEY_ISO_Level5_Lock:
    case GDK_KEY_ISO_Group_Shift:
    case GDK_KEY_ISO_Group_Latch:
    case GDK_KEY_ISO_Group_Lock:
        return TRUE;
#endif
    default:
        return FALSE;
    }
}

static guint16 lookup_keycode(GtkIMContextYong *ctx,guint keyval)
{
	GdkDisplay *dpy;
	GdkKeymap *km;
	GdkKeymapKey *keys;
	gint n_keys;
	guint16 res=0;
	if(keyval!=GDK_KEY_BackSpace)
		return 0;
	if(ctx->client_window)
		dpy=gdk_window_get_display(ctx->client_window);
	else
		dpy=gdk_display_get_default();
	if(!dpy) return 0;
	km=gdk_keymap_get_default();
	if(!km) return 0;
	if(!gdk_keymap_get_entries_for_keyval(km,keyval,&keys,&n_keys))
		return 0;
	if(n_keys>0)
		res=keys->keycode;
	g_free(keys);
	return res;
}

static GdkEventKey *
_create_gdk_event (GtkIMContextYong *ctx,
                   guint keyval,
                   guint state)
{
    gunichar c = 0;
    gchar buf[8];

    GdkEventKey *event = (GdkEventKey *)gdk_event_new ((state & GDK_RELEASE_MASK) ? GDK_KEY_RELEASE : GDK_KEY_PRESS);

    if (ctx && ctx->client_window)
        event->window = g_object_ref (ctx->client_window);
    
    event->time = GDK_CURRENT_TIME;

    event->send_event = FALSE;
    event->state = state|YONG_IGNORED_MASK;
    event->keyval = keyval;
    event->string = NULL;
    event->length = 0;
    event->hardware_keycode = lookup_keycode(ctx,keyval);
    event->group = 0;
    event->is_modifier = _key_is_modifier (keyval);

#ifdef DEPRECATED_GDK_KEYSYMS
    if (keyval != GDK_VoidSymbol)
#else
    if (keyval != GDK_KEY_VoidSymbol)
#endif
 	c = gdk_keyval_to_unicode (keyval);
    if (c) {
        gsize bytes_written;
        gint len;

        /* Apply the control key - Taken from Xlib */
        if (event->state & GDK_CONTROL_MASK) {
            if ((c >= '@' && c < '\177') || c == ' ') c &= 0x1F;
            else if (c == '2') {
                event->string = g_memdup ("\0\0", 2);
                event->length = 1;
                buf[0] = '\0';
                goto out;
            }
            else if (c >= '3' && c <= '7') c -= ('3' - '\033');
            else if (c == '8') c = '\177';
            else if (c == '/') c = '_' & 0x1F;
        }

        len = g_unichar_to_utf8 (c, buf);
        buf[len] = '\0';

        event->string = g_locale_from_utf8 (buf, len,
                                            NULL, &bytes_written,
                                            NULL);
        if (event->string)
            event->length = bytes_written;
#ifdef DEPRECATED_GDK_KEYSYMS
    } else if (keyval == GDK_Escape) {
#else
    } else if (keyval == GDK_KEY_Escape) {
#endif
        event->length = 1;
        event->string = g_strdup ("\033");
    }
#ifdef DEPRECATED_GDK_KEYSYMS
    else if (keyval == GDK_Return ||
             keyval == GDK_KP_Enter) {
#else
    else if (keyval == GDK_KEY_Return ||
             keyval == GDK_KEY_KP_Enter) {
#endif
        event->length = 1;
        event->string = g_strdup ("\r");
    }

    if (!event->string) {
        event->length = 0;
        event->string = g_strdup ("");
    }
out:
    return event;
}

static void ForwardKey(GtkIMContextYong *ctx,int key)
{
	guint keyval,state=0;
	int code=YK_CODE(key);
	GdkEventKey *event;

	if(ctx->client_window)
	{
		GtkWidget *widget=NULL;
		gdk_window_get_user_data(ctx->client_window,(void**)&widget);
		if(widget && (GTK_IS_TEXT_VIEW(widget) || GTK_IS_ENTRY(widget)))
		{
			switch(key){
				case CTRL_V:
					g_signal_emit_by_name(widget,"paste-clipboard",NULL);
					return;
				case YK_BACKSPACE:
					g_signal_emit_by_name(widget,"backspace",NULL);
					return;
				case YK_DELETE:
					g_signal_emit_by_name(widget,"delete-from-cursor",GTK_DELETE_CHARS,1,NULL);
					return;
				case YK_LEFT:
					g_signal_emit_by_name(widget,"move-cursor",GTK_MOVEMENT_LOGICAL_POSITIONS,-1,0,NULL);
					return;
				case YK_RIGHT:
					g_signal_emit_by_name(widget,"move-cursor",GTK_MOVEMENT_LOGICAL_POSITIONS,1,0,NULL);
					return;
				case YK_HOME:
					if(GTK_IS_ENTRY(widget))
					{
						gtk_editable_set_position(GTK_EDITABLE(widget),0);
						return;
					}
					break;
				case YK_END:
					g_signal_emit_by_name(widget,"move-cursor",GTK_MOVEMENT_DISPLAY_LINE_ENDS,1,0,NULL);
					return;
				case YK_ENTER:
					if(GTK_IS_ENTRY(widget))
					{
						g_signal_emit_by_name(widget,"activate",NULL);
					}
					else
					{
						g_signal_emit_by_name(widget,"insert-at-cursor","\n",NULL);
					}
					return;
				default:
					break;
			}
		}
	}
	
	switch(code){
	case YK_BACKSPACE:keyval=GDK_KEY_BackSpace;break;
	case YK_ESC:keyval=GDK_KEY_Escape;break;
	case YK_DELETE:keyval=GDK_KEY_Delete;break;
	case YK_ENTER:keyval=GDK_KEY_Return;break;
	case YK_HOME:keyval=GDK_KEY_Home;break;
	case YK_END:keyval=GDK_KEY_End;break;
	case YK_PGUP:keyval=GDK_KEY_Page_Up;break;
	case YK_PGDN:keyval=GDK_KEY_Page_Down;break;
	case YK_LEFT:keyval=GDK_KEY_Left;break;
	case YK_DOWN:keyval=GDK_KEY_Down;break;
	case YK_UP:keyval=GDK_KEY_Up;break;
	case YK_RIGHT:keyval=GDK_KEY_Right;break;
	case YK_TAB:keyval=GDK_KEY_Tab;break;
	default:
		if(code<0x80) 
		{
			keyval=code;
			break;
		}
		return;
	}
	
	if(key & KEYM_CTRL)
		state|=GDK_CONTROL_MASK;
	if(key & KEYM_SHIFT)
		state|=GDK_SHIFT_MASK;
	if(key & KEYM_UP)
		state|=GDK_RELEASE_MASK;
	event=_create_gdk_event(ctx,keyval,state);
	if(event!=NULL)
	{
		gdk_event_put ((GdkEvent *)event);
		gdk_event_free ((GdkEvent *)event);
	}
}
