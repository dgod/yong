// ybus-wayland.c - Wayland input method plugin for linim
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <glib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "llib.h"
#include "lcall.h"
#include "yong.h"
#include "ltricky.h"
#include "common.h"
#include "ybus.h"
#include "wui.h"

// --- dlopen'd wayland function pointers ---
// static struct wl_display *(*p_wl_display_connect)(const char *name);
// static void (*p_wl_display_disconnect)(struct wl_display *display);
// static int (*p_wl_display_flush)(struct wl_display *display);
// static int (*p_wl_display_dispatch)(struct wl_display *display);
// static int (*p_wl_display_get_fd)(struct wl_display *display);
// static int (*p_wl_display_get_error)(struct wl_display *display);
static int (*p_wl_display_roundtrip)(struct wl_display *display);
static int (*p_wl_proxy_add_listener)(struct wl_proxy *proxy,void (**implementation)(void), void *data);
static void *(*p_wl_proxy_get_user_data)(struct wl_proxy *proxy);
static void (*p_wl_proxy_set_user_data)(struct wl_proxy *proxy, void *user_data);
static uint32_t (*p_wl_proxy_get_version)(struct wl_proxy *proxy);
struct wl_proxy *(*p_wl_proxy_marshal_flags)(struct wl_proxy *proxy, uint32_t opcode,
		       const struct wl_interface *interface,
		       uint32_t version,
		       uint32_t flags, ...);
void (*p_wl_proxy_destroy)(struct wl_proxy *proxy);

static const struct wl_interface *p_wl_seat_interface;
static const struct wl_interface *p_wl_surface_interface;
static const struct wl_interface *p_wl_registry_interface;

// v1 only
static const struct wl_interface *p_wl_keyboard_interface;

static inline void *
p_wl_registry_bind(struct wl_registry *wl_registry, uint32_t name, const struct wl_interface *interface, uint32_t version)
{
	struct wl_proxy *id;

	id = p_wl_proxy_marshal_flags((struct wl_proxy *) wl_registry,
			 WL_REGISTRY_BIND, interface, version, 0, name, interface->name, version, NULL);

	return (void *) id;
}

static inline struct wl_registry *
p_wl_display_get_registry(struct wl_display *wl_display)
{
	struct wl_proxy *registry;

	registry = p_wl_proxy_marshal_flags((struct wl_proxy *) wl_display,
			 WL_DISPLAY_GET_REGISTRY, p_wl_registry_interface, p_wl_proxy_get_version((struct wl_proxy *) wl_display), 0, NULL);

	return (struct wl_registry *) registry;
}

static inline int
p_wl_keyboard_add_listener(struct wl_keyboard *wl_keyboard,
			 const struct wl_keyboard_listener *listener, void *data)
{
	return p_wl_proxy_add_listener((struct wl_proxy *) wl_keyboard,
				     (void (**)(void)) listener, data);
}

static bool l_wayland_debug=false;

#define CLIENT_ID_VAL			1

#define MOD_CONTROL_MASK 	KEYM_CTRL
#define MOD_ALT_MASK 		KEYM_ALT
#define MOD_SHIFT_MASK 		KEYM_SHIFT
#define MOD_SUPER_MASK		KEYM_SUPER
#define MOD_LOCK_MASK		KEYM_CAPS

struct simple_im{
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_seat *seat;

	struct zwp_input_method_v1 *input_method_v1;
	struct zwp_input_method_manager_v2 *input_method_manager_v2;
	union{
		struct{
			struct zwp_input_panel_v1 *input_panel_v1;
			struct zwp_input_method_context_v1 *context;
			struct wl_keyboard *keyboard;
		};
		struct{
			struct zwp_input_method_v2 *input_method_v2;
			struct zwp_input_method_keyboard_grab_v2 *keyboard_grab_v2;
		};
	};

	struct zwp_virtual_keyboard_manager_v1 *virtual_keyboard_manager_v1;
	union{
		struct{
			struct zwp_virtual_keyboard_v1 *virtual_keyboard_v1;
		};
	};

	struct xkb_context *xkb_context;

	uint32_t modifiers;

	int32_t repeat_rate;
	int32_t repeat_delay;
	uint32_t repeat_key;
	guint repeat_tmr;

	// v2 only keymap state
	struct{
		uint32_t format;
		int32_t fd;
		uint32_t size;
	}keymap_param;

	struct xkb_keymap *keymap;
	struct xkb_state *state;
	xkb_mod_mask_t control_mask;
	xkb_mod_mask_t alt_mask;
	xkb_mod_mask_t shift_mask;
	xkb_mod_mask_t super_mask;
	xkb_mod_mask_t lock_mask;

	struct{
		int depressed;
		int latched;
		int locked;
		int group;
	}modifiers_os;

	uint32_t serial;
	uint32_t time;

	int trigger;
	int enable;

	int last_press;
	uint32_t last_press_time;
	int bing;

	char app_active[128];
}simple_im;

#include "input-method-client-protocol-v1.h"
#include "input-method-protocol-v1.c"
#include "input-method-client-protocol-v2.h"
#include "virtual-keyboard-v1.h"
#include "input-method-protocol-v2.c"
#include "virtual-keyboard-v1.c"

// Forward declarations for functions used by protocol handlers
static YBUS_CONNECT *conn_app_set_active(const char *id);
static int input_method_keyboard_key_real(struct simple_im *keyboard, uint32_t key, uint32_t state);
static gboolean repeat_delay_func(struct simple_im *keyboard);
static void xim_send_key(CONN_ID conn_id,CLIENT_ID client_id,int key,int repeat);
int ybus_wayland_ui_init(W_INPUT_HOOK *input_hook);

static const char *xim_get_appid(CONN_ID conn_id);
static int xim_config(CONN_ID conn_id,CLIENT_ID client_id,const char *config,...);
static void xim_open_im(CONN_ID conn_id,CLIENT_ID client_id);
static void xim_close_im(CONN_ID conn_id,CLIENT_ID client_id);
static void xim_preedit_clear(CONN_ID conn_id,CLIENT_ID client_id);
static int xim_preedit_draw(CONN_ID conn_id,CLIENT_ID client_id,const char *s);
static void xim_send_string(CONN_ID conn_id,CLIENT_ID client_id,const char *s,int flags);
static void xim_send_key(CONN_ID conn_id,CLIENT_ID client_id,int key,int repeat);
static void xim_send_keys(CONN_ID conn_id,CLIENT_ID client_id,const int *key,int count);
static CONN_ID xim_copy_connect_id(CONN_ID id);
static void xim_free_connect_id(CONN_ID id);
static int xim_match_connect(CONN_ID a,CONN_ID b);
static int xim_init(void);

static YBUS_PLUGIN plugin={
	.name="wayland",
	.init=xim_init,
	.get_appid=xim_get_appid,
	.copy_connect_id=xim_copy_connect_id,
	.free_connect_id=xim_free_connect_id,
	.match_connect=xim_match_connect,
	.config=xim_config,
	.open_im=xim_open_im,
	.close_im=xim_close_im,
	.preedit_clear=xim_preedit_clear,
	.preedit_draw=xim_preedit_draw,
	.send_string=xim_send_string,
	.send_key=xim_send_key,
	.send_keys=xim_send_keys,
};

#include "im-protocol-v1.c"
#include "im-protocol-v2.c"

static bool wayland_init_done = false;

// --- YBUS_PLUGIN ---
static YBUS_CONNECT *conn_app_set_active(const char *id)
{
	l_strcpy(simple_im.app_active,sizeof(simple_im.app_active),id);
	YBUS_CONNECT *conn;
	conn=ybus_find_connect(&plugin,(CONN_ID)simple_im.app_active);
	if(!conn)
	{
		conn=ybus_add_connect(&plugin,(CONN_ID)simple_im.app_active);
		ybus_add_client(conn,CLIENT_ID_VAL,0);
	}
	return conn;
}

// --- key mapping ---

static int GetKey(struct simple_im *keyboard,int key,int state)
{
	uint32_t code;
	uint32_t num_syms;
	const xkb_keysym_t *syms;
	xkb_keysym_t sym;
	char text[64];
	int res=0;
	int mask=0;

	code = key + 8;
	num_syms = xkb_state_key_get_syms(keyboard->state, code, &syms);
	sym = XKB_KEY_NoSymbol;
	if (num_syms == 1)
		sym = syms[0];

	switch(sym){
	case XKB_KEY_BackSpace:
	case XKB_KEY_Tab:
	case XKB_KEY_Return:
	case XKB_KEY_Escape:
	case XKB_KEY_Delete:
	case XKB_KEY_Caps_Lock:
		res=sym&0xff;
		break;
	case XKB_KEY_Home:
	case XKB_KEY_Left:
	case XKB_KEY_Up:
	case XKB_KEY_Right:
	case XKB_KEY_Down:
	case XKB_KEY_Page_Up:
	case XKB_KEY_Page_Down:
	case XKB_KEY_End:
	case XKB_KEY_Insert:
		res=sym;
		break;
	case XKB_KEY_Shift_L:
	case XKB_KEY_Shift_R:
	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R:
	case XKB_KEY_Alt_L:
	case XKB_KEY_Alt_R:
		res=sym&0xff;
		break;
	case XKB_KEY_F1 ... XKB_KEY_F12:
		res=sym;
		break;
	case XKB_KEY_KP_Space:
		res=KEYM_KEYPAD|YK_SPACE;
		break;
	case XKB_KEY_KP_Enter:
		res=KEYM_KEYPAD|YK_ENTER;
		break;
	case XKB_KEY_KP_Tab:
		res=KEYM_KEYPAD|YK_TAB;
		break;
	case XKB_KEY_KP_Subtract:
		res=KEYM_KEYPAD|'-';
		break;
	case XKB_KEY_KP_Add:
		res=KEYM_KEYPAD|'+';
		break;
	case XKB_KEY_KP_Multiply:
		res=KEYM_KEYPAD|'*';
		break;
	case XKB_KEY_KP_Divide:
		res=KEYM_KEYPAD|'/';
		break;
	case XKB_KEY_KP_Decimal:
		res=KEYM_KEYPAD|'.';
		break;
	case XKB_KEY_KP_Equal:
		res=KEYM_KEYPAD|'=';
		break;
	case XKB_KEY_KP_0 ... XKB_KEY_KP_9:
		res=KEYM_KEYPAD|(sym-XKB_KEY_KP_0+'0');
		break;
	case XKB_KEY_ISO_Left_Tab:
		res=KEYM_SHIFT|YK_TAB;
		break;
	default:
		if (xkb_keysym_to_utf8(sym, text, sizeof(text)) <= 0)
			return res;
		if(strlen(text)>1)
			return res;
		res=text[0];
		break;
	}

	if((keyboard->modifiers&KEYM_CTRL) && res!=YK_LCTRL && res!=YK_RCTRL)
		mask|=KEYM_CTRL;
	if((keyboard->modifiers&KEYM_SHIFT) && res!=YK_LSHIFT && res!=YK_RSHIFT)
		mask|=KEYM_SHIFT;
	if((keyboard->modifiers&KEYM_ALT) && res!=YK_LALT && res!=YK_RALT)
		mask|=KEYM_ALT;
	if((keyboard->modifiers&KEYM_SUPER))
		mask|=KEYM_SUPER;
	if((keyboard->modifiers&KEYM_CAPS) && res!=YK_CAPSLOCK)
		mask|=KEYM_CAPS;

	if(mask)
		res=mask|toupper(res);

	return res;
}

static int GetKey_r_v1(struct simple_im *keyboard,int yk)
{
	int vk;

	yk&=~KEYM_MASK;

	switch(yk){
	case YK_BACKSPACE:vk=XKB_KEY_BackSpace;break;
	case YK_DELETE:vk=XKB_KEY_Delete;break;
	case YK_ENTER:vk=XKB_KEY_Return;break;
	case YK_HOME:vk=XKB_KEY_Home;break;
	case YK_END:vk=XKB_KEY_End;break;
	case YK_PGUP:vk=XKB_KEY_Page_Up;break;
	case YK_PGDN:vk=XKB_KEY_Page_Down;break;
	case YK_LEFT:vk=XKB_KEY_Left;break;
	case YK_DOWN:vk=XKB_KEY_Down;break;
	case YK_UP:vk=XKB_KEY_Up;break;
	case YK_RIGHT:vk=XKB_KEY_Right;break;
	case YK_TAB:vk=XKB_KEY_Tab;break;
	case YK_LCTRL:vk=XKB_KEY_Control_L;break;
	case YK_LSHIFT:vk=XKB_KEY_Shift_L;break;
	case YK_LALT:vk=XKB_KEY_Alt_L;break;
	case YK_LWIN:vk=XKB_KEY_Super_L;break;
	default:vk=yk;
	}

	return vk;
}

static int GetKey_r(struct simple_im *keyboard,int yk)
{
	int vk;

	yk&=~KEYM_MASK;

	switch(yk){
	case YK_BACKSPACE:vk=XKB_KEY_BackSpace;break;
	case YK_DELETE:vk=XKB_KEY_Delete;break;
	case YK_ENTER:vk=XKB_KEY_Return;break;
	case YK_HOME:vk=XKB_KEY_Home;break;
	case YK_END:vk=XKB_KEY_End;break;
	case YK_PGUP:vk=XKB_KEY_Page_Up;break;
	case YK_PGDN:vk=XKB_KEY_Page_Down;break;
	case YK_LEFT:vk=XKB_KEY_Left;break;
	case YK_DOWN:vk=XKB_KEY_Down;break;
	case YK_UP:vk=XKB_KEY_Up;break;
	case YK_RIGHT:vk=XKB_KEY_Right;break;
	case YK_TAB:vk=XKB_KEY_Tab;break;
	case YK_LCTRL:vk=XKB_KEY_Control_L;break;
	case YK_LSHIFT:vk=XKB_KEY_Shift_L;break;
	case YK_LALT:vk=XKB_KEY_Alt_L;break;
	case YK_LWIN:vk=XKB_KEY_Super_L;break;
	case YK_BACK:vk=XKB_KEY_XF86Back;break;
	default:vk=yk;
	}

	if(vk>='A' && vk<='Z')
	{
		vk+='a'-'A';
	}

	xkb_keycode_t min = xkb_keymap_min_keycode(keyboard->keymap);
    xkb_keycode_t max = xkb_keymap_max_keycode(keyboard->keymap);
	xkb_keycode_t code;

	for(code=min;code<max;code++)
	{
		const xkb_keysym_t *syms;
		xkb_keysym_t sym;
		if(xkb_state_key_get_syms(keyboard->state,code,&syms)<=0)
			continue;
		for(int j=0;syms[j]!=XKB_KEY_NoSymbol;j++)
		{
			sym=syms[j];
			if(sym==vk)
				return code-8;
		}
	}
	return -1;
}

// --- send helpers ---

static void send_string(struct simple_im *keyboard,const char *s)
{
	if(keyboard->input_method_manager_v2)
	{
		if(l_wayland_debug)
			fprintf(stderr,"commit_string v2 %s\n",s);
		zwp_input_method_v2_commit_string(keyboard->input_method_v2,s);
		zwp_input_method_v2_commit(keyboard->input_method_v2,keyboard->serial);
	}
	else if(keyboard->input_method_v1)
	{
		if(l_wayland_debug)
			fprintf(stderr,"commit_string v1 %s\n",s);
		zwp_input_method_context_v1_commit_string(keyboard->context,keyboard->serial,s);
	}
}

static void send_mods(struct simple_im *keyboard,int key)
{
	if(key&KEYM_CTRL)
		keyboard->modifiers_os.depressed|=keyboard->control_mask;
	else
		keyboard->modifiers_os.depressed&=~keyboard->control_mask;
	if(key&KEYM_SHIFT)
		keyboard->modifiers_os.depressed|=keyboard->shift_mask;
	else
		keyboard->modifiers_os.depressed&=~keyboard->shift_mask;
	if(key&KEYM_ALT)
		keyboard->modifiers_os.depressed|=keyboard->alt_mask;
	else
		keyboard->modifiers_os.depressed&=~keyboard->alt_mask;
	if(key&KEYM_WIN)
		keyboard->modifiers_os.depressed|=keyboard->super_mask;
	else
		keyboard->modifiers_os.depressed&=~keyboard->super_mask;
	if(keyboard->input_method_manager_v2 && keyboard->virtual_keyboard_v1)
	{
		zwp_virtual_keyboard_v1_modifiers(
						keyboard->virtual_keyboard_v1,
						keyboard->modifiers_os.depressed,
						keyboard->modifiers_os.latched,
						keyboard->modifiers_os.locked,
						keyboard->modifiers_os.group);
	}
	else
	{
		zwp_input_method_context_v1_modifiers(
						keyboard->context,
						keyboard->serial,
						keyboard->modifiers_os.depressed,
						keyboard->modifiers_os.latched,
						keyboard->modifiers_os.locked,
						keyboard->modifiers_os.group);
	}
}

static void send_key(struct simple_im *keyboard,int key,int type,int repeat)
{
	if(!keyboard->state)
		return;
	int sym;
	if(keyboard->input_method_manager_v2)
		sym=GetKey_r(keyboard,key);
	else
		sym=GetKey_r_v1(keyboard,key);
	if(!sym)
		return;
	if(keyboard->input_method_manager_v2 && keyboard->virtual_keyboard_v1)
	{
		if(type==0 || type==1)
		{
			if((KEYM_MASK&key)!=0)
				send_mods(keyboard,key);
			for(int i=0;i<repeat;i++)
				zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,sym,WL_KEYBOARD_KEY_STATE_PRESSED);
		}
		if(type==0 || type==2)
		{
			zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,sym,WL_KEYBOARD_KEY_STATE_RELEASED);
			if((KEYM_MASK&key)!=0 && keyboard->modifiers_os.depressed)
				send_mods(keyboard,0);
		}
	}
	else if(keyboard->input_method_v1)
	{
		int mask=0;
		if(key&KEYM_CTRL)
			mask|=keyboard->control_mask;
		if(key&KEYM_SHIFT)
			mask|=keyboard->shift_mask;
		if(key&KEYM_ALT)
			mask|=keyboard->alt_mask;
		if(type==0 || type==1)
		{
			for(int i=0;i<repeat;i++)
				zwp_input_method_context_v1_keysym(keyboard->context,keyboard->serial,0,sym,WL_KEYBOARD_KEY_STATE_PRESSED,mask);
		}
		if(type==0 || type==2)
		{
			zwp_input_method_context_v1_keysym(keyboard->context,keyboard->serial,0,sym,WL_KEYBOARD_KEY_STATE_RELEASED,mask);
		}
	}
}

static int preedit_draw(struct simple_im *keyboard,const char *s)
{
	char out[512];
	char *p;
	int cursor;
	l_strcpy(out,sizeof(out),s);
	if((p=strchr(out,'|'))!=NULL)
	{
		cursor=g_utf8_pointer_to_offset(out,p);
		memcpy(p,p+1,strlen(p+1)+1);
	}
	else
	{
		cursor=strlen(out);
	}
	if(keyboard->input_method_manager_v2)
	{
		zwp_input_method_v2_set_preedit_string(keyboard->input_method_v2,out,cursor,cursor);
		zwp_input_method_v2_commit(keyboard->input_method_v2,keyboard->serial);
	}
	else if(keyboard->input_method_v1)
	{
		zwp_input_method_context_v1_preedit_cursor(keyboard->context,cursor);
		zwp_input_method_context_v1_preedit_string(keyboard->context,keyboard->serial,out,out);
	}
	return 0;
}

static void preedit_clear(struct simple_im *keyboard)
{
	preedit_draw(keyboard,"");
}

// --- shared key processing ---
// Returns TRUE if the key was consumed (handled by input engine)

static int input_method_keyboard_key_real(
		struct simple_im *keyboard,
		uint32_t key,
		uint32_t state)
{
	int res=FALSE;
	int yk=GetKey(keyboard,key,state);
	if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
	{
		if(im.Bing && ((yk>='a' && yk<='z') || yk==' '))
		{
			int diff=y_im_diff_hand(keyboard->bing,yk);
			if(!keyboard->bing)
			{
				if(yk!=' ')
					keyboard->bing=yk;
			}
			else if(keyboard->time-keyboard->last_press_time>=im.BingSkip[diff])
			{
				if(yk==' ') yk=keyboard->bing;
				yk|=KEYM_BING;
			}
		}
		keyboard->last_press=yk;
		keyboard->last_press_time=keyboard->time;
	}
	if(state==WL_KEYBOARD_KEY_STATE_RELEASED)
	{
		keyboard->bing=0;
		yk|=KEYM_UP;
	}
	if(YK_CODE(yk)>=YK_LSHIFT && YK_CODE(yk)<=YK_RWIN)
	{
		keyboard->bing=0;
		if(keyboard->virtual_keyboard_v1)
			zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,keyboard->time,key,state);
		if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
			return TRUE;
		yk&=~KEYM_UP;
		if(yk!=keyboard->last_press || keyboard->time-keyboard->last_press_time>300)
			return TRUE;
	}
	if(state==WL_KEYBOARD_KEY_STATE_RELEASED && ((yk&~KEYM_UP)==keyboard->last_press))
	{
		keyboard->last_press=0;
	}	
	if(!keyboard->enable)
	{
		if(yk==keyboard->trigger && state)
		{
			ybus_on_open(&plugin,(CONN_ID)keyboard->app_active,CLIENT_ID_VAL);
			keyboard->enable=TRUE;
			res=TRUE;
		}
	}
	else
	{
		if(im.layout && !im.Bing)
		{
			int tmp=yk&~KEYM_KEYPAD;
			if(!(tmp&KEYM_MASK))
			{
				tmp=YK_CODE(yk);
				if(state==WL_KEYBOARD_KEY_STATE_RELEASED)
				{
					tmp=y_layout_keyup(im.layout,tmp,keyboard->time);
				}
				else
				{
					tmp=y_layout_keydown(im.layout,tmp,keyboard->time);
				}
				if(tmp>0)
				{
					char *p=(char*)&tmp;
					YBUS_CONNECT *yconn;
					YBUS_CLIENT *client;
					ybus_get_active(&yconn,&client);
					if(yconn->lang==LANG_CN)
					{
						for(int i=0;i<4 && p[i];i++)
						{
							int ret=ybus_on_key(&plugin,yconn->id,client->id,p[i]);
							if(ret)
							{
								y_im_speed_update(p[i],0);
							}
							else
							{
								xim_send_key(yconn->id,client->id,p[i],1);
							}
						}
					}
					else
					{
						for(int i=0;i<4 && p[i];i++)
						{
							xim_send_key(yconn->id,client->id,p[i],1);
						}
					}
					return TRUE;
				}
				else if(tmp==0)
				{
					return TRUE;
				}
			}
		}
		res=ybus_on_key(&plugin,(CONN_ID)keyboard->app_active,CLIENT_ID_VAL,yk);
	}
	if(!res && keyboard->virtual_keyboard_v1)
	{
		zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,keyboard->time,key,state);
		return TRUE;
	}
	return res;
}

static gboolean repeat_rate_func(struct simple_im *keyboard)
{
	keyboard->time+=1000/keyboard->repeat_rate;
	input_method_keyboard_key_real(keyboard,keyboard->repeat_key,WL_KEYBOARD_KEY_STATE_PRESSED);
	return TRUE;
}

static gboolean repeat_delay_func(struct simple_im *keyboard)
{
	keyboard->repeat_tmr=g_timeout_add(1000/keyboard->repeat_rate,(GSourceFunc)repeat_rate_func,keyboard);
	keyboard->time+=keyboard->repeat_delay;
	input_method_keyboard_key_real(keyboard,keyboard->repeat_key,WL_KEYBOARD_KEY_STATE_PRESSED);
	return FALSE;
}

// --- YBUS_PLUGIN callbacks ---

static const char *xim_get_appid(CONN_ID conn_id)
{
	return (const char*)conn_id;
}

static int xim_config(CONN_ID conn_id,CLIENT_ID client_id,const char *config,...)
{
	struct simple_im *keyboard=&simple_im;
	va_list ap;

	va_start(ap,config);
	if(!strcmp(config,"trigger"))
	{
		keyboard->trigger=va_arg(ap,int);
	}
	va_end(ap);

	return 0;
}

static void xim_open_im(CONN_ID conn_id,CLIENT_ID client_id)
{
	simple_im.enable=1;
	ybus_on_open(&plugin,conn_id,client_id);
}

static void xim_close_im(CONN_ID conn_id,CLIENT_ID client_id)
{
	simple_im.enable=0;
	ybus_on_close(&plugin,conn_id,client_id);
}

static void xim_preedit_clear(CONN_ID conn_id,CLIENT_ID client_id)
{
	preedit_clear(&simple_im);
}

static int xim_preedit_draw(CONN_ID conn_id,CLIENT_ID client_id,const char *s)
{
	return preedit_draw(&simple_im,s);
}

static void xim_send_string(CONN_ID conn_id,CLIENT_ID client_id,const char *s,int flags)
{
	char out[512];
	y_im_str_encode(s,out,flags);
	return send_string(&simple_im,out);
}

static void xim_send_key(CONN_ID conn_id,CLIENT_ID client_id,int key,int repeat)
{
	send_key(&simple_im,key,0,repeat);
}

static void send_keys_coroutine(LArray *arr)
{
	struct simple_im *keyboard=&simple_im;
	int *keys=(int*)arr->data;
	int count=arr->len;
	struct{
		bool iskey;
		union{
			int key;
			char str[8];
		};
	}prev;
	for(int i=0;i<count;i++)
	{
		int key=keys[i];
		int mask=KEYM_MASK&key;
		if(mask==KEYM_UP)
		{
			l_co_sleep(YK_CODE(key));
			continue;
		}
		if(mask==KEYM_BING)
		{
			int repeat=YK_CODE(key);
			for(int j=0;j<repeat;j++)
			{
				if(prev.iskey)
					send_key(keyboard,prev.key,0,1);
				else
					send_string(keyboard,prev.str);
			}
		}
		if((mask&KEYM_CAPS)!=0)
		{
			// don't support mouse event
			break;
		}
		if((mask&KEYM_VIRT)!=0)
		{
			int code=key&~KEYM_VIRT;
			int len=l_unichar_to_utf8(code,(uint8_t*)prev.str);
			prev.str[len]=0;
			prev.iskey=false;
			send_string(keyboard,prev.str);
		}
		else
		{
			int up=mask&KEYM_UP;
			mask&=~KEYM_UP;
			if(mask==KEYM_CTRL)
				key=YK_LCTRL;
			else if(mask==KEYM_SHIFT)
				key=YK_LSHIFT;
			else if(mask==KEYM_ALT)
				key=YK_LALT;
			else if(mask==KEYM_WIN)
				key=YK_LWIN;
			else
				key=YK_CODE(key);
			prev.iskey=true;
			prev.key=key;
			if(!mask)
			{
				send_key(keyboard,prev.key,0,1);
			}
			else
			{
				if(!up)
				{
					send_mods(keyboard,mask);
					send_key(keyboard,key,1,1);
				}
				else
				{
					send_key(keyboard,key,2,1);
					send_mods(keyboard,0);
				}
			}
		}
	}
	l_array_free(arr,NULL);
}

static void xim_send_keys(CONN_ID conn_id,CLIENT_ID client_id,const int *keys,int count)
{
	LArray *arr=l_array_new(count,sizeof(int));
	memcpy(arr->data,keys,count*sizeof(int));
	arr->len=count;
	l_co_create((void*)send_keys_coroutine,arr);
	l_co_sched();
}

static CONN_ID xim_copy_connect_id(CONN_ID id)
{
	return (CONN_ID)l_strdup((char*)id);
}

static void xim_free_connect_id(CONN_ID id)
{
	l_free((void*)id);
}

static int xim_match_connect(CONN_ID a,CONN_ID b)
{
	return !strcmp((char*)a,(char*)b);
}

static int xim_init(void)
{
	return 0;
}

// --- W_INPUT_HOOK callbacks for InputWin surface association ---

static void *input_hook_set_role(struct wl_surface *surface)
{
	struct simple_im *keyboard = &simple_im;
	if(keyboard->input_method_manager_v2 && keyboard->input_method_v2)
	{
		if(l_wayland_debug)
			fprintf(stderr,"input_hook: set_role v2 (input_popup_surface)\n");
		return zwp_input_method_v2_get_input_popup_surface(keyboard->input_method_v2, surface);
	}
	else if(keyboard->input_method_v1 && keyboard->input_panel_v1)
	{
		if(l_wayland_debug)
			fprintf(stderr,"input_hook: set_role v1 (input_panel_surface, overlay_panel)\n");
		struct zwp_input_panel_surface_v1 *panel_surface =
			zwp_input_panel_v1_get_input_panel_surface(keyboard->input_panel_v1, surface);
		zwp_input_panel_surface_v1_set_overlay_panel(panel_surface);
		return panel_surface;
	}
	return NULL;
}

static void input_hook_clr_role(void *popup)
{
	if(!popup)
		return;
	struct simple_im *keyboard = &simple_im;
	if(keyboard->input_method_manager_v2)
	{
		if(l_wayland_debug)
			fprintf(stderr,"input_hook: clr_role v2\n");
		zwp_input_popup_surface_v2_destroy((struct zwp_input_popup_surface_v2 *)popup);
	}
	else if(keyboard->input_method_v1)
	{
		if(l_wayland_debug)
			fprintf(stderr,"input_hook: clr_role v1\n");
		zwp_input_panel_surface_v1_destroy((struct zwp_input_panel_surface_v1 *)popup);
	}
}

// --- Wayland connection and protocol detection ---

int ybus_wayland_ui_init(W_INPUT_HOOK *input_hook)
{
	if(wayland_init_done)
		return 0;

	void *l_so=dlopen("libwayland-client.so.0",RTLD_LAZY);
	if(!l_so)
	{
		if(l_wayland_debug)
			fprintf(stderr,"wayland library not found\n");
		return -1;
	}

	if(getenv("YONG_WAYLAND_DEBUG"))
		l_wayland_debug=true;

	// p_wl_display_connect=dlsym(l_so,"wl_display_connect");
	// p_wl_display_disconnect=dlsym(l_so,"wl_display_disconnect");
	// p_wl_display_flush=dlsym(l_so,"wl_display_flush");
	// p_wl_display_dispatch=dlsym(l_so,"wl_display_dispatch");
	// p_wl_display_get_fd=dlsym(l_so,"wl_display_get_fd");
	// p_wl_display_get_error=dlsym(l_so,"wl_display_get_error");
	p_wl_display_roundtrip=dlsym(l_so,"wl_display_roundtrip");
	p_wl_proxy_add_listener=dlsym(l_so,"wl_proxy_add_listener");
	p_wl_proxy_get_user_data=dlsym(l_so,"wl_proxy_get_user_data");
	p_wl_proxy_set_user_data=dlsym(l_so,"wl_proxy_set_user_data");
	p_wl_proxy_get_version=dlsym(l_so,"wl_proxy_get_version");
	p_wl_proxy_marshal_flags=dlsym(l_so,"wl_proxy_marshal_flags");
	p_wl_proxy_destroy=dlsym(l_so,"wl_proxy_destroy");

	p_wl_seat_interface=dlsym(l_so,"wl_seat_interface");
	p_wl_surface_interface=dlsym(l_so,"wl_surface_interface");
	p_wl_registry_interface=dlsym(l_so,"wl_registry_interface");

	input_method_unstable_v2_types[6]=p_wl_surface_interface;
	input_method_unstable_v2_types[8]=p_wl_seat_interface;
	virtual_keyboard_unstable_v1_types[4]=p_wl_seat_interface;

	p_wl_keyboard_interface=dlsym(l_so,"wl_keyboard_interface");
	input_method_unstable_v1_types[5]=p_wl_keyboard_interface;
	input_method_unstable_v1_types[9]=p_wl_surface_interface;
	input_method_unstable_v1_types[10]=NULL;//p_wl_output_interface;

	// Get wl_display from wui
	struct wl_display *display=(struct wl_display *)wui->wayland_get_display();
	if(!display)
	{
		if(l_wayland_debug)
			fprintf(stderr,"no wayland display from wui\n");
		dlclose(l_so);
		return -1;
	}

	struct simple_im *keyboard=&simple_im;
	keyboard->display=display;
	keyboard->registry=p_wl_display_get_registry(keyboard->display);

	// Check what protocols are available via wui
	uint32_t im_name;
	bool has_v2 = wui->wayland_has_interface("zwp_input_method_manager_v2", 1, &im_name);
	bool has_v1 = wui->wayland_has_interface("zwp_input_method_v1", 1, NULL);

	if(has_v2)
	{
		keyboard->input_method_manager_v2 = wui->wayland_get_interface("zwp_input_method_manager_v2");
		if(!keyboard->input_method_manager_v2)
		{
			// Need to bind it ourselves
			uint32_t name;
			wui->wayland_has_interface("zwp_input_method_manager_v2", 1, &name);
			keyboard->input_method_manager_v2 = p_wl_registry_bind(keyboard->registry,
				name, &zwp_input_method_manager_v2_interface, 1);
		}
	}
	if(has_v1)
	{
		keyboard->input_method_v1 = wui->wayland_get_interface("zwp_input_method_v1");
		if(!keyboard->input_method_v1)
		{
			uint32_t name;
			wui->wayland_has_interface("zwp_input_method_v1", 1, &name);
			keyboard->input_method_v1 = p_wl_registry_bind(keyboard->registry,
				name, &zwp_input_method_v1_interface, 1);
		}
	}

	// Bind input_panel_v1 for v1
	if(has_v1 && !has_v2)
	{
		if(wui->wayland_has_interface("zwp_input_panel_v1", 1, NULL))
		{
			uint32_t name;
			wui->wayland_has_interface("zwp_input_panel_v1", 1, &name);
			keyboard->input_panel_v1 = p_wl_registry_bind(keyboard->registry,
				name, &zwp_input_panel_v1_interface, 1);
		}
	}

	// Bind virtual_keyboard_manager_v1
	if(wui->wayland_has_interface("zwp_virtual_keyboard_manager_v1", 1, NULL))
	{
		uint32_t name;
		wui->wayland_has_interface("zwp_virtual_keyboard_manager_v1", 1, &name);
		keyboard->virtual_keyboard_manager_v1 = p_wl_registry_bind(keyboard->registry,
			name, &zwp_virtual_keyboard_manager_v1_interface, 1);
	}

	// Get wl_seat from registry
	if(wui->wayland_has_interface("wl_seat", 1, NULL))
	{
		uint32_t name;
		wui->wayland_has_interface("wl_seat", 1, &name);
		keyboard->seat = p_wl_registry_bind(keyboard->registry, name, p_wl_seat_interface, 1);
	}

	p_wl_display_roundtrip(keyboard->display);

	if(keyboard->input_method_manager_v2 && keyboard->seat)
	{
		keyboard->input_method_v2=zwp_input_method_manager_v2_get_input_method(keyboard->input_method_manager_v2,keyboard->seat);
		if(l_wayland_debug)
			fprintf(stderr,"get input method v2 %p\n",keyboard->input_method_v2);
		zwp_input_method_v2_add_listener(keyboard->input_method_v2,
					  &input_method_listener_v2, keyboard);
	}
	else if(keyboard->input_method_v1)
	{
		zwp_input_method_v1_add_listener(keyboard->input_method_v1,
					  &input_method_listener_v1, keyboard);
		if(l_wayland_debug)
			fprintf(stderr,"get input method v1 %p\n",keyboard->input_method_v1);
	}

	keyboard->xkb_context = xkb_context_new(0);
	keyboard->trigger = CTRL_SPACE;

	input_hook->set_role=input_hook_set_role;
	input_hook->clr_role=input_hook_clr_role;

	wayland_init_done = true;
	return 0;
}

int ybus_wayland_init(void)
{
	ybus_add_plugin(&plugin);
	return 0;
}
