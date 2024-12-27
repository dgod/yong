#include "yong.h"
#include "xim.h"
#include "im.h"
#include "common.h"
#include "ybus.h"
#include "ltricky.h"

#include <ibus.h>

static int xim_getpid(CONN_ID conn_id);
static int xim_config(CONN_ID conn_id,CLIENT_ID client_id,const char *config,...);
static void xim_open_im(CONN_ID conn_id,CLIENT_ID client_id);
static void xim_close_im(CONN_ID conn_id,CLIENT_ID client_id);
static void xim_preedit_clear(CONN_ID conn_id,CLIENT_ID client_id);
static int xim_preedit_draw(CONN_ID conn_id,CLIENT_ID client_id,const char *s);
static void xim_send_string(CONN_ID conn_id,CLIENT_ID client_id,const char *s,int flags);
static void xim_send_key(CONN_ID conn_id,CLIENT_ID client_id,int key,int repeat);
static int xim_init(void);

static YBUS_PLUGIN plugin={
	.name="ibus",
	.init=xim_init,
	.getpid=xim_getpid,
	.config=xim_config,
	.open_im=xim_open_im,
	.close_im=xim_close_im,
	.preedit_clear=xim_preedit_clear,
	.preedit_draw=xim_preedit_draw,
	.send_string=xim_send_string,
	.send_key=xim_send_key,
};
static int trigger=CTRL_SPACE;
static int onspot=0;
static IBusBus *bus;
static IBusFactory *factory=NULL;
static void *ibus_so;
static int ibus_menu;
static int ibus_enable;
static int ibus_assist;

static IBusBus *(*p_ibus_bus_new)(void);
static IBusBus *(*p_ibus_bus_new_async)(void);
static GDBusConnection *(*p_ibus_bus_get_connection)(IBusBus *bus);
static guint (*p_ibus_bus_request_name)(IBusBus *bus,const gchar *name,guint flags);
static gboolean (*p_ibus_bus_register_component)(IBusBus *bus,IBusComponent *component);
static void (*p_ibus_component_output)(IBusComponent *component,GString *output,gint indent);
static void (*p_ibus_component_output_engines)(IBusComponent *component,GString *output,gint indent);

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
static void (*p_ibus_engine_get_content_type)(IBusEngine *engine,guint *purpose,guint *hints);
static void (*p_ibus_component_add_engine)(IBusComponent *component,IBusEngineDesc *engine);

static void (*p_ibus_text_append_attribute)(IBusText *text,guint type,guint value,guint start_index,gint end_index);
static IBusText *(*p_ibus_text_new_from_string)(const gchar *str);																	 
static IBusText *(*p_ibus_text_new_from_static_string)(const gchar *str);
static guint (*p_ibus_text_get_length)(IBusText *text);
static void (*p_ibus_text_append_attribute)(IBusText *text,guint type,guint value,guint start_index,gint end_index);

static IBusFactory *(*p_ibus_factory_new)(GDBusConnection *connection);
static void (*p_ibus_factory_add_engine)(IBusFactory *factory,const gchar *name,GType type);

static void (*p_ibus_engine_update_auxiliary_text)(IBusEngine *engine,IBusText *text,gboolean visible);
static void (*p_ibus_engine_show_auxiliary_text)(IBusEngine *engine);
static void (*p_ibus_engine_hide_auxiliary_text)(IBusEngine *engine);
static void (*p_ibus_engine_update_lookup_table)(IBusEngine *engine,IBusLookupTable *lookup_table,gboolean visible);
static void (*p_ibus_engine_show_lookup_table)(IBusEngine *engine);
static void (*p_ibus_engine_hide_lookup_table)(IBusEngine *engine);

static IBusLookupTable *(*p_ibus_lookup_table_new)(guint page_size,guint cursor_pos,gboolean cursor_visible,gboolean round);
static void (*p_ibus_lookup_table_append_candidate) (IBusLookupTable *table,IBusText *text);
static void (*p_ibus_lookup_table_set_orientation)(IBusLookupTable *table,gint orientation);

typedef struct _IBusYongEngine IBusYongEngine;
typedef struct _IBusYongEngineClass IBusYongEngineClass;

struct _IBusYongEngine {
	IBusEngine parent;

	int last_press;
	uint32_t last_press_time;
	uint8_t first;
};
 
struct _IBusYongEngineClass {
	IBusEngineClass parent;
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
 
static GType ibus_yong_engine_get_type (void)
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
                   "IBusYongEngine2",
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
	yong->last_press=0;
	yong->last_press_time=0;
	yong->first=1;
	ybus_add_connect(&plugin,(uintptr_t)yong);
}

static GObject *ibus_yong_engine_constructor(GType type,guint n_construct_params,GObjectConstructParam *construct_params)
{
    IBusYongEngine *yong;
 
    yong = (IBusYongEngine *)G_OBJECT_CLASS (parent_class)->constructor(type,n_construct_params,construct_params);
 
    return (GObject *)yong;
}

static void ibus_yong_engine_destroy (IBusYongEngine *yong)
{
	YBUS_CONNECT *yconn;
	yconn=ybus_find_connect(&plugin,(uintptr_t)yong);
	if(yconn)
	{
		ybus_free_connect(yconn);
	}

	IBUS_OBJECT_CLASS (parent_class)->destroy((IBusObject *)yong);
}

/* use xim's GetKey */
int GetKey(int KeyCode,int KeyState);

static gboolean ibus_yong_engine_process_key_event(IBusEngine *engine,guint keyval,guint keycode,guint modifiers)
{
	IBusYongEngine *yong=(IBusYongEngine*)engine;
	int key;
	
	key=GetKey(keyval,modifiers);
	if(!key)
		return FALSE;
	int caps=key&KEYM_CAPS;
	key&=~KEYM_CAPS;
	if(!(modifiers & IBUS_RELEASE_MASK))
	{
		if(!yong->last_press)
			yong->last_press_time=y_im_tick();
		yong->last_press=key;
	}
	if(key>=YK_LSHIFT && key<=YK_RWIN)
	{
		if(!(modifiers & IBUS_RELEASE_MASK))
			return FALSE;
		if(key!=yong->last_press || y_im_tick()-yong->last_press_time>300)
		{
			yong->last_press=0;
			return FALSE;
		}
	}
	if (modifiers & IBUS_RELEASE_MASK)
	{
		if(yong->last_press==key)
			yong->last_press=0;
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
		if(!(modifiers & IBUS_RELEASE_MASK) && key==yong->last_press && YongHotKey(key))
			return TRUE;
		break;
	}
	case YK_LCTRL:
	case YK_RCTRL:
	case YK_LSHIFT:
	case YK_RSHIFT:
	{
		ybus_on_key(&plugin,(CONN_ID)engine,0,key);
		return FALSE;
	}
	default:
	{
		if(!(modifiers & IBUS_RELEASE_MASK) && YongHotKey(key))
			return TRUE;
		break;
	}}
	YBUS_CLIENT *client=NULL;
	ybus_get_active(NULL,&client);
	if(client==NULL)
	{
		ibus_yong_engine_focus_in(engine);
		ybus_get_active(NULL,&client);
	}
	if(client && client->state && !(modifiers & IBUS_RELEASE_MASK))
	{
		if(ybus_on_key(&plugin,(CONN_ID)engine,0,key|caps))
		{
			return TRUE;
		}
	}
	return FALSE;
}

static void ibus_yong_auto_switch(IBusEngine *engine)
{
	int ret,index;
	const char *name;

	if(!ibus_menu)
		return;
	name=p_ibus_engine_get_name((IBusEngine*)engine);
	ret=l_sscanf(name,"yong:%d",&index);
	if(ret==1)
		YongSwitchIM(index);
}

static void ibus_yong_engine_focus_in(IBusEngine *engine)
{
	// work around ibus focus problem
	YBUS_CONNECT *active=NULL;
	if(!ybus_get_active(&active,NULL) && active->plugin!=&plugin)
	{
		int64_t now=ybus_now();
		if(llabs(now-active->alive)<50)
		{
			return;
		}
	}
	//fprintf(stderr,"focus in\n");
	ibus_yong_auto_switch(engine);
	if(((IBusYongEngine*)engine)->first)
	{
		if(y_im_get_config_int("IM","enable"))
			ybus_on_open(&plugin,(CONN_ID)engine,0);
		((IBusYongEngine*)engine)->first=0;
	}
	ybus_on_focus_in(&plugin,(CONN_ID)engine,0);
	parent_class->focus_in(engine);
}

static void ibus_yong_engine_focus_out(IBusEngine *engine)
{
	//fprintf(stderr,"focus out\n");
	ybus_on_focus_out(&plugin,(CONN_ID)engine,0);
	parent_class->focus_out (engine);
}

static void ibus_yong_engine_reset(IBusEngine *engine)
{
    parent_class->reset(engine);
}

static void ibus_yong_engine_enable(IBusEngine *engine)
{
	YBUS_CONNECT *connect;
	connect=ybus_find_connect(&plugin,(CONN_ID)engine);
	if(!connect)
		return;
	ibus_yong_auto_switch(engine);
	ybus_add_client(connect,0,0);
	((IBusYongEngine*)engine)->first=1;
	parent_class->enable(engine);
}

static void ibus_yong_engine_disable(IBusEngine *engine)
{
	ybus_on_close(&plugin,(CONN_ID)engine,0);
	parent_class->disable(engine);
}

static void ibus_yong_engine_set_cursor_location(IBusEngine *engine,gint x,gint y,gint w,gint h)
{
	YBUS_CONNECT *yconn;
	YBUS_CLIENT *client,*active=NULL;

	yconn=ybus_find_connect(&plugin,(uintptr_t)engine);
	if(!yconn)
		return;

	client=ybus_add_client(yconn,0,0);
	if(!client)
		return;
	client->track=1;
	client->x=x+w;
	client->y=y+h;
	ybus_get_active(NULL,&active);
	if(active==client)
		YongMoveInput(x+w,y+h);
}

static int xim_getpid(CONN_ID conn_id)
{
	return 0;
}

static int xim_config(CONN_ID conn_id,CLIENT_ID client_id,const char *config,...)
{
	va_list ap;
	int ret=0;
	va_start(ap,config);
	if(!strcmp(config,"trigger"))
	{
		int key=va_arg(ap,int);		
		trigger=key;
	}
	else if(!strcmp(config,"onspot"))
	{
		onspot=va_arg(ap,int);
	}
	else
	{
		ret=-1;
	}
	va_end(ap);
	return ret;
}

static void xim_open_im(CONN_ID conn_id,CLIENT_ID client_id)
{
	ybus_on_open(&plugin,conn_id,client_id);
}

static void xim_close_im(CONN_ID conn_id,CLIENT_ID client_id)
{
	ybus_on_close(&plugin,conn_id,client_id);
}

static void xim_preedit_clear(CONN_ID conn_id,CLIENT_ID client_id)
{
	p_ibus_engine_hide_preedit_text ((IBusEngine *)conn_id);
}

static int xim_preedit_draw(CONN_ID conn_id,CLIENT_ID client_id,const char *s)
{
	IBusText *text;
	if(!onspot)
		return 0;
	if (s != NULL && s[0] != 0)
	{
		int length;
		char out[512];
		y_im_str_encode(s,out,0);
		text = p_ibus_text_new_from_string(out);
		length=p_ibus_text_get_length(text);
		p_ibus_text_append_attribute (text, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_LOW, 0, -1);
		p_ibus_engine_update_preedit_text ((IBusEngine *)conn_id,text,length,TRUE);
	}
	else
	{
		text = p_ibus_text_new_from_string ("");
		p_ibus_engine_update_preedit_text ((IBusEngine *)conn_id, text, 0, FALSE);
	}
	//g_object_unref(text);
	return 0;
}

static void xim_send_string(CONN_ID conn_id,CLIENT_ID client_id,const char *s,int flags)
{
	char out[512];
	IBusText *text;

	xim_preedit_clear(conn_id,client_id);
	y_im_str_encode(s,out,flags);
	text = p_ibus_text_new_from_string (out);
	p_ibus_engine_commit_text ((IBusEngine *)conn_id , text);
	//g_object_unref (text);	

}

static int GetKey_r(int yk)
{
	int vk;

	switch(yk){
	case YK_BACKSPACE:vk=IBUS_BackSpace;break;
	case YK_ESC:vk=IBUS_Escape;break;
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

static void xim_send_key(CONN_ID conn_id,CLIENT_ID client_id,int key,int repeat)
{
	int KeyCode,KeyState;
	KeyCode=GetKey_r(key&~KEYM_MASK);
	if(KeyCode==-1)
		return;
	KeyState=0;
	if(key & KEYM_CTRL)
		KeyState|=IBUS_CONTROL_MASK;
	if(key & KEYM_SHIFT)
		KeyState|=IBUS_SHIFT_MASK;
	for(int i=0;i<repeat;i++)
	{
		KeyState&=~IBUS_RELEASE_MASK;
		p_ibus_engine_forward_key_event((IBusEngine*)conn_id,KeyCode,0,KeyState);
		KeyState|=IBUS_RELEASE_MASK;
		p_ibus_engine_forward_key_event((IBusEngine*)conn_id,KeyCode,0,KeyState);
	}
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
	onspot=y_im_get_config_int("IM","onspot");

	p_ibus_bus_new=dlsym(ibus_so,"ibus_bus_new");
	p_ibus_bus_new_async=dlsym(ibus_so,"ibus_bus_new_async");
	p_ibus_bus_get_connection=dlsym(ibus_so,"ibus_bus_get_connection");
	p_ibus_bus_request_name=dlsym(ibus_so,"ibus_bus_request_name");
	p_ibus_bus_register_component=dlsym(ibus_so,"ibus_bus_register_component");
	p_ibus_component_output=dlsym(ibus_so,"ibus_component_output");
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
	p_ibus_engine_get_content_type=dlsym(ibus_so,"ibus_engine_get_content_type");

	p_ibus_text_append_attribute=dlsym(ibus_so,"ibus_text_append_attribute");
	p_ibus_text_new_from_string=dlsym(ibus_so,"ibus_text_new_from_string");
	p_ibus_text_new_from_static_string=dlsym(ibus_so,"ibus_text_new_from_static_string");
	p_ibus_text_get_length=dlsym(ibus_so,"ibus_text_get_length");
	p_ibus_text_append_attribute=dlsym(ibus_so,"ibus_text_append_attribute");
	
	p_ibus_component_new=dlsym(ibus_so,"ibus_component_new");
	p_ibus_component_add_engine=dlsym(ibus_so,"ibus_component_add_engine");

	p_ibus_factory_new=dlsym(ibus_so,"ibus_factory_new");
	p_ibus_factory_add_engine=dlsym(ibus_so,"ibus_factory_add_engine");

	p_ibus_engine_update_auxiliary_text=dlsym(ibus_so,"ibus_engine_update_auxiliary_text");
	p_ibus_engine_show_auxiliary_text=dlsym(ibus_so,"ibus_engine_show_auxiliary_text");
	p_ibus_engine_hide_auxiliary_text=dlsym(ibus_so,"ibus_engine_hide_auxiliary_text");
	p_ibus_engine_update_lookup_table=dlsym(ibus_so,"ibus_engine_update_lookup_table");
	p_ibus_engine_show_lookup_table=dlsym(ibus_so,"ibus_engine_show_lookup_table");
	p_ibus_engine_hide_lookup_table=dlsym(ibus_so,"ibus_engine_hide_lookup_table");

	p_ibus_lookup_table_new=dlsym(ibus_so,"ibus_lookup_table_new");
	p_ibus_lookup_table_append_candidate=dlsym(ibus_so,"ibus_lookup_table_append_candidate");
	p_ibus_lookup_table_set_orientation=dlsym(ibus_so,"ibus_lookup_table_set_orientation");
	
	return 0;
}

static void ibus_connected_cb(IBusBus *bus,gpointer user_data)
{
}

static void ibus_disconnected_cb (IBusBus *bus,gpointer user_data)
{
	exit(0);
}

static int xim_init(void)
{
	bus = p_ibus_bus_new();
	g_signal_connect (bus, "connected", G_CALLBACK (ibus_connected_cb), NULL);
	g_signal_connect (bus, "disconnected", G_CALLBACK (ibus_disconnected_cb), NULL);
	factory = p_ibus_factory_new (p_ibus_bus_get_connection(bus));
	if(!factory)
		return -1;
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

	if(0==p_ibus_bus_request_name (bus, "org.freedesktop.IBus.Yong", 0))
	{
		fprintf(stderr,"ibus request name fail\n");
	}
	return 0;
}

static char *ibus_get_icon_path(int index)
{
	gchar *cur=g_get_current_dir();
	gchar *path;
	char *icon;
#if L_WORD_SIZE==64
	icon=strrchr(cur,'/');
	if(icon)
		*icon=0;
#endif
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

int ybus_ibus_output_xml(void)
{
	IBusComponent *component;
	GString *output;
	
	if(xim_ibus_base_init())
		return -1;
	
	component=p_ibus_component_new("org.freedesktop.IBus.Yong",
                                    "Yong input method",
                                    "2.6.0",
                                    "",
                                    "dgod <dgod@gmail.com>",
                                    "http://yong.dgod.net",
                                    "/usr/bin/yong --ibus",
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
                                         "setup","/usr/bin/yong --config",
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
                                         "setup","/usr/bin/yong --config",
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
    //p_ibus_component_output_engines(component, output, 1);
	p_ibus_component_output(component, output, 1);
    fprintf (stdout, "%s", output->str);
    g_string_free (output, TRUE);
	g_object_unref(component);
	return 0;
}

void ybus_ibus_menu_enable(int enable)
{
	ibus_menu=enable;
}

void ybus_ibus_enable(int enable)
{
	ibus_enable=enable;
}

int ybus_ibus_use_ibus_menu(void)
{
	return ibus_menu;
}

int ybus_ibus_init(void)
{
	if(!ibus_enable)
		return -1;
	const char *temp=getenv("GTK_IM_MODULE");
	if(temp && !strcmp(temp,"yong"))
		ibus_assist=1;
	if(0!=xim_ibus_base_init())
	{
		fprintf(stderr,"ibus base init fail\n");
		return -1;
	}
	ybus_add_plugin(&plugin);
	return 0;
}

static IBusText *get_aux_text(const EXTRA_IM *eim)
{
	uint8_t temp[MAX_CAND_LEN+1+MAX_CODE_LEN+2];
	if(!im.StringGet[0] && !im.CodeInput[0])
		return NULL;
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
		strcat((char*)temp,"|");
	}
	return p_ibus_text_new_from_string((const gchar*)temp);
}

static IBusLookupTable *get_cand_list(const EXTRA_IM *eim,int line)
{
	int i;
	IBusLookupTable *t;
	if((!eim->CodeInput[0] && !eim->StringGet[0]) || eim->CandWordCount==0)
		return NULL;
	t=p_ibus_lookup_table_new(eim->CandWordCount,eim->SelectIndex,TRUE,FALSE);
	p_ibus_lookup_table_set_orientation(t,line==2?IBUS_ORIENTATION_VERTICAL:IBUS_ORIENTATION_HORIZONTAL);
	for(i=0;i<eim->CandWordCount;i++)
	{
		IBusText *text;
		if(im.Hint && im.CodeTips[i][0])
		{
			char temp[MAX_CAND_LEN*2+MAX_TIPS_LEN+1];
			int pos=sprintf(temp,"%s",im.CandTable[i]);
			strcpy(temp+pos,im.CodeTips[i]);
			pos=g_utf8_strlen(im.CandTable[i],-1);
			text=p_ibus_text_new_from_string(temp);
			int end=pos+g_utf8_strlen(im.CodeTips[i],-1);
			p_ibus_text_append_attribute(text,IBUS_ATTR_TYPE_FOREGROUND,0xff0084,pos,end);
		}
		else
		{ 
			text=p_ibus_text_new_from_string(im.CandTable[i]);
		}
		p_ibus_lookup_table_append_candidate(t,text);
	}
	return t;
}

int ybus_ibus_input_hide(void)
{
	YBUS_CONNECT *conn;
	if(!ibus_enable || !ibus_assist)
		return -1;
	if(0!=ybus_get_active(&conn,NULL))
		return -1;
	if(conn->plugin!=&plugin)
		return -1;
	IBusEngine *engine=(IBusEngine*)conn->id;
	p_ibus_engine_hide_auxiliary_text(engine);
	p_ibus_engine_hide_lookup_table(engine);

	return 0;
}

int ybus_ibus_input_draw(int line)
{
	YBUS_CONNECT *conn;
	if(!ibus_enable || !ibus_assist)
		return -1;
	if(0!=ybus_get_active(&conn,NULL))
		return -1;
	if(conn->plugin!=&plugin)
		return -1;
	IBusEngine *engine=(IBusEngine*)conn->id;
	
	EXTRA_IM *eim=CURRENT_EIM();
	if(!eim)
	{
		p_ibus_engine_hide_auxiliary_text(engine);
		p_ibus_engine_hide_lookup_table(engine);
		return 0;
	}
	IBusText *text;
	text=get_aux_text(eim);
	if(text)
		p_ibus_engine_update_auxiliary_text(engine,text,TRUE);
	IBusLookupTable *tab;
	tab=get_cand_list(eim,line);
	if(text && tab)
	{
		p_ibus_engine_update_lookup_table(engine,tab,TRUE);
	}
	else
	{
		p_ibus_engine_hide_lookup_table(engine);
	}
	return 0;
}

