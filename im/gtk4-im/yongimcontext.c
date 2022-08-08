#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#include "yongimcontext.h"
#include "lcall.h"
#include "yong.h"
#include "ltricky.h"

#define YONG_IGNORED_MASK		(1<<31)

enum{
	APP_NORMAL=0,
};

static GObjectClass *parent_class;
static GType type_yong_im_context=0;

struct _YongIMContext{
	GtkIMContext parent;
	
	guint id;
	GtkWidget *client_widget;
	
	guint has_focus:1;
	guint use_preedit:1;
	guint is_wayland:1;
	
	gboolean skip_cursor;
	GdkRectangle cursor_area;	
	gchar *preedit_string;
};

struct _YongIMContextClass {
    GtkIMContextClass parent;
};

static void yong_im_context_class_init(YongIMContext *class);
static void yong_im_context_class_fini(YongIMContextClass *class);
static void yong_im_context_init(YongIMContext *ctx);
static void yong_im_context_finalize(GObject *obj);

static void yong_im_context_set_client_widget(GtkIMContext *context,GtkWidget *client_widget);
static gboolean yong_im_context_filter_keypress(GtkIMContext *context,GdkEvent *key);
static void yong_im_context_reset(GtkIMContext *context);
static void yong_im_context_focus_in(GtkIMContext *context);
static void yong_im_context_focus_out(GtkIMContext *context);
static void yong_im_context_set_cursor_location(GtkIMContext *context,GdkRectangle *area);
static void yong_im_context_set_use_preedit(GtkIMContext *context,gboolean use_preedit);
static void yong_im_context_get_preedit_string(GtkIMContext *context,gchar **str,PangoAttrList **attrs,gint *cursor_pos);
static void yong_im_context_set_surrounding(GtkIMContext *context,const char *text, int len,int cursor_index);

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

static int GetKey(guint KeyCode,guint KeyState,int release);
static void ForwardKey(YongIMContext *ctx,int key);

static guint _signal_commit_id = 0;
static guint _signal_preedit_changed_id = 0;
static guint _signal_preedit_start_id = 0;
static guint _signal_preedit_end_id = 0;

static guint _app_type;
static gboolean _enable;
static GSList *_ctx_list;
static guint _ctx_id;
static int _trigger;

void yong_im_context_register_type(GTypeModule *type_module)
{
	const GTypeInfo yong_im_context_info={
		sizeof(YongIMContextClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc)yong_im_context_class_init,
		(GClassFinalizeFunc)yong_im_context_class_fini,
		NULL,
		sizeof(YongIMContext),
		0,
		(GInstanceInitFunc) yong_im_context_init,
	};
	type_yong_im_context=g_type_module_register_type(
		type_module,
		GTK_TYPE_IM_CONTEXT,
		"YongIMContext",
		&yong_im_context_info,
		0
	);
}

GType yong_im_context_get_type(void)
{
	if(type_yong_im_context==0)
		yong_im_context_register_type(NULL);
	return type_yong_im_context;
}

YongIMContext *yong_im_context_new(void)
{
	return g_object_new(YONG_TYPE_IM_CONTEXT,NULL);
}

static int check_app_type(void)
{
	int pid = getpid();
	int type=APP_NORMAL;
	char tstr0[64];
	char exec[256];
	int i;
	sprintf(tstr0, "/proc/%d/exe", pid);
	if((i=readlink(tstr0, exec, sizeof(exec))) > 0)
	{
		goto out;
	}
out:
	return type;
}

static void yong_im_context_class_init(YongIMContext *class)
{
	GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);
	im_context_class->set_client_widget = yong_im_context_set_client_widget;
	im_context_class->filter_keypress = yong_im_context_filter_keypress;
	im_context_class->reset = yong_im_context_reset;
	im_context_class->get_preedit_string = yong_im_context_get_preedit_string;
	im_context_class->focus_in = yong_im_context_focus_in;
	im_context_class->focus_out = yong_im_context_focus_out;
	im_context_class->set_cursor_location = yong_im_context_set_cursor_location;
	im_context_class->set_use_preedit = yong_im_context_set_use_preedit;
	im_context_class->set_surrounding = yong_im_context_set_surrounding;
	gobject_class->finalize = yong_im_context_finalize;
	
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
}

static void yong_im_context_class_fini(YongIMContextClass *class)
{
}

static void yong_im_context_init(YongIMContext *ctx)
{
	ctx->has_focus=0;
	ctx->use_preedit=1;
	ctx->is_wayland=GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default());
	ctx->id=_ctx_id++;
	ctx->cursor_area=(GdkRectangle){-1,-1,0,0};
	ctx->skip_cursor=FALSE;
	
	_ctx_list=g_slist_prepend(_ctx_list,ctx);
	client_add_ic(ctx->id);
}

static void yong_im_context_finalize(GObject *obj)
{
	YongIMContext *ctx=YONG_IM_CONTEXT(obj);
	_ctx_list=g_slist_remove(_ctx_list,ctx);
	if(ctx->preedit_string)
		g_free(ctx->preedit_string);
	if(ctx->client_widget)
		g_object_unref(ctx->client_widget);
	client_del_ic(ctx->id);
	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void yong_im_context_set_client_widget(GtkIMContext *context,GtkWidget *client_widget)
{
	YongIMContext *ctx=YONG_IM_CONTEXT(context);
	if(ctx->client_widget)
	{
		g_object_unref(ctx->client_widget);
		ctx->client_widget=NULL;
	}
	if(client_widget)
	{
		ctx->client_widget=g_object_ref(client_widget);
	}
}

static gboolean yong_im_context_filter_keypress(GtkIMContext *context,GdkEvent *event)
{
	YongIMContext *ctx=YONG_IM_CONTEXT(context);
	int release=gdk_event_get_event_type(event)==GDK_KEY_RELEASE;
	int key=0;
	gboolean res=FALSE;
	GdkModifierType state=gdk_event_get_modifier_state(event);
	guint keyval=gdk_key_event_get_keyval(event);
	guint32 event_time=gdk_event_get_time(event);
	if(!ctx->has_focus && !release && !(state&YONG_IGNORED_MASK))
	{
		yong_im_context_focus_in(context);
	}
	key=GetKey(keyval,state,release);
	if(ctx->has_focus && !(state&YONG_IGNORED_MASK))
	{
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
			res=client_input_key(ctx->id,key,event_time);
			if(key==_trigger && !release && res==0)
			{
				client_enable(ctx->id);
				_enable=TRUE;
				res=TRUE;
			}
		}
	}
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
		GtkIMContextClass *parent=(GtkIMContextClass*)parent_class;
    	if (parent->filter_keypress)
    		return (*parent->filter_keypress) (context, event);
	}
	return res;
}
static void yong_im_context_reset(GtkIMContext *context)
{
}

static gboolean _set_cursor_location_internal(YongIMContext *ctx)
{
	GtkRoot *root;
	GdkRectangle area;
	int scale;
	double px,py;
	GtkNative *native;
	GdkDisplay *display;
	
	if(ctx->client_widget==NULL)
		return FALSE;
	area = ctx->cursor_area;
	root=gtk_widget_get_root(ctx->client_widget);
	scale=gtk_widget_get_scale_factor(ctx->client_widget);
	gtk_widget_translate_coordinates(ctx->client_widget,GTK_WIDGET(root),area.x,area.y,&px,&py);
	area.x=px;area.y=py;
	native=gtk_widget_get_native(GTK_WIDGET(root));
	if(native)
	{
		double offsetX = 0, offsetY = 0;
		gtk_native_get_surface_transform(native, &offsetX, &offsetY);
		area.x+=offsetX;
		area.y+=offsetY;		
	}
	display=gtk_widget_get_display(ctx->client_widget);
	if(GDK_IS_X11_DISPLAY(display) && native)
	{
		GdkSurface *surface=gtk_native_get_surface(native);
		if(surface && GDK_IS_X11_SURFACE(surface))
		{
			if (area.x == -1 && area.y == -1 && area.width == 0 && area.height == 0)
			{
				area.x = 0;
				area.y += gdk_surface_get_height(surface);
			}
			int rootX, rootY;
			Window child;
			XTranslateCoordinates(
                    GDK_SURFACE_XDISPLAY(surface), GDK_SURFACE_XID(surface),
                    gdk_x11_display_get_xrootwindow(display), area.x * scale,
                    area.y * scale, &rootX, &rootY, &child);
            area.x=rootX/scale;
            area.y=rootY/scale;
		}
	}
	if(scale!=1)
	{
		area.x*=scale;
		area.y*=scale;
		area.width*=scale;
		area.height*=scale;
	}
	if(ctx->is_wayland)
		client_set_cursor_location_relative(ctx->id,&area);
	else
		client_set_cursor_location(ctx->id,&area);
	return FALSE;
}

static gboolean _focus_in_internal(YongIMContext *ctx)
{
	GSList *p;
	for(p=_ctx_list;p!=NULL;p=p->next)
	{
		((YongIMContext*)p->data)->has_focus=0;
	}
	ctx->has_focus=1;
	_set_cursor_location_internal(ctx);
	client_focus_in(ctx->id);
	return FALSE;
}

static gboolean _focus_out_internal(YongIMContext *ctx)
{
	ctx->has_focus=0;
	client_focus_out(ctx->id);
	return FALSE;
}

static void yong_im_context_focus_in(GtkIMContext *context)
{
	YongIMContext *ctx=YONG_IM_CONTEXT(context);
	_focus_in_internal(ctx);
}

static void yong_im_context_focus_out(GtkIMContext *context)
{
	YongIMContext *ctx=YONG_IM_CONTEXT(context);
	_focus_out_internal(ctx);
}

static void yong_im_context_set_cursor_location(GtkIMContext *context,GdkRectangle *area)
{
	YongIMContext *ctx=YONG_IM_CONTEXT(context);
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
static void yong_im_context_set_use_preedit(GtkIMContext *context,gboolean use_preedit)
{
	YongIMContext *ctx=YONG_IM_CONTEXT(context);
	ctx->use_preedit=use_preedit?1:0;
}
static void yong_im_context_get_preedit_string(GtkIMContext *context,gchar **str,PangoAttrList **attrs,gint *cursor_pos)
{
	YongIMContext *ctx=YONG_IM_CONTEXT(context);
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
static void yong_im_context_set_surrounding(GtkIMContext *context,const char *text, int len,int cursor_index)
{
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
	(void)client_input_key_async;
	int ret,res;
	ret=l_call_client_call("input",&res,"iii",id,key,time);
	if(ret!=0) return 0;
	return res?TRUE:FALSE;
}

static gboolean client_input_key_async(guint id,int key,guint32 time)
{
	int ret;
	ret=l_call_client_call("input",0,"iii",id,key,time);
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

static YongIMContext *find_context(guint id)
{
	GSList *p;
	for(p=_ctx_list;p!=NULL;p=p->next)
	{
		YongIMContext *ctx=p->data;
		if(ctx->id==id)
			return ctx;
	}
	return NULL;
}

static void client_connect(void)
{
	YongIMContext *ctx;
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
		YongIMContext *ctx;
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
		g_signal_emit(ctx,_signal_commit_id,0,text);
	}
	else if(!strcmp(name,"preedit"))
	{
		guint id;
		int ret;
		YongIMContext *ctx;
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
		YongIMContext *ctx;
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
			//printf("preedit end\n");
		}
	}
	else if(!strcmp(name,"forward"))
	{
		guint id;
		int ret;
		YongIMContext *ctx;
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
		YongIMContext *ctx;
		
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

#ifndef GDK_MOD2_MASK
#define GDK_MOD2_MASK	0x10
#endif

static int GetKey(guint KeyCode,guint KeyState,int release)
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
	if ((KeyState & GDK_ALT_MASK) && KeyCode!=YK_LALT && KeyCode!=YK_RALT)
		ret|=KEYM_ALT;
	if ((KeyState & GDK_SUPER_MASK))
		ret|=KEYM_SUPER;
	if(KeyState & GDK_MOD2_MASK)
		ret|=KEYM_KEYPAD;
	if(release)
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

static void ForwardKey(YongIMContext *ctx,int key)
{
	GtkWidget *widget=ctx->client_widget;
	if(!widget)
		return;
	if(GTK_IS_TEXT_VIEW(widget) || GTK_IS_ENTRY(widget) || GTK_IS_TEXT(widget))
	{
		switch(key){
		case CTRL_V:
			g_signal_emit_by_name(widget,"paste-clipboard",NULL);
			break;
		case YK_BACKSPACE:
			g_signal_emit_by_name(widget,"backspace",NULL);
			break;
		case YK_LEFT:
			g_signal_emit_by_name(widget,"move-cursor",GTK_MOVEMENT_LOGICAL_POSITIONS,-1,0,NULL);
			break;
		case YK_RIGHT:
			g_signal_emit_by_name(widget,"move-cursor",GTK_MOVEMENT_LOGICAL_POSITIONS,1,0,NULL);
			break;
		default:
			break;
		}
	}
}


/* TODO
  转发按键功能没有实现
 */
