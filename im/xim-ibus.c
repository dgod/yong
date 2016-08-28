#include "yong.h"
#include "xim.h"
#include "im.h"
#include "common.h"
#include <ctype.h>

#include <gtk/gtk.h>

#ifdef CFG_XIM_IBUS

#include <ibus.h>
#include "xim-ibus.h"

static void *ibus_so;
static int ibus_menu;

static IBusBus *(*p_ibus_bus_new)(void);
static GDBusConnection *(*p_ibus_bus_get_connection)(IBusBus *bus);
static guint (*p_ibus_bus_request_name)(IBusBus *bus,const gchar *name,guint flags);
static gboolean (*p_ibus_bus_register_component)(IBusBus *bus,IBusComponent *component);
static void  (*p_ibus_component_output_engines)(IBusComponent *component,GString *output,gint indent);

static GType (*p_ibus_engine_get_type)(void);
#undef IBUS_TYPE_ENGINE
#define IBUS_TYPE_ENGINE p_ibus_engine_get_type()

static GType (*p_ibus_object_get_type)(void);
#undef IBUS_TYPE_OBJECT
#define IBUS_TYPE_OBJECT p_ibus_object_get_type()

static IBusEngineDesc *(*p_ibus_engine_desc_new)(const gchar *name,
                                                 const gchar *longname,
                                                 const gchar *description,
                                                 const gchar *language,
                                                 const gchar *license,
                                                 const gchar *author,
                                                 const gchar *icon,
                                                 const gchar *layout);
                                                 
static IBusEngineDesc *(*p_ibus_engine_desc_new_varargs)(const gchar *first_property_name,...);
												 
static IBusComponent *(*p_ibus_component_new)(const gchar *name,
                                                 const gchar *descritpion,
                                                 const gchar *version,
                                                 const gchar *license,
                                                 const gchar *author,
                                                 const gchar *homepage,
                                                 const gchar *exec,
                                                 const gchar *textdomain);

static const gchar *(*p_ibus_engine_get_name)(IBusEngine *engine);
static void (*p_ibus_engine_hide_preedit_text)(IBusEngine *engine);
static void (*p_ibus_engine_commit_text)(IBusEngine *engine,IBusText *text);
static void (*p_ibus_engine_forward_key_event)(IBusEngine *engine,guint keyval,gboolean is_press,guint state);
static void (*p_ibus_engine_update_preedit_text)(IBusEngine *engine,IBusText *text,guint cursor_pos,gboolean visible);
static void (*p_ibus_component_add_engine)(IBusComponent *component,IBusEngineDesc *engine);

static IBusText *(*p_ibus_text_new_from_string)(const gchar *str);																					 
static IBusText *(*p_ibus_text_new_from_static_string)(const gchar *str);
static guint (*p_ibus_text_get_length)(IBusText *text);
static void (*p_ibus_text_append_attribute)(IBusText *text,guint type,guint value,guint start_index,gint end_index);

static IBusFactory *(*p_ibus_factory_new)(GDBusConnection *connection);
static void (*p_ibus_factory_add_engine)(IBusFactory *factory,const gchar *name,GType type);

typedef struct _IBusYongEngine IBusYongEngine;
typedef struct _IBusYongEngineClass IBusYongEngineClass;
 
struct _IBusYongEngine {
	IBusEngine parent;
	//CONNECT_ID id;
	int state;
	int x;
	int y;
};
 
struct _IBusYongEngineClass {
	IBusEngineClass parent;
};

static int ibus_version;
static IBusYongEngine *cur_engine=NULL;
static CONNECT_ID xim_ibus_id={
	.x=POSITION_ORIG,
	.y=POSITION_ORIG
};

static void  ibus_yong_engine_class_init(IBusYongEngineClass *klass);
static void  ibus_yong_engine_init(IBusYongEngine *yong);
static GObject*ibus_yong_engine_constructor(GType type,guint n_construct_params,GObjectConstructParam *construct_params);
static void  ibus_yong_engine_destroy(IBusYongEngine *yong);

static gboolean ibus_yong_engine_process_key_event(IBusEngine *engine,guint keyval,guint keycode,guint modifiers);
static void ibus_yong_engine_focus_in(IBusEngine *engine);
static void ibus_yong_engine_focus_out(IBusEngine *engine);
static void ibus_yong_engine_reset(IBusEngine *engine);
static void ibus_yong_engine_enable(IBusEngine *engine);
static void ibus_yong_engine_disable(IBusEngine *engine);
static void ibus_yong_engine_set_cursor_location(IBusEngine *engine,gint x,gint y,gint w,gint h);

static IBusEngineClass *parent_class = NULL;

#define IBUS_TYPE_YONG_ENGINE  \
  (ibus_yong_engine_get_type ())
 
GType ibus_yong_engine_get_type (void)
{
  static GType type = 0;
 
  static const GTypeInfo type_info = {
    sizeof (IBusYongEngineClass),
    (GBaseInitFunc)    NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc)  ibus_yong_engine_class_init,
    NULL,
    NULL,
    sizeof (IBusYongEngine),
    0,
    (GInstanceInitFunc)ibus_yong_engine_init,
  };
 
  if (type == 0) {
    type = g_type_register_static (IBUS_TYPE_ENGINE,
                   "IBusYongEngine",
                   &type_info,
                   (GTypeFlags) 0);
  }
 
  return type;
}

static void ibus_yong_engine_class_init (IBusYongEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);
	IBusEngineClass *engine_class = IBUS_ENGINE_CLASS (klass);
 
	parent_class = (IBusEngineClass *) g_type_class_peek_parent (klass);
 
	object_class->constructor = ibus_yong_engine_constructor;
	ibus_object_class->destroy = (IBusObjectDestroyFunc) ibus_yong_engine_destroy;
 
	engine_class->process_key_event = ibus_yong_engine_process_key_event;
 
	engine_class->reset = ibus_yong_engine_reset;
	engine_class->enable = ibus_yong_engine_enable;
	engine_class->disable = ibus_yong_engine_disable;
 
	engine_class->focus_in = ibus_yong_engine_focus_in;
	engine_class->focus_out = ibus_yong_engine_focus_out;
	
 	engine_class->set_cursor_location=ibus_yong_engine_set_cursor_location;
}

static void ibus_yong_engine_init (IBusYongEngine *yong)
{
	CONNECT_ID *id=&xim_ibus_id;//&yong->id;
	memset(id,0,sizeof(CONNECT_ID));
	id->track=1;
	id->biaodian=im.Biaodian;
	id->trad=im.TradDef;
	yong->x=POSITION_ORIG;
	yong->y=POSITION_ORIG;
	yong->state=0;
}

static GObject *ibus_yong_engine_constructor(GType type,guint n_construct_params,GObjectConstructParam *construct_params)
{
    IBusYongEngine *yong;
 
    yong = (IBusYongEngine *)G_OBJECT_CLASS (parent_class)->constructor(type,n_construct_params,construct_params);
 
    return (GObject *)yong;
}

static void ibus_yong_engine_destroy (IBusYongEngine *yong)
{
	if(cur_engine==yong)
		cur_engine=NULL;
	IBUS_OBJECT_CLASS (parent_class)->destroy((IBusObject *)yong);
}

/* use xim's GetKey */
int GetKey(int KeyCode,int KeyState);

static gboolean ibus_yong_engine_process_key_event(IBusEngine *engine,guint keyval,guint keycode,guint modifiers)
{
	CONNECT_ID *id;
	int key;
	static int last_press;
	static uint32_t last_press_time;
	
	//printf("%x %x\n",modifiers,keyval);
		
	if(ibus_version>=1)
		modifiers=keycode;
	
	cur_engine=(IBusYongEngine*)engine;
	id=&xim_ibus_id;//&cur_engine->id;
	id->focus=1;
	
	key=GetKey(keyval,modifiers);
	if(!key)
		return FALSE;
	if(!(modifiers & IBUS_RELEASE_MASK))
	{
		if(!last_press)
			last_press_time=y_im_tick();
		last_press=key;
	}
	if(key>=YK_LSHIFT && key<=YK_RALT)
	{
		if(!(modifiers & IBUS_RELEASE_MASK))
			return FALSE;
		if(key!=last_press || y_im_tick()-last_press_time>300)
		{
			last_press=0;
			return FALSE;
		}
	}
	if (modifiers & IBUS_RELEASE_MASK)
	{
		if(last_press==key)
			last_press=0;
	}
	switch(key){
	case CTRL_SPACE:
	case SHIFT_SPACE:
	{
		if(!(modifiers & IBUS_RELEASE_MASK) && YongHotKey(key))
			return TRUE;
		break;
	}
	case CTRL_LSHIFT:
	case CTRL_RSHIFT:
	{
		if(!(modifiers & IBUS_RELEASE_MASK) && key==last_press && YongHotKey(key))
			return TRUE;
		break;
	}
	case YK_LCTRL:
	case YK_RCTRL:
	case YK_LSHIFT:
	case YK_RSHIFT:
	{
		y_im_input_key(key);
		return FALSE;
	}
	default:
	{
		if((modifiers & IBUS_RELEASE_MASK) && YongHotKey(key))
			return TRUE;
		break;
	}}
	if(id->state && !(modifiers & IBUS_RELEASE_MASK))
	{
		int mod=KEYM_MASK&key;
		key&=~KEYM_CAPS;
		if(YongKeyInput(key,mod))
		{
			y_im_speed_update(key,0);
			return TRUE;
		}
	}
	return FALSE;
}

static void ibus_yong_auto_switch(void)
{
	const char *engine;
	int ret,index;

	if(!ibus_menu)
		return;
	if(!cur_engine)
		return;
	engine=p_ibus_engine_get_name((IBusEngine*)cur_engine);
	ret=l_sscanf(engine,"yong:%d",&index);
	if(ret==1)
		YongSwitchIM(index);
}

static void ibus_yong_engine_focus_in(IBusEngine *engine)
{
	CONNECT_ID *id;
	IBusYongEngine *cur=(IBusYongEngine*)engine;
	//printf("focus in %p\n",engine);
	id=&xim_ibus_id;//&cur->id;
	if(id->state)
		cur_engine=cur;
	else
		cur_engine=0;
	id->focus=1;
	id->x=cur->x;
	id->y=cur->y;
	id->state=cur->state;
	
	ibus_yong_auto_switch();

	YongMoveInput(id->x,id->y);
	YongShowInput(id->state);
	YongShowMain(id->state);
	parent_class->focus_in(engine);
}

static void ibus_yong_engine_focus_out(IBusEngine *engine)
{
	CONNECT_ID *id;
	IBusYongEngine *cur=(IBusYongEngine*)engine;
	//printf("focus out\n");
	if(cur!=cur_engine)
		return;
	id=&xim_ibus_id;//&cur->id;
	id->focus=0;
	YongShowInput(0);
	YongShowMain(0);
	YongResetIM();
	parent_class->focus_out (engine);
}

static void ibus_yong_engine_reset(IBusEngine *engine)
{
	/* ibus will call this when mouse click happen */
	/* I don't wan't it */
	//printf("reset\n");
	//YongResetIM();
    parent_class->reset (engine);
}

static void ibus_yong_engine_enable(IBusEngine *engine)
{
	//CONNECT_ID *id;
	//printf("enable\n");
	cur_engine=(IBusYongEngine*)engine;
	//id=&xim_ibus_id;
	ibus_yong_auto_switch();
	xim_ibus_enable(1);
	((IBusYongEngine*)engine)->state=1;
	parent_class->enable(engine);
}

static void ibus_yong_engine_disable(IBusEngine *engine)
{
	//CONNECT_ID *id;
	//id=&xim_ibus_id;
	//printf("disable\n");
	xim_ibus_enable(0);
	cur_engine=0;
	((IBusYongEngine*)engine)->state=0;
	parent_class->disable(engine);
}

static void ibus_yong_engine_set_cursor_location(IBusEngine *engine,gint x,gint y,gint w,gint h)
{
	CONNECT_ID *id;
	
	((IBusYongEngine*)engine)->x=x+w;
	((IBusYongEngine*)engine)->y=y+h;
	
	id=&xim_ibus_id;//&((IBusYongEngine*)engine)->id;
	if(cur_engine && !id->focus)
		return;
	YongMoveInput(x+w,y+h);
}

static IBusBus *bus = NULL;
static IBusFactory *factory = NULL;
static int preedit;

static void ibus_disconnected_cb (IBusBus *bus,gpointer user_data)
{
	gtk_main_quit();
}

static char *ibus_get_icon_path(int index)
{
	gchar *cur=g_get_current_dir();
	gchar *path;
	char *icon;
	icon=y_im_get_im_config_string(index,"icon");
	if(icon)
	{
		if(strchr(icon,'/'))
			path=g_build_filename(cur,icon,NULL);
		else
			path=g_build_filename(cur,"/skin/",icon,NULL);
		l_free(icon);
	}
	else
	{
		path=g_build_filename(cur,"/skin/tray1.png",NULL);
	}
	g_free(cur);
	return path;
}

static int xim_ibus_base_init(void)
{
	ibus_so=dlopen("libibus.so",RTLD_LAZY);
	if(!ibus_so)
		ibus_so=dlopen("libibus-1.0.so.5",RTLD_LAZY);
	if(!ibus_so)
		ibus_so=dlopen("libibus-1.0.so.0",RTLD_LAZY);
	if(!ibus_so)
		ibus_so=dlopen("libibus.so.2",RTLD_LAZY);
	if(!ibus_so)
		ibus_so=dlopen("libibus.so.1",RTLD_LAZY);
	if(!ibus_so)
	{
		printf("yong: no ibus interface found %s\n",dlerror());
		return -1;
	}
	preedit=y_im_get_config_int("IM","onspot");

	p_ibus_bus_new=dlsym(ibus_so,"ibus_bus_new");
	p_ibus_bus_get_connection=dlsym(ibus_so,"ibus_bus_get_connection");
	p_ibus_bus_request_name=dlsym(ibus_so,"ibus_bus_request_name");
	p_ibus_bus_register_component=dlsym(ibus_so,"ibus_bus_register_component");
	p_ibus_component_output_engines=dlsym(ibus_so,"ibus_component_output_engines");

	p_ibus_engine_get_type=dlsym(ibus_so,"ibus_engine_get_type");
	p_ibus_object_get_type=dlsym(ibus_so,"ibus_object_get_type");
	
	p_ibus_engine_get_name=dlsym(ibus_so,"ibus_engine_get_name");
	p_ibus_engine_desc_new=dlsym(ibus_so,"ibus_engine_desc_new");
	p_ibus_engine_desc_new_varargs=dlsym(ibus_so,"ibus_engine_desc_new_varargs");
	p_ibus_engine_hide_preedit_text=dlsym(ibus_so,"ibus_engine_hide_preedit_text");
	p_ibus_engine_commit_text=dlsym(ibus_so,"ibus_engine_commit_text");
	p_ibus_engine_update_preedit_text=dlsym(ibus_so,"ibus_engine_update_preedit_text");
	p_ibus_engine_forward_key_event=dlsym(ibus_so,"ibus_engine_forward_key_event");
	
	p_ibus_text_new_from_string=dlsym(ibus_so,"ibus_text_new_from_string");
	p_ibus_text_new_from_static_string=dlsym(ibus_so,"ibus_text_new_from_static_string");
	p_ibus_text_get_length=dlsym(ibus_so,"ibus_text_get_length");
	p_ibus_text_append_attribute=dlsym(ibus_so,"ibus_text_append_attribute");
	
	p_ibus_component_new=dlsym(ibus_so,"ibus_component_new");
	p_ibus_component_add_engine=dlsym(ibus_so,"ibus_component_add_engine");

	p_ibus_factory_new=dlsym(ibus_so,"ibus_factory_new");
	p_ibus_factory_add_engine=dlsym(ibus_so,"ibus_factory_add_engine");
	
	return 0;
}

int xim_ibus_init(void)
{
	if(xim_ibus_base_init())
		return -1;
	bus = p_ibus_bus_new();
	g_signal_connect (bus, "disconnected", G_CALLBACK (ibus_disconnected_cb), NULL);
	factory = p_ibus_factory_new (p_ibus_bus_get_connection(bus));
    if(!ibus_menu)
    {
		p_ibus_factory_add_engine (factory, "yong", IBUS_TYPE_YONG_ENGINE);
	}
	else
	{
		int i;
		for(i=0;i<32;i++)
		{
			char temp[64];
			char *name,*engine;
			sprintf(temp,"%d",i);
			name=y_im_get_im_name(i);
			if(!name) break;
			engine=l_sprintf("yong:%s",temp);
			l_gb_to_utf8(name,temp,sizeof(temp));
			p_ibus_factory_add_engine (factory, engine, IBUS_TYPE_YONG_ENGINE);
			l_free(name);l_free(engine);
		}
	}

	p_ibus_bus_request_name (bus, "org.freedesktop.IBus.Yong", 0);
	return 0;
}

int xim_ibus_output_xml(void)
{
	IBusComponent *component;
	GString *output;
	
	if(xim_ibus_base_init())
		return -1;
	
	component=p_ibus_component_new("org.freedesktop.IBus.Yong",
                                    "Yong input method",
                                    "2.0.0",
                                    "",
                                    "dgod <dgod@gmail.com>",
                                    "http://yong.uueasy.com",
                                    "",
                                    "yong");	
	if(!component) return -1;
	 if(!ibus_menu)
    {
		char *icon=ibus_get_icon_path(-1);
		if(p_ibus_engine_desc_new_varargs)
		{
			p_ibus_component_add_engine (component,
							p_ibus_engine_desc_new_varargs ("name", "yong",
                                         "longname", "Yong",
                                         "description", "Yong Input Method",
                                         "language", "zh_CN",
                                         "license", "",
                                         "author", "dgod <dgod.osa@gmail.com>",
                                         "icon", icon,
                                         "layout", "us",
                                         "setup","/usr/bin/yong-config",
                                         NULL));
		}
		else
		{
			p_ibus_component_add_engine (component,
                               p_ibus_engine_desc_new ("yong",
                                                     "Yong",
                                                     "Yong Input Method",
                                                     "zh_CN",
                                                     "",
                                                     "dgod <dgod.osa@gmail.com>",
                                                     icon,
                                                     "us"));
		}
		g_free(icon);
	}
	else
	{
		int i;
		for(i=0;i<32;i++)
		{
			char temp[64];
			char *name,*engine;
			char *icon=ibus_get_icon_path(i);
			sprintf(temp,"%d",i);
			name=y_im_get_im_name(i);
			if(!name) break;
			engine=l_sprintf("yong:%s",temp);
			l_gb_to_utf8(name,temp,sizeof(temp));
			if(p_ibus_engine_desc_new_varargs)
			{
				p_ibus_component_add_engine (component,
							p_ibus_engine_desc_new_varargs ("name", engine,
                                         "longname",temp,
                                         "description", "",
                                         "language", "zh_CN",
                                         "license", "",
                                         "author", "",
                                         "icon", icon,
                                         "layout", "us",
                                         "setup","/usr/bin/yong-config",
                                         NULL));
			}
			else
			{
				p_ibus_component_add_engine (component,
                               p_ibus_engine_desc_new (engine,
                                                     temp,
                                                     "",
                                                     "zh_CN",
                                                     "",
                                                     "",
                                                     icon,
                                                     "us"));
			}
			g_free(icon);
			l_free(name);l_free(engine);
		}
	}
	
	output = g_string_new ("");
    p_ibus_component_output_engines(component, output, 1);
    fprintf (stdout, "%s", output->str);
    g_string_free (output, TRUE);
	g_object_unref(component);
	return 0;
}

void xim_ibus_menu_enable(int enable)
{
	ibus_menu=enable;
}

int xim_ibus_use_ibus_menu(void)
{
	return ibus_menu;
}

void xim_ibus_destroy(void)
{
}

CONNECT_ID *xim_ibus_get_connect(void)
{
	//return cur_engine?&cur_engine->id:0;
	return cur_engine?&xim_ibus_id:0;
}

static void xim_ibus_put_connect(CONNECT_ID *id)
{
	if(id==&xim_ibus_id)
		return;
	if(id->dummy)
	{
		CONNECT_ID *p=&xim_ibus_id;
		if(!p) return;
		p->corner=id->corner;
		p->lang=id->lang;
		p->biaodian=id->biaodian;
		p->trad=id->trad;
	}
}

void xim_ibus_enable(int enable)
{
	CONNECT_ID *id;
	if(!cur_engine)
		return;
	id=&xim_ibus_id;//&cur_engine->id;
	if(id->state==enable)
		return;
	if(enable==-1)
		return;
	id->state=enable;
	if(enable)
		YongSetLang(LANG_CN);
	YongShowInput(enable);
	YongShowMain(enable);
}

static int GetKey_r(int yk)
{
	int vk;

	switch(yk){
	case YK_BACKSPACE:vk=IBUS_BackSpace;break;
	case YK_DELETE:vk=IBUS_Delete;break;
	case YK_ENTER:vk=IBUS_Return;break;
	case YK_HOME:vk=IBUS_Home;break;
	case YK_END:vk=IBUS_End;break;
	case YK_PGUP:vk=IBUS_Page_Up;break;
	case YK_PGDN:vk=IBUS_Page_Down;break;
	case YK_LEFT:vk=IBUS_Left;break;
	case YK_DOWN:vk=IBUS_Down;break;
	case YK_UP:vk=IBUS_Up;break;
	case YK_RIGHT:vk=IBUS_Right;break;
	case 'V':vk=IBUS_V;break;
	default:return -1;
	}
	return vk;
}

void xim_ibus_forward_key(int key)
{
	int KeyCode,KeyState;
	if(!cur_engine)
		return;
	KeyCode=GetKey_r(key&~KEYM_MASK);
	if(KeyCode==-1)
		return;
	KeyState=0;
	if(key & KEYM_CTRL)
		KeyState|=IBUS_CONTROL_MASK;
	if(key & KEYM_SHIFT)
		KeyState|=IBUS_SHIFT_MASK;
	p_ibus_engine_forward_key_event((IBusEngine*)cur_engine,KeyCode,TRUE,KeyState);
	p_ibus_engine_forward_key_event((IBusEngine*)cur_engine,KeyCode,FALSE,KeyState);
}

int xim_ibus_trigger_key(int key)
{
	return 0;
}

void xim_ibus_send_string(const char *s,int flags)
{
	char out[512];
	IBusText *text;

	if(!cur_engine)
		return;	
	xim_ibus_preedit_clear();
	y_im_str_encode(s,out,flags);
	text = p_ibus_text_new_from_string (out);
	p_ibus_engine_commit_text ((IBusEngine *) cur_engine, text);
	//g_object_unref (text);	
}

int xim_ibus_preedit_clear(void)
{
	if(!cur_engine || !preedit)
		return 0;
	p_ibus_engine_hide_preedit_text ((IBusEngine *)cur_engine);
	return 0;
}

int xim_ibus_preedit_draw(const char *s,int len)
{
	IBusText *text;
	if(!cur_engine || !preedit)
		return 0;
	if (s != NULL && s[0] != 0)
	{
		int length;
		text = p_ibus_text_new_from_string(s);
		length=p_ibus_text_get_length(text);
		p_ibus_text_append_attribute (text, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_LOW, 0, -1);
		p_ibus_engine_update_preedit_text ((IBusEngine *)cur_engine,text,length,TRUE);
	}
	else
	{
		text = p_ibus_text_new_from_string ("");
		p_ibus_engine_update_preedit_text ((IBusEngine *)cur_engine, text, 0, FALSE);
	}
	//g_object_unref(text);
	return 0;
}

int y_xim_init_ibus(Y_XIM *x)
{
	x->init=xim_ibus_init;
	x->destroy=xim_ibus_destroy;
	x->enable=xim_ibus_enable;
	x->forward_key=xim_ibus_forward_key;
	x->trigger_key=xim_ibus_trigger_key;
	x->send_string=xim_ibus_send_string;
	x->preedit_clear=xim_ibus_preedit_clear;
	x->preedit_draw=xim_ibus_preedit_draw;
	x->get_connect=xim_ibus_get_connect;
	x->put_connect=xim_ibus_put_connect;
	x->name="ibus";
	return 0;
}

#endif /*CFG_XIM_IBUS*/
