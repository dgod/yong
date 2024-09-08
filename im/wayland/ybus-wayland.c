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
#include "xim.h"
#include "ui.h"
#include "ybus.h"

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell/gtk-layer-shell.h>

static struct wl_display *(*p_wl_display_connect)(const char *name);
static void (*p_wl_display_disconnect)(struct wl_display *display);
static int (*p_wl_display_flush)(struct wl_display *display);
static int (*p_wl_display_dispatch)(struct wl_display *display);
static int (*p_wl_display_get_fd)(struct wl_display *display);
static int (*p_wl_display_get_error)(struct wl_display *display);
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
static const struct wl_interface *p_wl_output_interface;

static struct wl_display *(*p_gdk_wayland_display_get_wl_display)(GdkDisplay *display);
#if GTK_CHECK_VERSION(4,0,0)
static struct wl_surface *(*p_gdk_wayland_surface_get_wl_surface)(GdkSurface *surface);
#else
static void (*p_gdk_wayland_window_set_use_custom_surface)(GdkWindow *window);
static struct wl_surface *(*p_gdk_wayland_window_get_wl_surface)(GdkWindow *window);
#endif

#ifndef WAYLAND_STANDALONE
static void (*p_gtk_layer_init_for_window) (GtkWindow *window);
static void (*p_gtk_layer_set_layer) (GtkWindow *window, GtkLayerShellLayer layer);
static void (*p_gtk_layer_set_margin) (GtkWindow *window, GtkLayerShellEdge edge, int margin_size);
static void (*p_gtk_layer_set_keyboard_mode) (GtkWindow *window, GtkLayerShellKeyboardMode mode);
static void (*p_gtk_layer_set_anchor) (GtkWindow *window, GtkLayerShellEdge edge, gboolean anchor_to_edge);
#endif

static inline int p_wl_registry_add_listener(struct wl_registry *wl_registry,
                         const struct wl_registry_listener *listener, void *data)
{
        return p_wl_proxy_add_listener((struct wl_proxy *) wl_registry,
                                     (void (**)(void)) listener, data);
}

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

static void noop(void)
{
}

#include "input-method-client-protocol-v1.h"
#include "input-method-protocol-v1.c"
#include "input-method-client-protocol-v2.h"
#include "virtual-keyboard-v1.h"
#include "input-method-protocol-v2.c"
#include "virtual-keyboard-v1.c"
#include "libwayland-glib-source.c"

static GWaylandSource *l_source;

#define CLIENT_ID_VAL			1

#define MOD_CONTROL_MASK 	KEYM_CTRL
#define MOD_ALT_MASK 		KEYM_ALT
#define MOD_SHIFT_MASK 		KEYM_SHIFT
#define MOD_SUPER_MASK		KEYM_SUPER
#define MOD_LOCK_MASK		KEYM_CAPS

#ifdef WAYLAND_STANDALONE
static gboolean client_input_key(guint id,int key,guint32 time);
static void client_focus_in(guint id);
static void client_focus_out(guint id);
static void client_enable(guint id);

#else
#include "common.h"

static int xim_getpid(CONN_ID conn_id);
static int xim_config(CONN_ID conn_id,CLIENT_ID client_id,const char *config,...);
static void xim_open_im(CONN_ID conn_id,CLIENT_ID client_id);
static void xim_close_im(CONN_ID conn_id,CLIENT_ID client_id);
static void xim_preedit_clear(CONN_ID conn_id,CLIENT_ID client_id);
static int xim_preedit_draw(CONN_ID conn_id,CLIENT_ID client_id,const char *s);
static void xim_send_string(CONN_ID conn_id,CLIENT_ID client_id,const char *s,int flags);
static void xim_send_key(CONN_ID conn_id,CLIENT_ID client_id,int key);
static int xim_init(void);

static YBUS_PLUGIN plugin={
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

#endif

enum{
	IM_WIN_NONE,
	IM_WIN_INPUT,
	IM_WIN_LAYER,
	IM_WIN_CUSTOM,
};

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

	bool layer_shell_v1;

	struct xkb_context *xkb_context;
	
	uint32_t modifiers;

	int32_t repeat_rate;
	int32_t repeat_delay;
	uint32_t repeat_key;
	guint repeat_tmr;
	
	struct xkb_keymap *keymap;
	struct xkb_state *state;
	xkb_mod_mask_t control_mask;
	xkb_mod_mask_t alt_mask;
	xkb_mod_mask_t shift_mask;
	xkb_mod_mask_t super_mask;
	xkb_mod_mask_t lock_mask;
	
	uint32_t serial;
	uint32_t time;

	int trigger;
	int enable;
	int preedit_string;
	int onspot;

	int last_press;
	uint32_t last_press_time;
	int bing;

#ifndef WAYLAND_STANDALONE
	struct{
		GtkWidget *input;
		GtkWidget *main;
		GtkWidget *tip;
		GtkWidget *keyboard;
	}wins;
#endif
}simple_im;

static void input_method_keyboard_keymap(
		struct simple_im *keyboard,
		struct zwp_input_method_keyboard_grab_v2 *zwp_input_method_keyboard_grab_v2,
		uint32_t format,
		int32_t fd,
		uint32_t size)
{
	//printf("input_method_keyboard_keymap %u %d %u\n",format,fd,size);
	if(keyboard->virtual_keyboard_v1)
	{
		zwp_virtual_keyboard_v1_keymap(keyboard->virtual_keyboard_v1, format, fd, size);
	}
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
	{
		close(fd);
		printf("format not support\n");
		return;
	}

	char *map_str=mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_str == MAP_FAILED)
	{
		close(fd);
		printf("mmap fail\n");
		return;
	}

	if(keyboard->keymap)
	{
		xkb_keymap_unref(keyboard->keymap);
	}

	keyboard->keymap =
		xkb_keymap_new_from_string(keyboard->xkb_context,
					map_str,
					XKB_KEYMAP_FORMAT_TEXT_V1,
					0);
	munmap(map_str, size);
	close(fd);

	keyboard->state = xkb_state_new(keyboard->keymap);
	if (!keyboard->state)
	{
		fprintf(stderr, "failed to create XKB state\n");
		xkb_keymap_unref(keyboard->keymap);
		return;
	}

	keyboard->control_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Control");
	keyboard->alt_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Mod1");
	keyboard->shift_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Shift");
	keyboard->super_mask = 
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Mod4");
	keyboard->lock_mask = 
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Lock");
}

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
	if((keyboard->modifiers&KEYM_CAPS))
		mask|=KEYM_CAPS;
	
	if(mask)
		res=mask|toupper(res);

	return res;
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
	default:vk=yk;
	}

	xkb_keycode_t min = xkb_keymap_min_keycode(keyboard->keymap);
    xkb_keycode_t max = xkb_keymap_max_keycode(keyboard->keymap);
	xkb_keycode_t code;
	for(code=min;code<max;code++)
	{
		if (xkb_state_key_get_one_sym(keyboard->state, code) ==vk)
			return code-8;
	}
	return 0;
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
	default:vk=yk;
	}

	return vk;
}

static void send_string(struct simple_im *keyboard,const char *s)
{
	if(keyboard->input_method_manager_v2)
	{
		zwp_input_method_v2_commit_string(keyboard->input_method_v2,s);
		zwp_input_method_v2_commit(keyboard->input_method_v2,keyboard->serial);
	}
	else if(keyboard->input_method_v1)
	{
		zwp_input_method_context_v1_commit_string(keyboard->context,keyboard->serial,s);
	}
}

static void send_key(struct simple_im *keyboard,int key)
{
	int sym;
	if(keyboard->input_method_manager_v2)
		sym=GetKey_r(keyboard,key);
	else
		sym=GetKey_r_v1(keyboard,key);
	if(!sym)
		return;
	// fprintf(stderr,"send key %x %x\n",key,sym);
	if(keyboard->input_method_manager_v2 && keyboard->virtual_keyboard_v1)
	{
		if(key&KEYM_CTRL)
			zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,XKB_KEY_Control_L,WL_KEYBOARD_KEY_STATE_PRESSED);
		if(key&KEYM_SHIFT)
			zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,XKB_KEY_Shift_L,WL_KEYBOARD_KEY_STATE_PRESSED);
		if(key&KEYM_ALT)
			zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,XKB_KEY_Alt_L,WL_KEYBOARD_KEY_STATE_PRESSED);
		zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,sym,WL_KEYBOARD_KEY_STATE_PRESSED);
		zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,sym,WL_KEYBOARD_KEY_STATE_RELEASED);
		if(key&KEYM_CTRL)
			zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,XKB_KEY_Control_L,WL_KEYBOARD_KEY_STATE_RELEASED);
		if(key&KEYM_SHIFT)
			zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,XKB_KEY_Shift_L,WL_KEYBOARD_KEY_STATE_RELEASED);
		if(key&KEYM_ALT)
			zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,0,XKB_KEY_Alt_L,WL_KEYBOARD_KEY_STATE_RELEASED);
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
		zwp_input_method_context_v1_keysym(keyboard->context,keyboard->serial,0,sym,WL_KEYBOARD_KEY_STATE_PRESSED,mask);
		zwp_input_method_context_v1_keysym(keyboard->context,keyboard->serial,0,sym,WL_KEYBOARD_KEY_STATE_RELEASED,mask);
	}
}

static int preedit_draw(struct simple_im *keyboard,const char *s)
{
	char out[512];
	char *p;
	int cursor;
	snprintf(out,sizeof(out),s);
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
	keyboard->preedit_string=out[0]?1:0;

	return 0;
}

static void preedit_clear(struct simple_im *keyboard)
{
	preedit_draw(keyboard,"");
}

static void input_method_keyboard_key_real(
		struct simple_im *keyboard,
		uint32_t key,
		uint32_t state)
{
	// printf("input_method_keyboard_key_real %u %u\n",key,state);
	int res=FALSE;
	int yk=GetKey(keyboard,key,state);
	if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
	{
		if(YK_CODE(yk)>=YK_LSHIFT && YK_CODE(yk)<=YK_RWIN && keyboard->last_press==yk)
			return;
#ifndef WAYLAND_STANDALONE
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
#endif
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
		zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,keyboard->time,key,state);
		if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
			return;
		if(YK_CODE(yk)!=keyboard->last_press || keyboard->time-keyboard->last_press_time>300)
			return;
		yk&=~KEYM_UP;
	}
	if(state==WL_KEYBOARD_KEY_STATE_RELEASED && ((yk&~KEYM_UP)==keyboard->last_press))
	{
		keyboard->last_press=0;
	}
	if(!keyboard->enable)
	{
		if(yk==keyboard->trigger && state)
		{
#ifdef WAYLAND_STANDALONE
			client_enable(CLIENT_ID_VAL);
#else
			ybus_on_open(&plugin,(CONN_ID)keyboard,CLIENT_ID_VAL);
#endif
			keyboard->enable=TRUE;
			res=TRUE;
		}
	}
	else
	{
#ifdef WAYLAND_STANDALONE
		if(yk==keyboard->trigger) l_call_client_connect();
		res=client_input_key(CLIENT_ID_VAL,yk,keyboard->time);
#else
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
								xim_send_key(yconn->id,client->id,p[i]);
							}
						}
					}
					else
					{
						for(int i=0;i<4 && p[i];i++)
						{
							xim_send_key(yconn->id,client->id,p[i]);
						}
					}
					return;
				}
				else if(tmp==0)
				{
					return;
				}
			}
		}
		res=ybus_on_key(&plugin,(CONN_ID)keyboard,CLIENT_ID_VAL,yk);
#endif
	}
	if(!res)
	{
		zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,keyboard->time,key,state);
	}
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

static void input_method_keyboard_key(
		struct simple_im *keyboard,
		struct zwp_input_method_keyboard_grab_v2 *zwp_input_method_keyboard_grab_v2,
		uint32_t serial,
		uint32_t time,
		uint32_t key,
		uint32_t state)
{
	// printf("input_method_keyboard_key %u %u %u %u\n",serial,time,key,state);
	keyboard->time=time;
	if(keyboard->repeat_tmr)
	{
		g_source_remove(keyboard->repeat_tmr);
		keyboard->repeat_tmr=0;
	}
	input_method_keyboard_key_real(keyboard,key,state);
	if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
	{
		if(keyboard->last_press>=YK_LSHIFT && keyboard->last_press<=YK_RWIN)
			return;
		keyboard->repeat_key=key;
		keyboard->repeat_tmr=g_timeout_add(keyboard->repeat_delay,(GSourceFunc)repeat_delay_func,keyboard);
	}
}

static void input_method_keyboard_modifiers(
		struct simple_im *keyboard,
		struct zwp_input_method_keyboard_grab_v2 *zwp_input_method_keyboard_grab_v2,
		uint32_t serial,
		uint32_t mods_depressed,
		uint32_t mods_latched,
		uint32_t mods_locked,
		uint32_t group)
{
	// printf("input_method_keyboard_modifiers %u %u %u %u\n",mods_depressed,mods_latched,mods_locked,group);
	zwp_virtual_keyboard_v1_modifiers(keyboard->virtual_keyboard_v1,mods_depressed,mods_latched,mods_locked,group);

	xkb_state_update_mask(keyboard->state, mods_depressed,
			      mods_latched, mods_locked, 0, 0, group);
	xkb_mod_mask_t mask = xkb_state_serialize_mods(keyboard->state,
					XKB_STATE_DEPRESSED | XKB_STATE_LATCHED);
	keyboard->modifiers = 0;
	if (mask & keyboard->control_mask)
		keyboard->modifiers |= MOD_CONTROL_MASK;
	if (mask & keyboard->alt_mask)
		keyboard->modifiers |= MOD_ALT_MASK;
	if (mask & keyboard->shift_mask)
		keyboard->modifiers |= MOD_SHIFT_MASK;
	if (mask & keyboard->super_mask)
		keyboard->modifiers |= MOD_SUPER_MASK;
	if (mask & keyboard->lock_mask)
		keyboard->modifiers |= MOD_LOCK_MASK;
}

static void input_method_keybaord_repeat_info(
		struct simple_im *keyboard,
		struct zwp_input_method_keyboard_grab_v2 *zwp_input_method_keyboard_grab_v2,
		int32_t rate,
		int32_t delay)
{
	//printf("repeat rate=%d delay=%d\n",rate,delay);
	keyboard->repeat_rate=rate;
	keyboard->repeat_delay=delay;
}

static struct zwp_input_method_keyboard_grab_v2_listener input_method_keyboard_listener={
	(void*)input_method_keyboard_keymap,
	(void*)input_method_keyboard_key,
	(void*)input_method_keyboard_modifiers,
	(void*)input_method_keybaord_repeat_info
};

static void input_method_activate(
		struct simple_im *keyboard,
		struct zwp_input_method_v2 *zwp_input_method_v2)
{
	// printf("input_method_activate\n");
	if(!simple_im.keyboard_grab_v2)
	{
		simple_im.keyboard_grab_v2=zwp_input_method_v2_grab_keyboard(zwp_input_method_v2);
		zwp_input_method_keyboard_grab_v2_add_listener(simple_im.keyboard_grab_v2,&input_method_keyboard_listener,keyboard);
	}
#ifdef WAYLAND_STANDALONE
	client_focus_in(CLIENT_ID_VAL);
#else
	ybus_on_focus_in(&plugin,(CONN_ID)keyboard,CLIENT_ID_VAL);
#endif
}

static void input_method_deactivate(
		struct simple_im *keyboard,
		struct zwp_input_method_v2 *zwp_input_method_v2)
{
	// printf("input_method_deactivate\n");
#ifdef WAYLAND_STANDALONE
	client_focus_out(CLIENT_ID_VAL);
#else
	ybus_on_focus_out(&plugin,(CONN_ID)keyboard,CLIENT_ID_VAL);
#endif
}

static void input_method_done(
		struct simple_im *keyboard,
		struct zwp_input_method_v2 *zwp_input_method_v2)
{
	// printf("input method done\n");
	keyboard->serial++;
}

static void input_method_unavailable(void *data,
			    struct zwp_input_method_v2 *zwp_input_method_v2)
{
	// printf("input method unavailable\n");
	exit(0);
}

static const struct zwp_input_method_v2_listener input_method_listener_v2 = {
	(void*)input_method_activate,
	(void*)input_method_deactivate,
	(void*)noop,
	(void*)noop,
	(void*)noop,
	(void*)input_method_done,
	(void*)input_method_unavailable
};

static void input_method_context_commit_state_v1(void *data,
			    struct zwp_input_method_context_v1 *context,
			    uint32_t serial)
{
	struct simple_im *keyboard = data;
	keyboard->serial = serial;
}

static const struct zwp_input_method_context_v1_listener input_method_context_listener_v1={
	(void*)noop,
	(void*)noop,
	(void*)noop,
	(void*)noop,
	(void*)input_method_context_commit_state_v1,
	(void*)noop
};

static void input_method_keyboard_keymap_v1(struct simple_im *keyboard,
			     struct wl_keyboard *wl_keyboard,
			     uint32_t format,
			     int32_t fd,
			     uint32_t size)
{
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
	{
		close(fd);
		return;
	}
	char *map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_str == MAP_FAILED)
	{
		close(fd);
		return;
	}
	if(keyboard->keymap)
	{
		xkb_keymap_unref(keyboard->keymap);
	}
	keyboard->keymap =
		xkb_keymap_new_from_string(keyboard->xkb_context,
					map_str,
					XKB_KEYMAP_FORMAT_TEXT_V1,
					0);

	munmap(map_str, size);
	close(fd);
	if (!keyboard->keymap) {
		fprintf(stderr, "failed to compile keymap\n");
		return;
	}
	keyboard->state = xkb_state_new(keyboard->keymap);
	if (!keyboard->state) {
		fprintf(stderr, "failed to create XKB state\n");
		xkb_keymap_unref(keyboard->keymap);
		return;
	}

	keyboard->control_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Control");
	keyboard->alt_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Mod1");
	keyboard->shift_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Shift");
	keyboard->super_mask = 
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Mod4");
	keyboard->lock_mask = 
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Lock");
}

static void input_method_keyboard_key_v1(struct simple_im *keyboard,
			  struct wl_keyboard *wl_keyboard,
			  uint32_t serial,
			  uint32_t time,
			  uint32_t key,
			  uint32_t state)
{
	if (!keyboard->state)
	{
		zwp_input_method_context_v1_key(keyboard->context,serial,time,key,state);
		return;
	}
	int res=FALSE;
	int yk=GetKey(keyboard,key,state);
	if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
	{
		if(YK_CODE(yk)>=YK_LSHIFT && YK_CODE(yk)<=YK_RWIN && keyboard->last_press==yk)
		{
			return;
		}
#ifndef WAYLAND_STANDALONE
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
#endif
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
		zwp_input_method_context_v1_key(keyboard->context,serial,time,key,state);
		if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
			return;
		if(YK_CODE(yk)!=keyboard->last_press || keyboard->time-keyboard->last_press_time>300)
			return;
		yk&=~KEYM_UP;
	}
	if(state==WL_KEYBOARD_KEY_STATE_RELEASED && ((yk&~KEYM_UP)==keyboard->last_press))
	{
		keyboard->last_press=0;
	}
	if(!keyboard->enable)
	{
		if(yk==keyboard->trigger && state)
		{
#ifdef WAYLAND_STANDALONE
			client_enable(CLIENT_ID_VAL);
#else
			ybus_on_open(&plugin,(CONN_ID)keyboard,CLIENT_ID_VAL);
#endif
			keyboard->enable=TRUE;
			res=TRUE;
		}
	}
	else
	{
#ifdef WAYLAND_STANDALONE
		if(yk==keyboard->trigger) l_call_client_connect();
		res=client_input_key(CLIENT_ID_VAL,yk,keyboard->time);
#else
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
								xim_send_key(yconn->id,client->id,p[i]);
							}
						}
					}
					else
					{
						for(int i=0;i<4 && p[i];i++)
						{
							xim_send_key(yconn->id,client->id,p[i]);
						}
					}
					return;
				}
				else if(tmp==0)
				{
					return;
				}
			}
		}
		res=ybus_on_key(&plugin,(CONN_ID)keyboard,CLIENT_ID_VAL,yk);
		// fprintf(stderr,"%x %d\n",yk,res);
#endif
	}
	if(!res)
	{
		zwp_input_method_context_v1_key(keyboard->context,serial,time,key,state);
	}
}

static void input_method_keyboard_modifiers_v1(struct simple_im *keyboard,
				struct wl_keyboard *wl_keyboard,
				uint32_t serial,
				uint32_t mods_depressed,
				uint32_t mods_latched,
				uint32_t mods_locked,
				uint32_t group)
{
	// fprintf(stderr,"input_method_keyboard_modifiers_v1 %u %u %u %u\n",mods_depressed,mods_latched,mods_locked,group);
	struct zwp_input_method_context_v1 *context = keyboard->context;
	xkb_state_update_mask(keyboard->state, mods_depressed,
			      mods_latched, mods_locked, 0, 0, group);
	xkb_mod_mask_t mask = xkb_state_serialize_mods(keyboard->state,
					XKB_STATE_DEPRESSED /*| XKB_STATE_LATCHED*/);
	keyboard->modifiers = 0;
	if (mask & keyboard->control_mask)
		keyboard->modifiers |= MOD_CONTROL_MASK;
	if (mask & keyboard->alt_mask)
		keyboard->modifiers |= MOD_ALT_MASK;
	if (mask & keyboard->shift_mask)
		keyboard->modifiers |= MOD_SHIFT_MASK;
	if (mask & keyboard->super_mask)
		keyboard->modifiers |= MOD_SUPER_MASK;
	if (mask & keyboard->lock_mask)
		keyboard->modifiers |= MOD_LOCK_MASK;
	zwp_input_method_context_v1_modifiers(context, serial,
				       mods_depressed, mods_latched,
				       mods_locked, group);
}

static const struct wl_keyboard_listener input_method_keyboard_listener_v1 = {
	(void*)input_method_keyboard_keymap_v1,
	(void*)noop,
	(void*)noop,
	(void*)input_method_keyboard_key_v1,
	(void*)input_method_keyboard_modifiers_v1
};

static void input_method_activate_v1(struct simple_im *keyboard,
			 struct zwp_input_method_v1 *zwp_input_method_v1,
			 struct zwp_input_method_context_v1 *context)
{
	// fprintf(stderr,"input_method_activate_v1\n");
	if (keyboard->context)
		zwp_input_method_context_v1_destroy(keyboard->context);
	keyboard->serial=0;
	keyboard->context = context	;
	zwp_input_method_context_v1_add_listener(context,
					  &input_method_context_listener_v1,
					  keyboard);
	keyboard->keyboard = zwp_input_method_context_v1_grab_keyboard(context);
	p_wl_keyboard_add_listener(keyboard->keyboard,
				 &input_method_keyboard_listener_v1,
				 keyboard);
#ifdef WAYLAND_STANDALONE
	client_focus_in(CLIENT_ID_VAL);
#else
	ybus_on_focus_in(&plugin,(CONN_ID)keyboard,CLIENT_ID_VAL);
#endif

}

static void input_method_deactivate_v1(struct simple_im *keyboard,
			   struct zwp_input_method_v1 *zwp_input_method_v1,
			   struct zwp_input_method_context_v1 *context)
{
	// fprintf(stderr,"input_method_deactivate_v1\n");
#ifdef WAYLAND_STANDALONE
	client_focus_out(CLIENT_ID_VAL);
#else
	ybus_on_focus_out(&plugin,(CONN_ID)keyboard,CLIENT_ID_VAL);
#endif
}


static const struct zwp_input_method_v1_listener input_method_listener_v1 = {
	(void*)input_method_activate_v1,
	(void*)input_method_deactivate_v1,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	struct simple_im *keyboard=data;
	// fprintf(stderr,"%s %u\n",interface,version);
	if(!strcmp(interface,"wl_seat"))
	{
		if(!l_source->display_owned)
		{
			GdkDisplay *display=gdk_display_get_default();
			GdkSeat *seat=gdk_display_get_default_seat(display);
			struct wl_seat *(*p_gdk_wayland_seat_get_wl_seat)(GdkSeat *seat);
			p_gdk_wayland_seat_get_wl_seat=dlsym(NULL,"gdk_wayland_seat_get_wl_seat");
			keyboard->seat=p_gdk_wayland_seat_get_wl_seat(seat);
		}
		else
		{
			keyboard->seat=p_wl_registry_bind(registry, name, p_wl_seat_interface, 1);
		}
	}
	else if(!strcmp(interface,"zwp_input_method_manager_v2"))
	{
		keyboard->input_method_manager_v2 = p_wl_registry_bind(registry, name, &zwp_input_method_manager_v2_interface, 1);
	}
	else if(!strcmp(interface,"zwp_virtual_keyboard_manager_v1"))
	{
		keyboard->virtual_keyboard_manager_v1 = p_wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
	}
	else if(!strcmp(interface,"zwlr_layer_shell_v1"))
	{
		keyboard->layer_shell_v1=true;
	}
	else if(!strcmp(interface,"zwp_input_method_v1"))
	{
		if(!keyboard->input_method_manager_v2)
		{
			keyboard->input_method_v1 = p_wl_registry_bind(registry, name, &zwp_input_method_v1_interface, 1);
			// fprintf(stderr,"input_method_v1=%p\n",keyboard->input_method_v1);
		}
	}
	else if(!strcmp(interface,"zwp_input_panel_v1"))
	{
		if(!keyboard->input_method_manager_v2)
		{
			keyboard->input_panel_v1=p_wl_registry_bind(registry, name, &zwp_input_panel_v1_interface, 1);
			// fprintf(stderr,"input_panel_v1=%p\n",keyboard->input_panel_v1);
		}
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static bool load_gtk_layer_shell(void)
{
#ifdef WAYLAND_STANDALONE
	return false;
#else
	bool ui_is_wayland(void);
	if(p_gtk_layer_init_for_window)
	   return true;
	if(!ui_is_wayland())
		return false;
	void *l_so=dlopen("libgtk-layer-shell.so.0",RTLD_LAZY);
	if(l_so!=NULL)
	{
		p_gtk_layer_init_for_window=dlsym(l_so,"gtk_layer_init_for_window");
		p_gtk_layer_set_layer=dlsym(l_so,"gtk_layer_set_layer");
		p_gtk_layer_set_margin=dlsym(l_so,"gtk_layer_set_margin");
		p_gtk_layer_set_keyboard_mode=dlsym(l_so,"gtk_layer_set_keyboard_mode");
		p_gtk_layer_set_anchor=dlsym(l_so,"gtk_layer_set_anchor");
		return true;
	}
	else
	{
		return false;
	}
#endif
}

int ybus_wayland_ui_init(void)
{
	void *l_so;

	if(l_source)
		return 0;
	
	l_so=dlopen("libwayland-client.so.0",RTLD_LAZY);
	if(!l_so)
	{
		//printf("wayland library not found\n");
		return -1;
	}

	p_wl_display_connect=dlsym(l_so,"wl_display_connect");
	p_wl_display_disconnect=dlsym(l_so,"wl_display_disconnect");
	p_wl_display_flush=dlsym(l_so,"wl_display_flush");
	p_wl_display_dispatch=dlsym(l_so,"wl_display_dispatch");
	p_wl_display_get_fd=dlsym(l_so,"wl_display_get_fd");
	p_wl_display_get_error=dlsym(l_so,"wl_display_get_error");
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
	p_wl_output_interface=dlsym(l_so,"wl_output_interface");
	input_method_unstable_v1_types[5]=p_wl_keyboard_interface;
	input_method_unstable_v1_types[9]=p_wl_surface_interface;
	input_method_unstable_v1_types[10]=p_wl_output_interface;
/*
	xdg_shell_types[6]=p_wl_surface_interface;
	xdg_shell_types[12]=p_wl_seat_interface;
	xdg_shell_types[16]=p_wl_seat_interface;
	xdg_shell_types[18]=p_wl_seat_interface;
	xdg_shell_types[21]=p_wl_output_interface;
	xdg_shell_types[22]=p_wl_seat_interface;

	wlr_layer_shell_unstable_v1_types[5]=p_wl_surface_interface;
	wlr_layer_shell_unstable_v1_types[6]=p_wl_output_interface;
*/
	p_gdk_wayland_display_get_wl_display=dlsym(NULL,"gdk_wayland_display_get_wl_display");
#if GTK_CHECK_VERSION(4,0,0)
	p_gdk_wayland_surface_get_wl_surface=dlsym(NULL,"gdk_wayland_surface_get_wl_surface");
#else
	p_gdk_wayland_window_set_use_custom_surface=dlsym(NULL,"gdk_wayland_window_set_use_custom_surface");
	p_gdk_wayland_window_get_wl_surface=dlsym(NULL,"gdk_wayland_window_get_wl_surface");
#endif

	l_source=g_wayland_source_new(NULL,NULL);
	if(!l_source) return -1;

	if(!l_source->display_owned)
	{
		load_gtk_layer_shell();
	}

	struct simple_im *keyboard=&simple_im;
	keyboard->display=g_wayland_source_get_display(l_source);
	keyboard->registry=p_wl_display_get_registry(simple_im.display);
	p_wl_registry_add_listener(keyboard->registry,&registry_listener,keyboard);
	p_wl_display_roundtrip(keyboard->display);
	
	if(keyboard->input_method_manager_v2)
		keyboard->input_method_v2=zwp_input_method_manager_v2_get_input_method(keyboard->input_method_manager_v2,keyboard->seat);
	else if(keyboard->input_method_v1)
	{
		zwp_input_method_v1_add_listener(keyboard->input_method_v1,
					  &input_method_listener_v1, keyboard);
	}

	return 0;
}

int ybus_wayland_init(void)
{	
#ifndef WAYLAND_STANDALONE
	ybus_add_plugin(&plugin);
#endif

	return 0;
}


#ifdef WAYLAND_STANDALONE

static void client_connect(void)
{
}

static int client_dispatch(const char *name,LCallBuf *buf)
{
	if(!strcmp(name,"commit"))
	{
		guint id;
		int ret;
		char text[1024];
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ret=l_call_buf_get_string(buf,text,sizeof(text));
		if(ret!=0) return -1;
		if(simple_im.preedit_string)
		{
			preedit_clear(&simple_im);
		}
		send_string(&simple_im,text);
	}
	else if(!strcmp(name,"preedit"))
	{
		guint id;
		int ret;
		char text[1024];
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ret=l_call_buf_get_string(buf,text,sizeof(text));
		if(ret!=0) return -1;
		preedit_draw(&simple_im,text);
	}
	else if(!strcmp(name,"preedit_clear"))
	{
		if(simple_im.preedit_string)
		{
			preedit_clear(&simple_im);
		}
	}
	else if(!strcmp(name,"forward"))
	{
		guint id;
		int ret;
		int key;
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ret=l_call_buf_get_val(buf,key);
		if(ret!=0) return -1;
		send_key(&simple_im,key);
	}
	else if(!strcmp(name,"enable"))
	{
		simple_im.enable=1;
	}
	else if(!strcmp(name,"disable"))
	{
		simple_im.enable=0;
	}
	else if(!strcmp(name,"trigger"))
	{
		int ret;
		ret=l_call_buf_get_val(buf,simple_im.trigger);
		if(ret!=0) return -1;
	}
	return 0;
}

static gboolean client_input_key(guint id,int key,guint32 time)
{
	int ret,res;
	ret=l_call_client_call("input",&res,"iii",id,key,time);
	if(ret!=0) return 0;
	return res?TRUE:FALSE;
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

static gboolean error_callback(GMainLoop *loop)
{
	g_main_loop_quit(loop);
	return FALSE;
}

int main(void)
{
	GMainLoop *loop;
	struct simple_im *keyboard=&simple_im;

	ybus_wayland_init();

	loop=g_main_loop_new(NULL,FALSE);
	g_wayland_source_set_error_callback(l_source,(GSourceFunc)error_callback,loop,NULL);

	keyboard->display=g_wayland_source_get_display(l_source);
	keyboard->registry=p_wl_display_get_registry(keyboard->display);
	wl_registry_add_listener(keyboard->registry,&registry_listener,keyboard);
	wl_display_roundtrip(keyboard->display);
	if(!keyboard->virtual_keyboard_v1 && keyboard->seat && keyboard->virtual_keyboard_manager_v1)
	{
		keyboard->virtual_keyboard_v1 = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(keyboard->virtual_keyboard_manager_v1, keyboard->seat);
		// printf("create virtual keyboard %p\n",keyboard->virtual_keyboard);
	}
	if(!keyboard->input_method_v2 && keyboard->seat && keyboard->input_method_manager_v2)
	{
		keyboard->input_method_v2=zwp_input_method_manager_v2_get_input_method(keyboard->input_method_manager_v2,keyboard->seat);
		// printf("get input method %p\n",keyboard->input_method);
		zwp_input_method_v2_add_listener(keyboard->input_method_v2,
					  &input_method_listener_v2, keyboard);
	}

	keyboard->xkb_context = xkb_context_new(0);
	keyboard->trigger = CTRL_SPACE;

	l_call_client_set_connect(client_connect);
	l_call_client_dispatch(client_dispatch);

	g_main_loop_run(loop);

	return 0;
}

#else

int ybus_wayland_get_custom(GtkWidget *w)
{
	return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"wayland-custom"));
}

int ybus_wayland_win_show(GtkWidget *w,int show)
{
	int win_type=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"wayland-custom"));
	if(win_type!=IM_WIN_INPUT)
		return -1;
	if(show)
	{
		void *surface=g_object_get_data(G_OBJECT(w),"wayland-surface");
		if(surface)
			return 0;
		struct wl_surface *s=p_gdk_wayland_window_get_wl_surface(gtk_widget_get_window(w));
		if(simple_im.input_method_manager_v2 && simple_im.input_method_v2)
		{
			surface=zwp_input_method_v2_get_input_popup_surface(simple_im.input_method_v2,s);
			g_object_set_data_full(G_OBJECT(w),"wayland-surface",surface,
				(GDestroyNotify)zwp_input_popup_surface_v2_destroy);
		}
		else if(simple_im.input_method_v1 && simple_im.input_panel_v1)
		{
			surface=zwp_input_panel_v1_get_input_panel_surface(simple_im.input_panel_v1,s);
			zwp_input_panel_surface_v1_set_overlay_panel(surface);
			g_object_set_data_full(G_OBJECT(w),"wayland-surface",surface,
				(GDestroyNotify)zwp_input_panel_surface_v1_destroy);
			fprintf(stderr,"show %p\n",surface);
		}
	}
	else
	{
		// fprintf(stderr,"hide\n");
		g_object_set_data(G_OBJECT(w),"wayland-surface",NULL);
	}
	return 0;
}

extern int wayland_show_hack;

static void wayland_init_input_win(struct simple_im *keyboard)
{
	if(!keyboard->wins.input)
		return;
	int type=ybus_wayland_get_custom(keyboard->wins.input);
	if(g_object_get_data(G_OBJECT(keyboard->wins.input),"wayland-surface")!=NULL)
		return;
	if(type==IM_WIN_INPUT && keyboard->input_method_manager_v2 && keyboard->input_method_v2)
	{
		if(!l_source)
			return;
		GdkWindow *w=gtk_widget_get_window(keyboard->wins.input);
		if(p_gdk_wayland_window_set_use_custom_surface!=NULL)
		{
			p_gdk_wayland_window_set_use_custom_surface(w);
		}
		if(wayland_show_hack)
		{
			struct wl_surface *s=p_gdk_wayland_window_get_wl_surface(w);
			if(s!=NULL && keyboard->input_method_v2)
			{
				void *surface=zwp_input_method_v2_get_input_popup_surface(keyboard->input_method_v2,s);
				g_object_set_data_full(G_OBJECT(keyboard->wins.input),"wayland-surface",surface,
						(GDestroyNotify)zwp_input_popup_surface_v2_destroy);
			}
		}
		else
		{
			gtk_widget_show(keyboard->wins.input);
		}
	}
	else if(type==IM_WIN_INPUT && keyboard->input_method_v1 && keyboard->input_panel_v1)
	{
		if(!l_source)
			return;
		GdkWindow *w=gtk_widget_get_window(keyboard->wins.input);
		if(p_gdk_wayland_window_set_use_custom_surface!=NULL)
		{
			p_gdk_wayland_window_set_use_custom_surface(w);
		}
		if(wayland_show_hack)
		{
			struct wl_surface *s=p_gdk_wayland_window_get_wl_surface(w);
			if(s!=NULL && keyboard->input_panel_v1)
			{
				void *surface=zwp_input_panel_v1_get_input_panel_surface(keyboard->input_panel_v1,s);
				zwp_input_panel_surface_v1_set_overlay_panel(surface);
				g_object_set_data_full(G_OBJECT(keyboard->wins.input),"wayland-surface",surface,
						(GDestroyNotify)zwp_input_panel_surface_v1_destroy);
			}
		}
		else
		{
			gtk_widget_show(keyboard->wins.input);
		}
	}
	else
	{
		if(!p_gtk_layer_init_for_window || !keyboard->layer_shell_v1)
			return;
		GtkWindow *w=GTK_WINDOW(keyboard->wins.input);
		p_gtk_layer_init_for_window(w);
		p_gtk_layer_set_layer(w,GTK_LAYER_SHELL_LAYER_OVERLAY);
		p_gtk_layer_set_keyboard_mode(w,GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
		p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_LEFT,TRUE);
		p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_RIGHT,FALSE);
		p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_TOP,TRUE);
		p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_BOTTOM,FALSE);
		g_object_set_data(G_OBJECT(w),"wayland-custom",GINT_TO_POINTER(IM_WIN_LAYER));
		g_object_set_data(G_OBJECT(keyboard->wins.input),"wayland-surface",GINT_TO_POINTER(1));
	}
}

static void wayland_init_tip_win(struct simple_im *keyboard)
{
	if(!l_source || l_source->display_owned)
		return;
	if(!keyboard->wins.tip)
		return;
	int type=ybus_wayland_get_custom(keyboard->wins.tip);
	if(g_object_get_data(G_OBJECT(keyboard->wins.tip),"wayland-surface")!=NULL)
		return;
	if(type==IM_WIN_INPUT && keyboard->input_method_manager_v2)
	{
		GdkWindow *w=gtk_widget_get_window(keyboard->wins.tip);
		if(p_gdk_wayland_window_set_use_custom_surface==NULL)
			return;
		p_gdk_wayland_window_set_use_custom_surface(w);
		if(wayland_show_hack)
		{
			struct wl_surface *s=p_gdk_wayland_window_get_wl_surface(w);
			if(s!=NULL && keyboard->input_method_v2)
			{
				struct zwp_input_popup_surface_v2 *surface=zwp_input_method_v2_get_input_popup_surface(keyboard->input_method_v2,s);
				g_object_set_data_full(G_OBJECT(keyboard->wins.tip),"wayland-surface",surface,
						(GDestroyNotify)zwp_input_popup_surface_v2_destroy);
			}
		}
		else
		{
			gtk_widget_show(keyboard->wins.tip);
		}
	}
	else
	{
		if(!p_gtk_layer_init_for_window || !keyboard->layer_shell_v1)
			return;
		GtkWindow *w=GTK_WINDOW(keyboard->wins.tip);
		p_gtk_layer_init_for_window(w);
		p_gtk_layer_set_layer(w,GTK_LAYER_SHELL_LAYER_OVERLAY);
		p_gtk_layer_set_keyboard_mode(w,GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
		p_gtk_layer_set_margin(w,GTK_LAYER_SHELL_EDGE_TOP,gdk_screen_height()/2-32);
		p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_LEFT,FALSE);
		p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_RIGHT,FALSE);
		p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_TOP,TRUE);
		p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_BOTTOM,FALSE);
		g_object_set_data(G_OBJECT(w),"wayland-custom",GINT_TO_POINTER(IM_WIN_LAYER));
		g_object_set_data(G_OBJECT(keyboard->wins.tip),"wayland-surface",GINT_TO_POINTER(1));
	}
}

static void wayland_init_main_win(struct simple_im *keyboard)
{
	if(!p_gtk_layer_init_for_window || !keyboard->layer_shell_v1)
	{
		if(keyboard->wins.main)
		{
			GdkWindow *w=gtk_widget_get_window(keyboard->wins.main);
			if(g_object_get_data(G_OBJECT(keyboard->wins.main),"wayland-surface")!=NULL)
				return;
			if(p_gdk_wayland_window_set_use_custom_surface!=NULL)
			{
				p_gdk_wayland_window_set_use_custom_surface(w);
				g_object_set_data(G_OBJECT(w),"wayland-custom",GINT_TO_POINTER(IM_WIN_CUSTOM));
				g_object_set_data(G_OBJECT(keyboard->wins.main),"wayland-surface",GINT_TO_POINTER(1));
				gtk_widget_show(keyboard->wins.main);
			}
		}
		return;
	}
	if(!keyboard->wins.main)
		return;
	if(g_object_get_data(G_OBJECT(keyboard->wins.main),"wayland-surface")!=NULL)
		return;
	GtkWindow *w=GTK_WINDOW(keyboard->wins.main);
	p_gtk_layer_init_for_window(w);
	p_gtk_layer_set_layer(w,GTK_LAYER_SHELL_LAYER_TOP);
	p_gtk_layer_set_keyboard_mode(w,GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_LEFT,TRUE);
	p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_RIGHT,FALSE);
	p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_TOP,TRUE);
	p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_BOTTOM,FALSE);
	g_object_set_data(G_OBJECT(w),"wayland-custom",GINT_TO_POINTER(IM_WIN_LAYER));
	g_object_set_data(G_OBJECT(keyboard->wins.main),"wayland-surface",GINT_TO_POINTER(1));
}

static void wayland_init_keyboard_win(struct simple_im *keyboard)
{
	if(!p_gtk_layer_init_for_window || !keyboard->layer_shell_v1)
			return;
	if(!keyboard->wins.keyboard)
		return;
	if(g_object_get_data(G_OBJECT(keyboard->wins.keyboard),"wayland-surface")!=NULL)
		return;
	GtkWindow *w=GTK_WINDOW(keyboard->wins.keyboard);
	p_gtk_layer_init_for_window(w);
	p_gtk_layer_set_layer(w,GTK_LAYER_SHELL_LAYER_TOP);
	p_gtk_layer_set_keyboard_mode(w,GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_LEFT,TRUE);
	p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_RIGHT,FALSE);
	p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_TOP,TRUE);
	p_gtk_layer_set_anchor(w,GTK_LAYER_SHELL_EDGE_BOTTOM,FALSE);
	g_object_set_data(G_OBJECT(w),"wayland-custom",GINT_TO_POINTER(IM_WIN_LAYER));
	g_object_set_data(G_OBJECT(keyboard->wins.keyboard),"wayland-surface",GINT_TO_POINTER(1));
}

void ybus_wayland_win_move(GtkWidget *w,int x,int y)
{
	if(IM_WIN_LAYER!=ybus_wayland_get_custom(w))
		return;
	if(p_gtk_layer_set_margin && simple_im.layer_shell_v1)
	{
		p_gtk_layer_set_margin(GTK_WINDOW(w),GTK_LAYER_SHELL_EDGE_LEFT,x);
		p_gtk_layer_set_margin(GTK_WINDOW(w),GTK_LAYER_SHELL_EDGE_TOP,y);
		g_object_set_data(G_OBJECT(w),"wayland-win-x",GINT_TO_POINTER(x));
		g_object_set_data(G_OBJECT(w),"wayland-win-y",GINT_TO_POINTER(y));
	}
}

void ybus_wayland_win_move_relative(GtkWidget *w,int dx,int dy)
{
	int x=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"wayland-win-x"));
	int y=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"wayland-win-y"));
	x+=dx;
	y+=dy;
	ybus_wayland_win_move(w,x,y);
}

static void on_win_realize(GtkWidget *w,struct simple_im *keyboard)
{
	// printf("realize %p\n",w);
	if(w==keyboard->wins.input)
	{
		wayland_init_input_win(keyboard);
	}
	else if(w==keyboard->wins.tip)
	{
		wayland_init_tip_win(keyboard);
	}
}

void ybus_wayland_set_window(GtkWidget *w,const char *role,int type)
{
	struct simple_im *keyboard=&simple_im;
	g_object_set_data(G_OBJECT(w),"wayland-custom",GINT_TO_POINTER(type));
	// printf("set win %p %s %d\n",w,role,type);
	if(!strcmp(role,"input"))
	{
		keyboard->wins.input=w;
		if(type==IM_WIN_LAYER)
			wayland_init_input_win(keyboard);
	}
	else if(!strcmp(role,"main"))
	{
		keyboard->wins.main=w;
		wayland_init_main_win(keyboard);
	}
	else if(!strcmp(role,"keyboard"))
	{
		keyboard->wins.keyboard=w;
		wayland_init_keyboard_win(keyboard);
	}
	else if(!strcmp(role,"tip"))
	{
		keyboard->wins.tip=w;
		if(type==IM_WIN_LAYER)
			wayland_init_tip_win(keyboard);
	}
	if(w!=NULL)
	{
		if(gtk_widget_get_realized(w))
			on_win_realize(w,keyboard);
	}
}

static int xim_getpid(CONN_ID conn_id)
{
	return 0;
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
	else if(!strcmp(config,"onspot"))
	{
		keyboard->onspot=va_arg(ap,int);
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

	if(!simple_im.onspot)
		return 0;
	return preedit_draw(&simple_im,s);
}

static void xim_send_string(CONN_ID conn_id,CLIENT_ID client_id,const char *s,int flags)
{
	char out[512];
	y_im_str_encode(s,out,flags);
	return send_string(&simple_im,out);
}

static void xim_send_key(CONN_ID conn_id,CLIENT_ID client_id,int key)
{
	send_key(&simple_im,key);
}

static int xim_init(void)
{
	struct simple_im *keyboard=&simple_im;

	if(!keyboard->virtual_keyboard_v1 && keyboard->seat && keyboard->virtual_keyboard_manager_v1)
	{
		keyboard->virtual_keyboard_v1 = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(keyboard->virtual_keyboard_manager_v1, keyboard->seat);
		// printf("create virtual keyboard %p\n",keyboard->virtual_keyboard);
	}
	if(keyboard->input_method_v2 && keyboard->seat)
	{
		// printf("get input method %p\n",keyboard->input_method);
		zwp_input_method_v2_add_listener(keyboard->input_method_v2,
					  &input_method_listener_v2, keyboard);
	}
	wayland_init_input_win(keyboard);
	wayland_init_tip_win(keyboard);
	wayland_init_main_win(keyboard);
	wayland_init_keyboard_win(keyboard);
	
	simple_im.xkb_context = xkb_context_new(0);

	YBUS_CONNECT *yconn=ybus_add_connect(&plugin,(CONN_ID)&simple_im);
	ybus_add_client(yconn,CLIENT_ID_VAL,0);
	return 0;	
}

#endif

