#include <stdarg.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <IMdkit.h>
#include <Xi18n.h>
#include <XimFunc.h>
#include <Xi18nX.h>

#include <gtk/gtk.h>
//#include <gdk/gdkx.h>

#include <langinfo.h>

#include "llib.h"
#include "xim.h"
#include "ybus.h"
#include "im.h"
#include "common.h"
#include "gbk.h"

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
	.name="xim",
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

#define DEFAULT_IMNAME	"yong"

static XIMStyle SupportStyle[] = {
	XIMPreeditCallbacks | XIMStatusNothing,		/* OnTheSpot */
	XIMPreeditCallbacks | XIMStatusCallbacks,
	XIMPreeditCallbacks | XIMStatusArea,
	XIMPreeditPosition  | XIMStatusNothing,		/* OverTheSpot */
	XIMPreeditPosition  | XIMStatusNone,
	XIMPreeditPosition  | XIMStatusArea,
	XIMPreeditNothing   | XIMStatusNothing,		/* Root */
	XIMPreeditNothing   | XIMStatusNone,
	0
};
static XIMStyle *OnSpotStyle=SupportStyle;
static int OnSpotStyleCount=sizeof(SupportStyle)/sizeof(XIMStyle)-1;
static XIMStyle *OverSpotStyle=SupportStyle+3;
static int OverSpotStyleCount=sizeof(SupportStyle)/sizeof(XIMStyle)-3-1;

static XIMEncoding SupportEncoding[] = {
	"COMPOUND_TEXT",
	NULL
};

static char SupportLocale[512]=
	"en_US.UTF-8,en_US.UTF8,en_US.utf8,"
	"zh_CN.UTF-8,zh_CN.UTF8,zh_CN.utf8,"
	"zh_CN.GB18030,zh_CN.GB2312,zh_CN,zh_CN.GBK,"
	"zh_TW.UTF-8,zh_TW.UTF8,zh_TW.utf8,"
	"zh_HK.UTF-8,zh_HK.UTF8,zh_HK.utf8";

static XIMTriggerKey Trigger_Keys[] = {
	{XK_space,ControlMask,ControlMask},
	{0L, 0L, 0L}
};

static int onspot;
static XIMS ims;
static Display *dpy;
static int screen;

static int xim_set_trigger(int key)
{
	XIMTriggerKeys on_keys;
	if(key<=0)
		return -1;

	if(key & KEYM_CTRL)
	{
		Trigger_Keys[0].modifier=ControlMask;
		Trigger_Keys[0].modifier_mask=ControlMask;
	}
	else if(key & KEYM_SHIFT)
	{
		Trigger_Keys[0].modifier=ShiftMask;
		Trigger_Keys[0].modifier_mask=ShiftMask;
	}
	else if(key & KEYM_ALT)
	{
		Trigger_Keys[0].modifier=Mod1Mask;
		Trigger_Keys[0].modifier_mask=Mod1Mask;
	}
	else if(key & KEYM_SUPER)
	{
		Trigger_Keys[0].modifier=Mod4Mask;
		Trigger_Keys[0].modifier_mask=Mod4Mask;
	}
	/*if(YK_CODE(key)==YK_LALT)
	{
		Trigger_Keys[0].keysym=XK_Alt_L;
	}
	else if(YK_CODE(key)==YK_LSHIFT)
	{
		Trigger_Keys[0].keysym=XK_Shift_L;
	}
	else
	{
		Trigger_Keys[0].keysym=YK_CODE(key);
	}*/
	switch(YK_CODE(key)){
	case YK_LALT:
		Trigger_Keys[0].keysym=XK_Alt_L;
		break;
	case YK_RALT:
		Trigger_Keys[0].keysym=XK_Alt_R;
		break;
	case YK_LSHIFT:
		Trigger_Keys[0].keysym=XK_Shift_L;
		break;
	case YK_RSHIFT:
		Trigger_Keys[0].keysym=XK_Shift_R;
		break;
	case YK_LCTRL:
		Trigger_Keys[0].keysym=XK_Control_L;
		break;
	case YK_RCTRL:
		Trigger_Keys[0].keysym=XK_Control_R;
		break;
	case YK_LWIN:
		Trigger_Keys[0].keysym=XK_Super_L;
		break;
	case YK_RWIN:
		Trigger_Keys[0].keysym=XK_Super_L;
		break;
	default:
		Trigger_Keys[0].keysym=YK_CODE(key);
		break;
	}

	if(!ims)
		return 0;
	on_keys.count_keys = 1;
	on_keys.keylist = Trigger_Keys;
	if(ims)
		IMSetIMValues(ims,IMOnKeysList,&on_keys,NULL);
	return 0;
}

typedef struct{
	Window client_window;
	Window focus_window;
	INT32 input_style;
	int last_len;
}YBUS_CLIENT_PRIV;

static int error_code;

static int xim_check_window(Window win)
{
	Window root;
	int x, y;
	unsigned width, height, bw, depth;
	error_code=0;
	XGetGeometry(dpy,win,&root,&x,&y,&width,&height,&bw,&depth);
	XSync(dpy, False);
	if (error_code==0) return 1;
	return 0;
}

static int xim_getpid(CONN_ID conn_id)
{
	YBUS_CONNECT *conn=ybus_find_connect(&plugin,conn_id);
	Xi18nClient *client;
	XClient *xc;
	Xi18nCore *core=ims->protocol;
	
	client=_Xi18nFindClient(core,conn->id);
	if(!client)
	{
		return -1;
	}
	xc=client->trans_rec;
	if(!xim_check_window(xc->client_win))
	{
		core->methods.disconnect(ims,conn->id);
		return -1;
	}
			
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
		ret=xim_set_trigger(key);
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
	IMPreeditStateStruct ips;
    ips.major_code = 0;
    ips.minor_code = 0;
    ips.icid = client_id;
    ips.connect_id = conn_id;
    IMPreeditStart (ims, (XPointer)&ips);
    ybus_on_open(&plugin,conn_id,client_id);
}

static void xim_close_im(CONN_ID conn_id,CLIENT_ID client_id)
{	
	IMPreeditStateStruct ips;
    ips.major_code = 0;
    ips.minor_code = 0;
    ips.icid = client_id;
    ips.connect_id = conn_id;
    IMPreeditEnd (ims, (XPointer)&ips);
    ybus_on_close(&plugin,conn_id,client_id);
}

static void xim_preedit_clear(CONN_ID conn_id,CLIENT_ID client_id)
{
	IMPreeditCBStruct data;
	XIMText text;
	XIMFeedback feedback[1] = {0};
	YBUS_CLIENT_PRIV *priv=ybus_get_priv(&plugin,conn_id,client_id);
	if(!priv) return;
	if(!onspot) return;
	if(priv->last_len<=0)
		return;
	if(!(priv->input_style & XIMPreeditCallbacks))
		return;
	data.major_code = XIM_PREEDIT_DRAW;
	data.connect_id = conn_id;
	data.icid = client_id;
	data.todo.draw.caret = 0;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = priv->last_len;
	data.todo.draw.text = &text;

	text.feedback = feedback;
	text.length = 0;
	text.string.multi_byte = "";
	IMCallCallback(ims, (XPointer)&data);
	priv->last_len = 0;

}

static int xim_preedit_draw(CONN_ID conn_id,CLIENT_ID client_id,const char *s)
{
	YBUS_CLIENT_PRIV *priv=ybus_get_priv(&plugin,conn_id,client_id);
	int i;
	IMPreeditCBStruct data;
	XIMFeedback feedback[256];
	XIMText text;
	XTextProperty tp;
	int len;

	
	if(!priv)
		return 0;
	if(!(priv->input_style & XIMPreeditCallbacks))
		return 0;
		
	if(!onspot) return 0;

	len=l_gb_strlen(s,-1);

	for(i=0; i<len; i++)
		feedback[i] = XIMUnderline;
	feedback[len] = 0;

	data.major_code = XIM_PREEDIT_DRAW;
	data.connect_id = conn_id;
	data.icid = client_id;
	data.todo.draw.caret = len;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = priv->last_len;
	data.todo.draw.text = &text;
	text.encoding_is_wchar = False;
	{
		char temp[512];
		l_gb_to_utf8(s,temp,sizeof(temp));s=temp;
		XmbTextListToTextProperty (dpy, (char **)&s, 1, XCompoundTextStyle, &tp);
	}
	text.string.multi_byte = (char*)tp.value;
	text.length = strlen((char*)tp.value);
	text.feedback = feedback;

	IMCallCallback(ims, (XPointer)&data);
	XFree (tp.value);

	priv->last_len=len;
	
	return 0;
}

static void xim_send_string(CONN_ID conn_id,CLIENT_ID client_id,const char *s,int flags)
{
	XTextProperty tp;
	IMCommitStruct cms;
	char out[512];
	char *ps;
	
	xim_preedit_clear(conn_id,client_id);

	{
		y_im_str_encode(s,out,0);
		ps=out;
	}

	XmbTextListToTextProperty (dpy, (char **) &ps, 1, XCompoundTextStyle, &tp);
	memset (&cms, 0, sizeof (cms));
	cms.major_code = XIM_COMMIT;
	cms.icid = client_id;
	cms.connect_id = conn_id;
	cms.flag = XimLookupChars;
	cms.commit_string = (char *) tp.value;
	IMCommitString (ims, (XPointer) & cms);
	XFree (tp.value);
}

static int GetKey_r(int yk)
{
	int vk;

	switch(yk){
	case YK_BACKSPACE:vk=XK_BackSpace;break;
	case YK_ESC:vk=XK_Escape;break;
	case YK_DELETE:vk=XK_Delete;break;
	case YK_ENTER:vk=XK_Return;break;
	case YK_HOME:vk=XK_Home;break;
	case YK_END:vk=XK_End;break;
	case YK_PGUP:vk=XK_Page_Up;break;
	case YK_PGDN:vk=XK_Page_Down;break;
	case YK_LEFT:vk=XK_Left;break;
	case YK_DOWN:vk=XK_Down;break;
	case YK_UP:vk=XK_Up;break;
	case YK_RIGHT:vk=XK_Right;break;
	case YK_TAB:vk=XK_Tab;break;
	case 'V':vk=XK_V;break;
	default:return -1;
	}
	vk=XKeysymToKeycode(dpy,vk);
	return vk;
}

static void xim_send_key(CONN_ID conn_id,CLIENT_ID client_id,int key,int repeat)
{
	YBUS_CLIENT_PRIV *priv;
	
	IMForwardEventStruct forwardEvent;
	XEvent          xEvent;
	int keycode;
	int state=0;
	
	keycode=GetKey_r(YK_CODE(key));
	if(keycode<0)
		return;
		
	priv=ybus_get_priv(&plugin,conn_id,client_id);
	if(!priv) return;
		
	if(key & KEYM_CTRL)
		state|=ControlMask;
	if(key & KEYM_SHIFT)
		state|=ShiftMask;

	memset (&forwardEvent, 0, sizeof (IMForwardEventStruct));
	forwardEvent.connect_id = conn_id;
	forwardEvent.icid = client_id;
	forwardEvent.major_code = XIM_FORWARD_EVENT;
	forwardEvent.sync_bit = 0;
	forwardEvent.serial_number = 0L;

	xEvent.xkey.type = KeyPress;
	xEvent.xkey.display = dpy;
	xEvent.xkey.serial = 0L;
	xEvent.xkey.send_event = False;
	xEvent.xkey.x = xEvent.xkey.y = xEvent.xkey.x_root = xEvent.xkey.y_root = 0;
	xEvent.xkey.same_screen = False;
	xEvent.xkey.subwindow = None;
	xEvent.xkey.window = None;
	xEvent.xkey.root = DefaultRootWindow (dpy);
	xEvent.xkey.state = state;
	if (priv->focus_window)
		xEvent.xkey.window = priv->focus_window;
	else if (priv->client_window)
		xEvent.xkey.window = priv->client_window;

	xEvent.xkey.keycode = keycode;
	for(int i=0;i<repeat;i++)
	{
		xEvent.xkey.type = KeyPress;
		memcpy (&(forwardEvent.event), &xEvent, sizeof (forwardEvent.event));
		IMForwardEvent (ims, (XPointer) (&forwardEvent));

		xEvent.xkey.type = KeyRelease;
		memcpy (&(forwardEvent.event), &xEvent, sizeof (forwardEvent.event));
		IMForwardEvent (ims, (XPointer) (&forwardEvent));
	}
}

static XErrorHandler OldXErrorHandler;
static int YongXErrorHandler(Display *d, XErrorEvent *e)
{
	if(e->error_code!=BadMatch &&
		e->error_code!=BadDrawable &&
		e->error_code!=BadWindow)
	{
		OldXErrorHandler(d,e);
	}
	error_code=e->error_code;
	return 0;
}

void YongSetXErrorHandler(void)
{
	OldXErrorHandler=XSetErrorHandler(YongXErrorHandler);
}

static Bool YongOpenHandler(IMOpenStruct *data)
{
	ybus_add_connect(&plugin,data->connect_id);
	return True;
}

static Bool YongCloseHandler (IMOpenStruct * data)
{
	YBUS_CONNECT *conn;
	conn=ybus_find_connect(&plugin,data->connect_id);
	if(!conn) return True;
	ybus_free_connect(conn);
	return True;
}

static Bool set_cursor_location_default(CONN_ID conn_id,CLIENT_ID client_id)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	YBUS_CLIENT_PRIV *priv;
	XWindowAttributes xwa;
	Window child;
	Window window;
	

	conn=ybus_find_connect(&plugin,conn_id);
	if(!conn) return True;
	client=ybus_find_client(conn,client_id);
	if(!client) return True;

	if(client->track)
		return True;

	priv=(YBUS_CLIENT_PRIV*)client->priv;
	window=priv->focus_window?priv->focus_window:priv->client_window;
	if(!window) return True;
	XGetWindowAttributes (dpy, window, &xwa);
	XTranslateCoordinates (dpy, window,
			xwa.root,
			0,
			xwa.height,
			&client->x,
			&client->y,
			&child);

	YongMoveInput(client->x,client->y);
	return True;
}

static void store_ic_values(YBUS_CLIENT *client,IMChangeICStruct *data)
{
	XICAttribute *ic_attr = data->ic_attr;
	XICAttribute *pre_attr = data->preedit_attr;
	YBUS_CLIENT *active=NULL;
	
	int i;
	YBUS_CLIENT_PRIV *priv=(YBUS_CLIENT_PRIV*)client->priv;
	
	for(i=0;i<data->ic_attr_num;++i, ++ic_attr)
	{
		if (strcmp (XNInputStyle, ic_attr->name) == 0)
            priv->input_style = *(uint32_t *) ic_attr->value;
		if (strcmp (XNClientWindow, ic_attr->name) == 0)
            priv->client_window = (Window)(*(CARD32 *) data->ic_attr[i].value);
        else if (strcmp (XNFocusWindow, ic_attr->name) == 0)
            priv->focus_window = (Window)(*(CARD32 *) data->ic_attr[i].value);
	}
	
	for(i=0; i< data->preedit_attr_num; ++i, ++pre_attr)
	{
		if (strcmp (XNSpotLocation, pre_attr->name) == 0)
		{
			Bool ret=False;
			XPoint *pt=((XPoint *)pre_attr->value);
			int X,Y;
			Window window;
			
			if (priv->focus_window)
			{
				ret=XTranslateCoordinates(dpy, priv->focus_window, RootWindow(dpy,screen),
					pt->x, pt->y, &X, &Y, &window);
			}
			if (!ret && priv->client_window)
			{
				ret=XTranslateCoordinates(dpy, priv->client_window, RootWindow(dpy,screen),
					pt->x, pt->y, &X, &Y, &window);
			}			
			if(ret==False)
			{
				break;
			}
#if !GTK_CHECK_VERSION(3,0,0)
			if(gtk_major_version==2 && gtk_minor_version<15)
			{
				Y+=18; /* work around bug at low version of gtk */
			}
#endif
			client->track=1;
			client->x=X;
			client->y=Y;
			
			ybus_get_active(0,&active);
			if(active==client)
			{
				YongMoveInput(X,Y);
			}
			break;
		}
	}
}

static Bool YongCreateICHandler (IMChangeICStruct * data)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	static int base_icid=1;
	data->icid=base_icid++;
	conn=ybus_find_connect(&plugin,data->connect_id);
	if(!conn) return False;
	client=ybus_add_client(conn,data->icid,sizeof(YBUS_CLIENT_PRIV));
	store_ic_values(client,data);
	return True;
}

static Bool YongDestroyICHandler (IMChangeICStruct * data)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	conn=ybus_find_connect(&plugin,data->connect_id);
	if(!conn) return True;
	client=ybus_find_client(conn,data->icid);
	if(!client) return True;
	conn->alive=0;
	ybus_free_client(conn,client);
	return True;
}

static Bool YongGetICValuesHandler (IMChangeICStruct * data)
{
	XICAttribute *ic_attr = data->ic_attr;
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	int i;
	
	conn=ybus_find_connect(&plugin,data->connect_id);
	if(!conn) return False;
	client=ybus_find_client(conn,data->icid);
	if(!client) return False;
	
	for (i = 0; i < data->ic_attr_num; ++i, ++ic_attr) {
        if (strcmp (XNFilterEvents, ic_attr->name) == 0) {
            ic_attr->value = (void *) malloc (sizeof (CARD32));
            *(CARD32 *) ic_attr->value = KeyPressMask | KeyReleaseMask;
            ic_attr->value_length = sizeof (CARD32);
        }
    }
#if 1
    /* useless, just let the application not crash */
    XICAttribute   *sts_attr = data->status_attr;
    for (i = 0; i < (int) data->status_attr_num; i++, sts_attr++)
    {
		if (!strcmp (XNArea, sts_attr->name))
		{
			sts_attr->value = (void *) calloc (1,sizeof (XRectangle));
			sts_attr->value_length = sizeof (XRectangle);
		}
		else if (!strcmp (XNAreaNeeded, sts_attr->name))
		{
			sts_attr->value = (void *) calloc (1,sizeof (XRectangle));
			sts_attr->value_length = sizeof (XRectangle);
		}
		else if (!strcmp (XNFontSet, sts_attr->name))
		{
			CARD16          base_len = (CARD16) strlen ("Monospace");
			int             total_len = sizeof (CARD16) + (CARD16) base_len;
			char           *p;

			sts_attr->value = (void *) malloc (total_len);
			p = (char *) sts_attr->value;
			memmove (p, &base_len, sizeof (CARD16));
			p += sizeof (CARD16);
			memmove (p, "Monospace", base_len);
			sts_attr->value_length = total_len;

		}
		else if (!strcmp (XNForeground, sts_attr->name))
		{
			sts_attr->value = (void *) malloc (sizeof (long));
			*(long *) sts_attr->value = 0;
			sts_attr->value_length = sizeof (long);
		}
		else if (!strcmp (XNBackground, sts_attr->name))
		{
			sts_attr->value = (void *) malloc (sizeof (long));
			*(long *) sts_attr->value = 0;
			sts_attr->value_length = sizeof (long);

		}
		else if (!strcmp (XNLineSpace, sts_attr->name))
		{
			sts_attr->value = (void *) malloc (sizeof (long));
			*(long *) sts_attr->value = 18;
			sts_attr->value_length = sizeof (long);
		}
    }
#endif
	return True;
}

static Bool YongSetICValuesHandler (IMChangeICStruct * data)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	
	conn=ybus_find_connect(&plugin,data->connect_id);
	if(!conn) return True;
	client=ybus_find_client(conn,data->icid);
	if(!client) return True;
	store_ic_values(client,data);
	return True;
}

static Bool YongTriggerNotifyHandler (IMTriggerNotifyStruct * data)
{
	ybus_on_focus_in(&plugin,data->connect_id,data->icid);
	ybus_on_open(&plugin,data->connect_id,data->icid);
	return True;
}

static Bool YongSetFocusHandler (IMChangeFocusStruct * data)
{
	ybus_on_focus_in(&plugin,data->connect_id,data->icid);
	return True;
}

static Bool YongUnsetFocusHandler (IMChangeFocusStruct * data)
{
	ybus_on_focus_out(&plugin,data->connect_id,data->icid);
	return True;
}

int GetKey(int KeyCode,int KeyState)
{
	int ret;
	int mask;

	switch(KeyCode){
	case XK_ISO_Left_Tab:
		KeyCode=YK_TAB;
		break;
	}
	if (KeyState & Mod2Mask)
	{
		if (KeyCode >= 0xffaa && KeyCode <= 0xffb9)
			KeyCode = "*+ -./0123456789"[KeyCode-0xffaa];
		else /* remove the mask for other keys */
			KeyState &=~ Mod2Mask;
		/* bug: gtk will only send one release key at 0xffaa-0xffaf */
		/* other like xterm is just right, so don't deal it now */
	}
	if((KeyCode&0xff)<0x20 || (KeyCode&0xff)>=0x80)
		KeyCode=KeyCode&0xff;
	ret=KeyCode;

	if ((KeyState & ControlMask) && KeyCode!=YK_LCTRL && KeyCode!=YK_RCTRL)
		ret|=KEYM_CTRL;
	if ((KeyState & ShiftMask) && KeyCode!=YK_LSHIFT && KeyCode!=YK_RSHIFT)
		ret|=KEYM_SHIFT;
	if ((KeyState & Mod1Mask) && KeyCode!=YK_LALT && KeyCode!=YK_RALT)
		ret|=KEYM_ALT;
	if ((KeyState & Mod4Mask) && KeyCode!=YK_LWIN && KeyCode!=YK_RWIN)
		ret|=KEYM_SUPER;
	if(KeyState & Mod2Mask)
		ret|=KEYM_KEYPAD;
	if(KeyState & LockMask)
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

static Bool YongForwardHandler(IMForwardEventStruct *data)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;

	XKeyEvent *kev=(XKeyEvent *) & data->event;
	KeySym keysym;
	int KeyState;
	char strbuf[64];
	int Key;
	
	static int last_press;
	static Time last_press_time;
	static int bing;
	
	if(ybus_get_active(&conn,&client))
	{
		IMForwardEvent(ims,(XPointer)data);
		return True;
	}
	if(conn->plugin!=&plugin)
	{
		IMForwardEvent(ims,(XPointer)data);
		return True;
	}
	if(conn->id!=data->connect_id || client->id!=data->icid)
	{
		IMForwardEvent(ims,(XPointer)data);
		return True;
	}
	
	XLookupString (kev, strbuf, sizeof(strbuf), &keysym, NULL);
	KeyState = kev->state;
	Key = GetKey (keysym, KeyState);
	if(!Key)
	{
		IMForwardEvent(ims,(XPointer)data);
		return True;
	}
	
	if(kev->type==KeyPress)
	{
		if(im.Bing && ((Key>='a' && Key<='z') || Key==' '))
		{
			int diff=y_im_diff_hand(bing,Key);
			if(!bing)
			{
				if(Key!=' ')
					bing=Key;
			}
			else if(kev->time-last_press_time>=im.BingSkip[diff])
			{
				if(Key==' ') Key=bing;
				Key|=KEYM_BING;
				//printf("%d\n",kev->time-last_press_time);
			}
		}
		last_press=Key;
		last_press_time=kev->time;		
	}
	if(YK_CODE(Key)>=YK_LSHIFT && YK_CODE(Key)<=YK_RWIN)
	{
		bing=0;
		IMForwardEvent(ims,(XPointer)data);
		if(kev->type==KeyPress) return True;
		if(Key!=last_press || kev->time-last_press_time>300)
			return True;
	}
	if(kev->type==KeyRelease)
	{
		if(last_press==Key)
			last_press=0;
		bing=0;
	}
	
	if(conn->state && kev->type==KeyPress)
	{
		set_cursor_location_default(data->connect_id,data->icid);
	}
	
	switch(Key){
	case CTRL_SPACE:
	case SHIFT_SPACE:
	case KEYM_SUPER|YK_SPACE:
	{
		if(kev->type==KeyPress && ybus_on_key(&plugin,data->connect_id,data->icid,Key))
			return True;
		break;
	}
	case CTRL_LSHIFT:
	case CTRL_RSHIFT:
	case CTRL_LALT:
	case CTRL_RALT:
	case KEYM_CTRL|YK_LWIN:
	case KEYM_CTRL|YK_RWIN:
	{
		if(ybus_on_key(&plugin,data->connect_id,data->icid,Key))
			return True;
		break;
	}
	case YK_LCTRL:
	case YK_RCTRL:
	case YK_LSHIFT:
	case YK_RSHIFT:
	{
		ybus_on_key(&plugin,data->connect_id,data->icid,Key);
		return True;
	}
	default:
	{
		//if(kev->type==KeyRelease && YongHotKey(Key))
		//	return True;
		break;
	}}
	
	if(im.layout && !im.Bing && conn->state)
	{
		int tmp;
		tmp=Key&~KEYM_KEYPAD;
		if(!(tmp&KEYM_MASK))
		{
			tmp=YK_CODE(tmp);
			if(kev->type==KeyRelease)
			{
				tmp=y_layout_keyup(im.layout,tmp,kev->time);
			}
			else
			{
				tmp=y_layout_keydown(im.layout,tmp,kev->time);
			}
			if(tmp>0)
			{
				char *p=(char*)&tmp;
				int i;
				if(conn->lang==LANG_CN)
				{
					for(i=0;i<=3 && p[i];i++)
					{
						int ret=ybus_on_key(&plugin,data->connect_id,data->icid,p[i]);
						if(ret)
						{
							y_im_speed_update(p[i],0);
						}
						else
						{
							xim_send_key(data->connect_id,data->icid,p[i],1);
						}
					}
					return TRUE;
				}
				else
				{
					
					for(i=0;i<=3 && p[i];i++)
					{
						xim_send_key(data->connect_id,data->icid,p[i],1);
					}
					return TRUE;
				}
			}
			else if(tmp==0)
			{
				return TRUE;
			}
		}
	}
	
	if(conn->state && kev->type==KeyPress)
	{
		int ret=ybus_on_key(&plugin,data->connect_id,data->icid,Key);
		if(ret) return True;
	}
	
	if(kev->type==KeyPress || (KEYM_MASK & Key))
	{
		IMForwardEvent(ims,(XPointer)data);
	}

	// android studio will fail if we not send back something
	if(kev->type==KeyRelease)
	{
		IMForwardEvent(ims,(XPointer)data);
	}
	return True;
}

static Bool YongProtoHandler (XIMS _ims, IMProtocol * data)
{
#define XIM_DEBUG	0
	switch (data->major_code)
	{
		case XIM_OPEN:
#if XIM_DEBUG
			fprintf(stderr,"XIM_OPEN\n");
#endif
			return YongOpenHandler ((IMOpenStruct *) data);
		case XIM_CLOSE:
#if XIM_DEBUG
			fprintf(stderr,"XIM_CLOSE\n");
#endif
			return YongCloseHandler ((IMOpenStruct *) data);
		case XIM_CREATE_IC:
#if XIM_DEBUG
			fprintf(stderr,"XIM_CREATE_IC\n");
#endif
			return YongCreateICHandler ((IMChangeICStruct *) data);
		case XIM_DESTROY_IC:
#if XIM_DEBUG
			fprintf(stderr,"XIM_DESTROY_IC\n");
#endif
			return YongDestroyICHandler ((IMChangeICStruct *) data);
		case XIM_SET_IC_VALUES:
			//fprintf(stderr,"XIM_SET_IC_VALUES\n");
			return YongSetICValuesHandler ((IMChangeICStruct *) data);
		case XIM_GET_IC_VALUES:
#if XIM_DEBUG
			fprintf(stderr,"XIM_GET_IC_VALUES\n");
#endif
			return YongGetICValuesHandler ((IMChangeICStruct *) data);
		case XIM_FORWARD_EVENT:
#if XIM_DEBUG
			fprintf(stderr,"XIM_FORWARD_EVENT\n");
#endif
			return YongForwardHandler((IMForwardEventStruct *)data);
		case XIM_SET_IC_FOCUS:
#if XIM_DEBUG
			fprintf(stderr,"XIM_SET_IC_FOCUS\n");
#endif
			return YongSetFocusHandler ((IMChangeFocusStruct *)data);
		case XIM_UNSET_IC_FOCUS:
#if XIM_DEBUG
			fprintf(stderr,"XIM_UNSET_IC_FOCUS\n");
#endif
			return YongUnsetFocusHandler ((IMChangeFocusStruct *) data);;
		case XIM_RESET_IC:
#if XIM_DEBUG
			fprintf(stderr,"XIM_RESET_IC\n");
#endif
			return True;
		case XIM_TRIGGER_NOTIFY:
#if XIM_DEBUG
			fprintf(stderr,"XIM_TRIGER_NOTIFY\n");
#endif
			return YongTriggerNotifyHandler ((IMTriggerNotifyStruct *) data);
		default:
#if XIM_DEBUG
			fprintf(stderr,"%d\n",data->major_code);
#endif
			return False;
	}
#undef XIM_DEBUG
}

typedef struct _XIMSource
{
	GSource source;
	GPollFD poll;
}XIMSource;

static gboolean xim_source_prepare (GSource *source,gint *timeout)
{
	*timeout = -1;
	if(XPending(dpy)>0)
		return TRUE;
	return FALSE;
}

static gboolean xim_source_check(GSource *source)
{
	return (((XIMSource*)source)->poll.revents&G_IO_IN)?TRUE:FALSE;
}

static gboolean xim_source_dispatch (GSource *source,GSourceFunc callback,gpointer user_data)
{
	while(XPending(dpy)>0)
	{
		XEvent e;
		XNextEvent(dpy,&e);
		XFilterEvent(&e,None);
	}
	return TRUE;
}

static GSourceFuncs xim_source_funcs =
{
 	xim_source_prepare,
	xim_source_check,
	xim_source_dispatch,
	NULL
};

static guint xim_poll_display_fd(Display *dpy)
{
	XIMSource *s;
	s=(XIMSource*)g_source_new(&xim_source_funcs,sizeof(XIMSource));
	g_source_set_name((GSource*)s,"ybus-xim");
	s->poll.fd=ConnectionNumber(dpy);
	s->poll.events=G_IO_IN;
	g_source_add_poll((GSource*)s,&s->poll);
	return g_source_attach((GSource*)s,NULL);
}

static int xim_init(void)
{
	XIMStyles * input_styles;
	XIMTriggerKeys on_keys;
	XIMEncodings *encodings;
	char *p,*imname;
	Window im_window;
	guint source;

	imname = getenv ("XMODIFIERS");
	if(imname)
	{
		if (!strncmp (imname, "@im=",4))
			imname += 4;
		else
			imname = DEFAULT_IMNAME;
	}
	else
	{
		imname = DEFAULT_IMNAME;
	}
	
	input_styles = (XIMStyles *) malloc (sizeof (XIMStyles));
	if(onspot)
	{
		input_styles->count_styles = OnSpotStyleCount;
		input_styles->supported_styles = OnSpotStyle;
	}
	else
	{
		input_styles->count_styles = OverSpotStyleCount;
		input_styles->supported_styles = OverSpotStyle;
	}
	
	on_keys.count_keys = 1;
	on_keys.keylist = Trigger_Keys;
	
	encodings = (XIMEncodings *) malloc (sizeof (XIMEncodings));
	encodings->count_encodings = sizeof (SupportEncoding) / sizeof (XIMEncoding) - 1;
	encodings->supported_encodings =SupportEncoding;
	
	p = getenv ("LC_CTYPE");
	if (!p)
	{
		p = getenv ("LC_ALL");
		if (!p)	p = getenv ("LANG");
	}
	if (p)
	{
		if(!strstr(SupportLocale,p))
		{
			strcat(SupportLocale, ",");
			strcat(SupportLocale, p);
		}
	}
	
	//dpy=gdk_x11_get_default_xdisplay();
	if(!dpy)
		dpy=XOpenDisplay(NULL);
	if(!dpy) return -1;
	source=xim_poll_display_fd(dpy);
	
	screen=DefaultScreen(dpy);
	im_window=XCreateSimpleWindow(dpy,DefaultRootWindow(dpy),0,0,1,1,1,0,0);
	
	ims = IMOpenIM (dpy, IMModifiers, "Xi18n", IMServerWindow, im_window, IMServerName, imname,
		IMLocale, SupportLocale, IMServerTransport, "X/", IMInputStyles, input_styles,
		IMEncodingList, encodings, IMProtocolHandler, YongProtoHandler, IMFilterEventMask,
		KeyPressMask | KeyReleaseMask, IMOnKeysList,&on_keys,NULL);
		
	free(encodings);
	free(input_styles);

	if(!ims)
	{
		fprintf (stderr, "yong start fail, may XIM daemon named \"%s\" there\n", imname);
		g_source_remove(source);
		XDestroyWindow(dpy,im_window);
		XCloseDisplay(dpy);
		dpy=NULL;
		return -1;
	}

	return 0;
}

void ybus_xim_get_workarea(int *x, int *y, int *width, int *height)
{
	if(!dpy)
	{
		dpy=XOpenDisplay(NULL);
		if(!dpy)
			return;
	}
	Window root = RootWindow(dpy, DefaultScreen(dpy));
	Atom net_workarea_atom = XInternAtom(dpy, "_NET_WORKAREA", True);
	if(net_workarea_atom==None)
		return;
	Atom actualType;
	int format;
	unsigned long numItems, bytesAfter;
	unsigned char *data = NULL;
	int status=XGetWindowProperty(dpy,root,net_workarea_atom,0,4,False,
			AnyPropertyType,&actualType,&format,&numItems,&bytesAfter,&data);
	if(status!=Success)
		return;
	if (actualType == XA_CARDINAL && numItems>=4)
	{
		long *workArea = (long*)data;
		*x=workArea[0];
		*y=workArea[1];
		*width=workArea[2];
		*height=workArea[3];
	}
	XFree(data);
}


int ybus_xim_init(void)
{
	if(getenv("DISPLAY")==NULL)
		return -1;
	ybus_add_plugin(&plugin);
	return 0;
}
