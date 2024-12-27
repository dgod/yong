#include "common.h"
#include "ui.h"
#include "translate.h"
#include "ui-draw.h"

#include <math.h>
#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#else
#include <gtk/gtk.h>
void ybus_wayland_win_move(GtkWidget *w,int x,int y);
void ybus_wayland_win_move_relative(GtkWidget *w,int dx,int dy);
void ui_get_workarea(int *x, int *y, int *width, int *height);
#endif
#include <assert.h>

typedef struct{
	short x,y,w,h;
}KBD_RECT;

enum{
	KBTN_NORMAL=0,
	KBTN_SELECT,
};

enum{
	KBT_MAIN=0,
	KBT_SHIFT,
	KBT_NORMAL,
};

typedef struct{
	UI_COLOR bg[2];
	UI_COLOR fg[2];
	UI_COLOR border[2];
}BTN_DESC;

typedef struct{
	void *next;
	char *data;
	KBD_RECT rc;
	short type;
	short state;
}KBD_BTN;

typedef struct{
	const char *name;
	UI_FONT font;
	double scale;
	BTN_DESC desc[3];
	KBD_BTN main;
	KBD_BTN *shift;
	LPtrArray *line;
	UI_WINDOW win;
#if defined(GTK_CHECK_VERSION)
	UI_WINDOW canvas;
#endif
	KBD_BTN *psel;
	uint32_t key_mods;
}Y_KBD_LAYOUT;

typedef struct{
	LXml *config;
	Y_KBD_LAYOUT layout;
	int8_t cur;
	int8_t sub;
	uint8_t xim;
	uint8_t first;
	uint8_t show;
}Y_KBD_STATE;

static Y_KBD_STATE kst;
extern UI_WINDOW MainWin;

#if defined(GTK_CHECK_VERSION)
bool ui_is_wayland(void);
void ybus_wayland_set_window(GtkWidget *w,const char *role,int type);
int ybus_wayland_get_custom(GtkWidget *w);
void ui_win_show(GtkWidget *w,int show);
void ui_set_css(GtkWidget *widget,const gchar *data);
#endif

#include "keyboard.xml.c"

static void kbd_main_new(void);
static void kbd_main_show(int b);
int y_kbd_init(const char *fn)
{
#ifndef _WIN32
	if(getenv("FBTERM_IM_SOCKET"))
		return 0;
#endif
	memset(&kst,0,sizeof(kst));
	kst.first=1;
	char *text=l_file_get_contents(fn,NULL,y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);
	if(text)
	{
		kst.config=l_xml_load(text);
		l_free(text);
	}
	else
	{
		kst.config=l_xml_load((const char*)keyboard_xml);
	}
	if(!kst.config)
		return -2;
	LXmlNode *layout=l_xml_get_child(kst.config->root.child,"layout");
	if(!layout)
	{
		l_xml_free(kst.config);
		kst.config=NULL;
		return -3;
	}
	LXmlNode *data=l_xml_get_child(kst.config->root.child,"data");
	if(!data)
	{
		l_xml_free(kst.config);
		kst.config=NULL;
		return -4;
	}
	const char *keyboard=y_im_get_config_data("IM","keyboard");
	if(!keyboard)
		keyboard=l_xml_get_prop(data,"default");
	if(keyboard!=NULL)
	{
		int pos=0;
		for(LXmlNode *p=data->child;p!=NULL;p=p->next,pos++)
		{
			if(!strcmp(p->name,keyboard))
			{
				kst.cur=pos;
				kst.sub=0;
				break;
			}
		}
	}
	kbd_main_new();
	return 0;
}

static void btn_free(KBD_BTN *b)
{
	if(!b)
		return;
	l_free(b->data);
	l_free(b);
}

static void line_free(KBD_BTN *b)
{
	l_slist_free(b,(LFreeFunc)btn_free);
}

#define l_xml_get_prop_int(node,name)			\
({												\
 	const char *r=l_xml_get_prop(node,name);	\
	r?atoi(r):0;								\
})

#define l_xml_get_prop_double(node,name,def)	\
({												\
 	const char *r=l_xml_get_prop(node,name);	\
	r?strtod(r,NULL):def;							\
})

#define l_xml_get_prop_color(node,name)			\
 	ui_color_parse(l_xml_get_prop(node,name))

static int kbd_select(int8_t pos,int8_t sub)
{
	double scale;
	
	if(pos==kst.cur && sub==kst.sub && kst.layout.name)
		return 0;
	scale=y_ui_get_scale();
	kst.layout.scale=scale;
	kst.layout.name=NULL;
	kst.layout.psel=NULL;
	l_free(kst.layout.main.data);
	kst.layout.main.data=NULL;
	l_ptr_array_free(kst.layout.line,(LFreeFunc)line_free);
	kst.layout.line=l_ptr_array_new(5);
	ui_font_free(kst.layout.font);
	kst.layout.font=NULL;
	if(pos==-1)
		return 0;
	LXmlNode *root=kst.config->root.child;
	LXmlNode *data=l_xml_get_child(root,"data");
	LXmlNode *layout=l_xml_get_child(root,"layout");
	LXmlNode *keyboard=l_slist_nth(data->child,pos);
	if(!keyboard)
		return -1;
	const char *tag=l_xml_get_prop(keyboard,"layout");
	if(!tag)
		return 0;
	for(layout=layout->child;layout!=NULL;layout=layout->next)
	{
		if(!strcmp(layout->name,tag))
		{
			break;
		}
	}
	if(!layout)
		return 0;
	kst.layout.desc[KBT_MAIN].bg[KBTN_NORMAL]=l_xml_get_prop_color(layout,"bg");
	double layout_scale=l_xml_get_prop_double(layout,"scale",1.0);
	scale*=layout_scale;
	kst.layout.scale=scale;
	int w=l_xml_get_prop_int(layout,"w");
	int h=l_xml_get_prop_int(layout,"h");
	kst.layout.main.rc.w=(short)(scale*w);
	kst.layout.main.rc.h=(short)(scale*h);
	kst.layout.main.type=KBT_MAIN;
	kst.layout.font=ui_font_parse(kst.layout.win,l_xml_get_prop(layout,"font"),scale);

	LXmlNode *button=l_xml_get_child(layout,"key");
	if(!button)
		return -1;
	LXmlNode *color=l_xml_get_child(button,"normal");
	kst.layout.desc[KBT_NORMAL].bg[KBTN_NORMAL]=l_xml_get_prop_color(color,"bg");
	kst.layout.desc[KBT_NORMAL].fg[KBTN_NORMAL]=l_xml_get_prop_color(color,"fg");
	kst.layout.desc[KBT_NORMAL].border[KBTN_NORMAL]=l_xml_get_prop_color(color,"border");
	color=l_xml_get_child(button,"select");
	kst.layout.desc[KBT_NORMAL].bg[KBTN_SELECT]=l_xml_get_prop_color(color,"bg");
	kst.layout.desc[KBT_NORMAL].fg[KBTN_SELECT]=l_xml_get_prop_color(color,"fg");
	kst.layout.desc[KBT_NORMAL].border[KBTN_SELECT]=l_xml_get_prop_color(color,"border");
	
	kst.xim=l_xml_get_prop_int(keyboard,"xim");

	kst.layout.line=l_ptr_array_new(5);
	LXmlNode *rows=l_xml_get_child(layout,"rows");
	if(!rows)
		return -1;
	KBD_BTN *prev=NULL;
	kst.layout.shift=NULL;
	for(LXmlNode *row=rows->child;row!=NULL;row=row->next)
	{
		int y=l_xml_get_prop_int(row,"y")*scale;
		int h=l_xml_get_prop_int(row,"h")*scale;
		if(prev && prev->rc.y+prev->rc.h>y+1)
		{
			for(KBD_BTN *p=prev;p!=NULL;p=p->next)
				p->rc.h=y-p->rc.y+1;
		}
		KBD_BTN *head=NULL;
		prev=NULL;
		for(LXmlNode *key=row->child;key!=NULL;key=key->next)
		{
			int x=l_xml_get_prop_int(key,"x")*scale;
			int w=l_xml_get_prop_int(key,"w")*scale;
			KBD_BTN *btn=l_alloc0(sizeof(KBD_BTN));
			btn->rc.x=(short)x;btn->rc.y=(short)y;
			btn->rc.w=(short)w;btn->rc.h=(short)h;
			if(prev && prev->rc.x+prev->rc.w>x+1)
			{
				prev->rc.w=x-prev->rc.x+1;
			}
			btn->state=KBTN_NORMAL;
			btn->type=KBT_NORMAL;
			head=l_slist_append(head,btn);
			prev=btn;
		}
		l_ptr_array_append(kst.layout.line,head);
		prev=head;
	}
	LXmlNode *sub_keyboard=l_slist_nth(keyboard->child,sub);
	if(!sub_keyboard)
		return -1;
	for(int i=0;i<l_ptr_array_length(kst.layout.line);i++)
	{
		LXmlNode *row=l_slist_nth(sub_keyboard->child,i);
		if(!row)
			break;
		if(!row->data)
			continue;
		char **arr=l_strsplit(row->data,' ');
		KBD_BTN *h=l_ptr_array_nth(kst.layout.line,i);
		for(int j=0;arr[j]!=NULL;j++)
		{
			KBD_BTN *btn=l_slist_nth(h,j);
			if(!btn)
				break;
			char gb[64];
			l_utf8_to_gb(arr[j],gb,sizeof(gb));
			if(!strcmp(gb,"$COMMA"))
				btn->data=l_strdup(",");
			else if(!strcmp(gb,"$NONE"))
				btn->data=NULL;
			else if(!strcmp(gb,"$LSHIFT"))
			{
				kst.layout.shift=btn;
				btn->data=NULL;
			}
			else
				btn->data=l_strdup(YT(gb));

			if(kst.xim && btn->data && btn->data[0]=='$' && kst.layout.key_mods)
			{
				const char *text=btn->data;
				text+=y_im_str_desc(text,0);
				int key=y_im_str_to_key(text+1,NULL);
				if((kst.layout.key_mods&KEYM_CTRL) && key==YK_LCTRL)
				{
					btn->state=KBTN_SELECT;
				}
				else if((kst.layout.key_mods&KEYM_ALT) && key==YK_LALT)
					btn->state=KBTN_SELECT;
			}
		}
		l_strfreev(arr);
	}
	if(kst.layout.shift!=NULL)
	{
		kst.layout.shift->state=l_xml_get_prop_int(sub_keyboard,"select");
		const char *name=l_xml_get_prop(sub_keyboard,"name");
		if(name)
		{
			char gb[64];
			l_utf8_to_gb(name,gb,sizeof(gb));
			kst.layout.shift->data=l_strdup(gb);
		}
		else
		{
			kst.layout.shift->data=l_strdup("Shift");
		}
	}
	kst.layout.name=YT8(l_xml_get_prop(keyboard,"name"));
	if(!kst.layout.name) kst.layout.name="";
#ifdef _WIN32
	{
		WCHAR temp[64];
		RECT rc;
		l_utf8_to_utf16(kst.layout.name,temp,sizeof(temp));
		SetWindowText(kst.layout.win,temp);
		InvalidateRect(kst.layout.win,NULL,TRUE);
		
		int w,h,cx,cy;
		w=GetSystemMetrics(SM_CXSCREEN);
		h=GetSystemMetrics(SM_CYSCREEN);
		cy=GetSystemMetrics(SM_CYCAPTION)+2*GetSystemMetrics(SM_CYFIXEDFRAME);
		cx=2*GetSystemMetrics(SM_CXFIXEDFRAME);
		if(kst.first)
		{
			MoveWindow(kst.layout.win,
				(w-cx-kst.layout.main.rc.w)/2,
				(h-cy-kst.layout.main.rc.h)/2,
				cx+kst.layout.main.rc.w,
				cy+kst.layout.main.rc.h,
				TRUE);
			kst.first=0;
		}
		else
		{
			GetWindowRect(kst.layout.win,&rc);
			MoveWindow(kst.layout.win,
				rc.left,rc.top,
				cx+kst.layout.main.rc.w,
				cy+kst.layout.main.rc.h,
				TRUE);
		}
		GetClientRect(kst.layout.win,&rc);
	}
#else
	gtk_window_set_title(GTK_WINDOW(kst.layout.win),kst.layout.name);
	gtk_widget_queue_draw(kst.layout.win);
	gtk_widget_set_size_request(GTK_WIDGET(kst.layout.canvas),
			kst.layout.main.rc.w,
			kst.layout.main.rc.h);
	if(kst.first)
	{
		if(ui_is_wayland())
		{
			int x,y,w,h;
			ui_get_workarea(&x,&y,&w,&h);
			ybus_wayland_win_move(kst.layout.win,
				x+(w-kst.layout.main.rc.w)/2,
				y+h-kst.layout.main.rc.h);
		}
		else
		{
			gtk_window_set_position(GTK_WINDOW(kst.layout.win),GTK_WIN_POS_CENTER);
		}
		kst.first=0;
	}
#endif
	kst.cur=pos;
	kst.sub=sub;
	return 0;
}

static void kbd_select_sub(void);

int y_kbd_show(int b)
{
	if(!kst.config || !kst.layout.win)
	{
		printf("yong: keyboard no config\n");
		return -1;
	}
	if(b==1 && kst.layout.name)
	{
		kbd_main_show(b);
		return 0;
	}
	if(b==-1)
	{
		if(kst.layout.name) b=0;
		else b=1;
	}
	if(b)
	{
		if(!kbd_select(kst.cur,kst.sub))
		{
			kbd_main_show(b);
		}
	}
	else
	{
		kbd_main_show(b);
		kbd_select(-1,0);
	}
	kst.show=b;
	return 0;
}

#ifdef _WIN32
// delay hide for QQ flicker
static bool pending_hide;
static void kbd_hide_latter(void)
{
	if(!pending_hide)
		return;
	y_kbd_show(0);
	kst.show=1;
	pending_hide=false;
}

int y_kbd_show_with_main(int b)
{
	if(b)
	{
		pending_hide=false;
		if(kst.show)
		{
			y_kbd_show(1);
		}
	}
	else
	{
		if(kst.show)
		{
			if(!pending_hide)
			{
				pending_hide=true;
				y_ui_timer_add(50,(void*)kbd_hide_latter,NULL);
			}
		}
	}
	return 0;
}

#else

int y_kbd_show_with_main(int b)
{
	// 打开软键盘，wayland中目标程序必然失去焦点
	// gnome中拖动窗口时，也会导致目标程序失去焦点
	// 此时这个函数又要求立刻关闭软键盘，导致没有意义
	// linux下只在layer shell时实现这个功能
#if defined(GTK_CHECK_VERSION)
	if(!ui_is_wayland() || !ybus_wayland_get_custom(kst.layout.win))
		return -1;
#endif
	if(b)
	{
		if(kst.show)
		{
			y_kbd_show(1);
		}
	}
	else
	{
		if(kst.show)
		{
			y_kbd_show(0);
			kst.show=1;
		}
	}
	return 0;
}
#endif

static void kbd_paint(void)
{
#ifdef _WIN32
	InvalidateRect(kst.layout.win,NULL,TRUE);
#else
	gtk_widget_queue_draw(kst.layout.win);
#endif
}

void y_kbd_select(int pos,int sub)
{
	if(kst.layout.name)
	{
		if(0!=kbd_select(pos,sub))
			return;
		kbd_paint();
	}
	else
	{
		kst.cur=pos;
		kst.sub=sub;
		y_kbd_show(1);
	}
}

#ifndef _WIN32
static gboolean keyboard_motion_cb(GtkWidget *window,GdkEventMotion *event,gpointer user_data)
{
	gint drag_x=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window),"drag-x"));
	gint drag_y=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window),"drag-y"));
	gint x=(gint) event->x_root;
	gint y=(gint) event->y_root;

	if(x>kst.layout.main.rc.w ||y>kst.layout.main.rc.h)
		return TRUE;

	ybus_wayland_win_move_relative(kst.layout.win,
			x - drag_x,
			y - drag_y);
	return TRUE;
}
#endif

static int kbd_click(int x,int y,int up)
{
	if(!kst.config || !kst.layout.win)
	{
		return -1;
	}
	if(!up)
	{
		int i;
		kst.layout.psel=NULL;
		for(i=0;i<l_ptr_array_length(kst.layout.line);i++)
		{
			KBD_BTN *btn=l_ptr_array_nth(kst.layout.line,i);
			for(;btn;btn=btn->next)
			{
				if(y<=btn->rc.y || y>=btn->rc.h+btn->rc.y)
					break;
				if(x>btn->rc.x && x<btn->rc.x+btn->rc.w)
				{
					kst.layout.psel=btn;
					break;
				}
			}			
		}
	}
	else
	{
		KBD_BTN *btn=kst.layout.psel;
		if(!btn)
		{
			return 0;
		}
		if(x>btn->rc.x && x<btn->rc.x+btn->rc.w &&
				y>btn->rc.y && y<btn->rc.h+btn->rc.y)
		{
			if(btn==kst.layout.shift)
			{
				kbd_select_sub();
			}
			else
			{
				char *text=0;
				int key=-1;
				btn->state=KBTN_NORMAL;
				text=btn->data;
				if(text) text+=y_im_str_desc(text,0);
				if(text && kst.xim)
				{
					if(!strcmp(text,"$_"))
						key=' ';
					else if(text[0]=='$')
						key=y_im_str_to_key(text+1,NULL);
					else if(text[0] && !text[1])
						key=text[0];
					if(key==YK_LCTRL || key==YK_LALT)
					{
						if(key==YK_LCTRL)
						{
							kst.layout.key_mods=l_flip_bits(kst.layout.key_mods,KEYM_CTRL);
							if((kst.layout.key_mods&KEYM_CTRL)!=0)
								btn->state=KBTN_SELECT;
						}
						else if(key==YK_LALT)
						{
							kst.layout.key_mods=l_flip_bits(kst.layout.key_mods,KEYM_ALT);
							if((kst.layout.key_mods&KEYM_ALT)!=0)
								btn->state=KBTN_SELECT;
						}
						text=NULL;
						key=0;
					}
					else if(key && kst.layout.key_mods)
					{
						if(key>='a' && key<='A')
							key=toupper(key);
						key|=kst.layout.key_mods;
						if(kst.layout.shift && kst.layout.shift->rc.w && kst.layout.shift->state==KBTN_SELECT)
							key|=KEYM_SHIFT;
					}
				}
				if((key<=0 || !y_im_input_key(key)) && text)
					y_xim_send_string(text);
			}
		}
		kst.layout.psel=NULL;
	}
	kbd_paint();
	return 0;
}

#ifdef _WIN32
#define IDC_KEYBOARD	1000
HWND GetFocusedWindow(void);
static void kbd_popup_menu(LPtrArray *arr,int cur,void (*cb)(int),int from)
{
	POINT pos;
	static HMENU MainMenu;
	int half=(l_ptr_array_length(arr)+1)/2;
	if(!kst.config || !kst.layout.win)
		return;
	if(MainMenu)
	{
		DestroyMenu(MainMenu);
	}
	MainMenu=CreatePopupMenu();
	for(int i=0;i<l_ptr_array_length(arr);i++)
	{
		const char *name=l_ptr_array_nth(arr,i);
		WCHAR tmp[64];
		l_utf8_to_utf16(name,tmp,sizeof(tmp));
		int flag=MF_STRING;
		if(cur==i) flag|=MF_CHECKED;
		if(i%half==0 && i!=0) flag|=MF_MENUBARBREAK;
		AppendMenu(MainMenu,flag,IDC_KEYBOARD+i,tmp);
		if(i%half!=half-1)
			AppendMenu(MainMenu,MF_SEPARATOR,0,0);
	}
	HWND hWnd=GetFocusedWindow();
	GetCursorPos(&pos);
	UINT uFlags=TPM_RETURNCMD|TPM_NONOTIFY;
	if(from==1)
	{
		RECT rc;
		GetWindowRect(y_ui_main_win(),&rc);
		if(rc.top>300)
		{
			pos.y=rc.top;
			uFlags|=TPM_BOTTOMALIGN;
		}
		else
		{
			pos.y=rc.bottom;
			uFlags|=TPM_TOPALIGN;
		}
	}

	SetForegroundWindow(kst.layout.win);
	int id=TrackPopupMenu(MainMenu,uFlags,pos.x,pos.y,
		0,kst.layout.win,NULL);
	SetForegroundWindow(hWnd);
	if(id>0)
		cb(id-IDC_KEYBOARD);
}
#else
static void (*_menu_click_cb)(int);
static void menu_activate(GtkMenuItem *item,gpointer data)
{
	int id=GPOINTER_TO_INT(data);
	if(!_menu_click_cb)
		return;
	_menu_click_cb(id);
	_menu_click_cb=NULL;
}

static void kbd_popup_menu(LPtrArray *arr,int cur,void (*cb)(int),int from)
{
	static GtkWidget *MainMenu;
	int half=(l_ptr_array_length(arr)+1)/2;
	if(MainMenu)
	{
		gtk_widget_destroy(MainMenu);
	}
	if(ui_is_wayland())
	{
		// gtk-layer-shell在没有显示时，直接显示菜单会导致程序崩溃，所以先显示一下
		if(!gtk_widget_get_visible(kst.layout.win))
		{
			ui_win_show(kst.layout.win,0);
			gtk_widget_show(kst.layout.win);
		}
	}
	_menu_click_cb=cb;
	MainMenu=gtk_menu_new();
	if(from==1)
	{
		if(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(MainWin),"show"))-1<=0)
		{
			ui_win_show(MainWin,1);
		}
		gtk_menu_attach_to_widget(GTK_MENU(MainMenu),MainWin,NULL);
	}
	else
	{
		gtk_menu_attach_to_widget(GTK_MENU(MainMenu),kst.layout.win,NULL);
	}
	for(int i=0;i<l_ptr_array_length(arr);i++)
	{
		const char *name=l_ptr_array_nth(arr,i);
		GtkWidget *item;
		int x,y;
		item=gtk_check_menu_item_new_with_label(name);
		if(cur==i)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),TRUE);
		g_signal_connect(G_OBJECT(item),"activate",
				G_CALLBACK(menu_activate),GINT_TO_POINTER(i));
		y=i%half;x=i/half;
		gtk_menu_attach(GTK_MENU(MainMenu),item,x,x+1,y,y+1);
		gtk_widget_show(item);
	}
	gtk_menu_popup(GTK_MENU(MainMenu),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
}
#endif

static void kbd_menu_select(int id)
{
	if(0==kbd_select(id,0))
	{
		kbd_paint();
		y_kbd_show(1);
	}
}

static void y_kbd_popup_menu_real(int from)
{
	LPtrArray *arr;
	LXmlNode *data=l_xml_get_child(kst.config->root.child,"data");
	if(!data || !data->child)
		return;
	arr=l_ptr_array_new(10);
	for(LXmlNode *keyboard=data->child;keyboard!=NULL;keyboard=keyboard->next)
	{
		const char *name=YT8(l_xml_get_prop(keyboard,"name"));
		if(!name || !name[0])
		{
			break;
		}
		l_ptr_array_append(arr,name);
	}
	kbd_popup_menu(arr,kst.cur,kbd_menu_select,from);
	l_ptr_array_free(arr,NULL);
}

void y_kbd_popup_menu(void)
{
#ifdef _WIN32
	PostMessage(kst.layout.win,WM_USER,0,0);
#else
	y_kbd_popup_menu_real(1);
#endif
}

static void sub_select_cb(int id)
{
	kbd_select(kst.cur,id);
	kbd_paint();
}

static void kbd_select_sub(void)
{
	LXmlNode *node;
	node=l_xml_get_child(kst.config->root.child,"data");
	node=l_slist_nth(node->child,kst.cur);
	int menu=l_xml_get_prop_int(node,"menu");
	LXmlNode *h=node->child;
	node=l_slist_nth(h,kst.sub);
	if(!menu)
	{
		int sub=node->next?kst.sub+1:0;
		kbd_select(kst.cur,sub);
		kbd_paint();
	}
	else
	{
		LPtrArray *arr=l_ptr_array_new(10);
		node=h;
		for(;node!=NULL;node=node->next)
		{
			const char *name=l_xml_get_prop(node,"name");
			l_ptr_array_append(arr,name);
		}
		kbd_popup_menu(arr,kst.sub,sub_select_cb,0);
		l_ptr_array_free(arr,NULL);
	}
}

static void draw_button(DRAW_CONTEXT1 *ctx,KBD_BTN *btn,int which)
{
	int x,y,w,h;
#ifdef _WIN32
	TCHAR temp[64];
#else
	char temp[64];
#endif
	int state=btn->state;

	if(btn==kst.layout.psel) state=KBTN_SELECT;
	if(which==0 || which==1)
	{
		ui_fill_rect(ctx,btn->rc.x,btn->rc.y,btn->rc.w,btn->rc.h,kst.layout.desc[btn->type].bg[state]);
		ui_draw_rect(ctx,btn->rc.x,btn->rc.y,btn->rc.w,btn->rc.h,kst.layout.desc[btn->type].border[state],1);
	}
	if(which==0 || which==2)
	{
		UI_FONT font=kst.layout.font;
		if(btn->data)
			y_im_disp_cand(btn->data,(char*)temp,8,0,NULL,NULL);
		else
			temp[0]=0;
		ui_text_size(ctx->dc,font,temp,&w,&h);
		x=btn->rc.x+(btn->rc.w-w)/2;
		y=btn->rc.y+(btn->rc.h-h)/2;
		ui_draw_text(ctx,font,x,y,temp,kst.layout.desc[btn->type].fg[state]);
	}
}

static void OnKeyboardPaint(DRAW_CONTEXT1 *ctx)
{
	int i;
	KBD_BTN *main=&kst.layout.main;

	ui_fill_rect(ctx,0,0,main->rc.w,main->rc.h,kst.layout.desc[KBT_MAIN].bg[KBTN_NORMAL]);

#ifdef _WIN32
	for(i=0;i<l_ptr_array_length(kst.layout.line);i++)
	{
		KBD_BTN *btn=l_ptr_array_nth(kst.layout.line,i);
		for(;btn;btn=btn->next)
		{
			if(btn->state==KBTN_SELECT)
				continue;
			draw_button(ctx,btn,1);
		}
	}
	for(i=0;i<l_ptr_array_length(kst.layout.line);i++)
	{
		KBD_BTN *btn=l_ptr_array_nth(kst.layout.line,i);
		for(;btn;btn=btn->next)
		{
			if(btn->state!=KBTN_SELECT) continue;
			draw_button(ctx,btn,1);
		}
	}
	ui_draw_text_begin(ctx);
	UI_FONT font=kst.layout.font;
	HANDLE old=NULL;
	if(!font->dw)
		old=SelectObject(ctx->dc,font->gdi);
	for(i=0;i<l_ptr_array_length(kst.layout.line);i++)
	{
		KBD_BTN *btn=l_ptr_array_nth(kst.layout.line,i);
		for(;btn;btn=btn->next)
		{
			if(btn->state==KBTN_SELECT)
				continue;
			draw_button(ctx,btn,2);
		}
	}
	for(i=0;i<l_ptr_array_length(kst.layout.line);i++)
	{
		KBD_BTN *btn=l_ptr_array_nth(kst.layout.line,i);
		for(;btn;btn=btn->next)
		{
			if(btn->state!=KBTN_SELECT) continue;
			draw_button(ctx,btn,2);
		}
	}
	if(!font->dw)
		SelectObject(ctx->dc,old);
	ui_draw_text_end(ctx);
#else
	if(!kst.layout.font)
	{
		return;	
	}
	for(i=0;i<l_ptr_array_length(kst.layout.line);i++)
	{
		KBD_BTN *btn=l_ptr_array_nth(kst.layout.line,i);
		for(;btn;btn=btn->next)
		{
			if(btn->state==KBTN_SELECT)
				continue;
			draw_button(ctx,btn,0);
		}
	}
	for(i=0;i<l_ptr_array_length(kst.layout.line);i++)
	{
		KBD_BTN *btn=l_ptr_array_nth(kst.layout.line,i);
		for(;btn;btn=btn->next)
		{
			if(btn->state!=KBTN_SELECT) continue;
			draw_button(ctx,btn,0);
		}
	}
#endif
}

#ifdef _WIN32
static LRESULT WINAPI kbd_win_proc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg){
	case WM_CREATE:
		break;
	case WM_SYSCOMMAND:
		if(wParam==SC_CLOSE)
		{
			y_kbd_show(0);
		}
		else
		{
			return DefWindowProc(hWnd,msg,wParam,lParam);
		}
		break;
	case WM_CLOSE:
		y_kbd_show(0);
		break;
	case WM_COMMAND:
		if(0==kbd_select(wParam-IDC_KEYBOARD,0))
			y_kbd_show(1);
		break;
	case WM_MOUSEMOVE:
		break;
	case WM_LBUTTONDOWN:
		SetCapture(hWnd);
		kbd_click(LOWORD(lParam),HIWORD(lParam),0);
		break;
	case WM_LBUTTONUP:
		kbd_click(LOWORD(lParam),HIWORD(lParam),1);
		ReleaseCapture();
		break;
	case WM_USER:
		y_kbd_popup_menu_real(1);
		break;
	case WM_RBUTTONUP:
		y_kbd_popup_menu_real(0);
		break;
	case WM_ERASEBKGND:
	{
		return 1;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT rc;
		int w,h;
		HDC hdc,hmem;
		HBITMAP bmp,oldbmp;
		DRAW_CONTEXT1 ctx;
		BITMAPINFO bmi;
		void *bits;
		GetClientRect(hWnd,&rc);
		w=rc.right-rc.left;h=rc.bottom-rc.top;
		hdc=BeginPaint(hWnd,&ps);
		hmem=CreateCompatibleDC(hdc);
		ZeroMemory(&bmi, sizeof(bmi));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,(void **)&bits, NULL, 0);
		oldbmp=SelectObject(hmem,bmp);
		ui_draw_begin(&ctx,hWnd,hmem);
		OnKeyboardPaint(&ctx);
		ui_draw_end(&ctx);
		BitBlt(hdc,0,0,w,h,hmem,0,0,SRCCOPY);
		EndPaint(hWnd,&ps);
		SelectObject(hmem,oldbmp);
		DeleteObject(bmp);
		DeleteDC(hmem);
		break;
	}
	default:
		return DefWindowProc(hWnd,msg,wParam,lParam);
	}
	return 0;
}

int kbd_main_new_real(void)
{
	WNDCLASS wc;
	
	if(!kst.config)
		return 0;

	wc.style=0;
	wc.lpfnWndProc=kbd_win_proc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=GetModuleHandle(0);
	wc.hIcon=0;
	wc.hCursor=LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
	wc.lpszMenuName=NULL;
	wc.lpszClassName=_T("yong_kbd");
	RegisterClass(&wc);
	
	kst.layout.win=CreateWindowEx(WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_APPWINDOW,
		_T("yong_kbd"),_T(""),WS_CAPTION|WS_OVERLAPPED|WS_SYSMENU,
		0,0,0,0,NULL,NULL,GetModuleHandle(NULL),NULL);
		
	return 1;
}

static void kbd_main_new(void)
{
}

static void kbd_main_show(int b)
{
	if(!b)
	{
		ShowWindow(kst.layout.win,SW_HIDE);
	}
	else
	{
		//ShowWindow(kst.layout.win,SW_RESTORE);
		SetWindowPos(kst.layout.win,HWND_TOP,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
	}
}
#else

static gboolean kbd_draw(GtkWidget *window,cairo_t *cr)
{
	DRAW_CONTEXT1 ctx;
	ui_draw_begin(&ctx,window,cr);
	OnKeyboardPaint(&ctx);
	ui_draw_end(&ctx);
	return FALSE;
}

static gboolean kbd_click_cb (GtkWidget *window,GdkEventButton *event,gpointer user_data)
{
	gint click=GPOINTER_TO_INT(user_data);
	if(event->button==3 && click==1)
	{
		y_kbd_popup_menu_real(0);
	}
	else if(event->button==1)
	{
		kbd_click(event->x,event->y,click);
		
		if(ui_is_wayland())
		{
			static guint keyboard_motion_handler;
			if(!kst.layout.psel && click==0 && keyboard_motion_handler==0)
			{
				GdkCursor *cursor=gdk_cursor_new (GDK_FLEUR);
				gdk_window_set_cursor(gtk_widget_get_window(kst.layout.win),cursor);
				g_object_unref (cursor);
				g_object_set_data(G_OBJECT(kst.layout.canvas),"drag-x",GINT_TO_POINTER(event->x_root));
				g_object_set_data(G_OBJECT(kst.layout.canvas),"drag-y",GINT_TO_POINTER(event->y_root));
				keyboard_motion_handler=g_signal_connect(G_OBJECT(kst.layout.canvas),"motion-notify-event",
						G_CALLBACK(keyboard_motion_cb),NULL);
			}
			else if(keyboard_motion_handler)
			{
				g_signal_handler_disconnect (G_OBJECT(kst.layout.canvas),keyboard_motion_handler);
				keyboard_motion_handler=0;
				gdk_window_set_cursor(gtk_widget_get_window(kst.layout.win),NULL);
			}
		}
	}
	return TRUE;
}

static gboolean kbd_hide_cb(void)
{
	y_kbd_show(0);
	return TRUE;
}

static void kbd_main_new(void)
{
	kst.layout.win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_keep_above(GTK_WINDOW(kst.layout.win),TRUE);
	gtk_window_set_modal(GTK_WINDOW(kst.layout.win),FALSE);
	gtk_window_set_resizable(GTK_WINDOW(kst.layout.win),FALSE);
	if(!ui_is_wayland())
	{
		// header bar和layer shell配合有问题
		// GtkWidget *titlebar=gtk_header_bar_new();
		// gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(titlebar),TRUE);
		// gtk_widget_show(titlebar);
		// gtk_window_set_titlebar(GTK_WINDOW(kst.layout.win),titlebar);
	}
	GdkScreen *screen = gdk_screen_get_default();
	GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
	if(visual)
	{
		gtk_widget_set_visual(kst.layout.win,visual);
	}
	if(ui_is_wayland())
	{
		ybus_wayland_set_window(kst.layout.win,"keyboard",2);
		ui_set_css(kst.layout.win,"window{background-color:transparent}\n");
	}
	gtk_widget_realize(kst.layout.win);
	gdk_window_set_functions(gtk_widget_get_window(kst.layout.win),GDK_FUNC_MOVE|GDK_FUNC_MINIMIZE|GDK_FUNC_CLOSE);
	gtk_window_set_accept_focus(GTK_WINDOW(kst.layout.win),FALSE);
	kst.layout.canvas=gtk_drawing_area_new();
	gtk_widget_show(kst.layout.canvas);
	gtk_container_add(GTK_CONTAINER(kst.layout.win),GTK_WIDGET(kst.layout.canvas));
	gtk_widget_add_events(kst.layout.canvas, GDK_EXPOSURE_MASK|GDK_BUTTON_RELEASE_MASK|GDK_BUTTON_PRESS_MASK|GDK_POINTER_MOTION_MASK);
	g_signal_connect(kst.layout.win,"delete-event",
			G_CALLBACK(kbd_hide_cb),NULL);
	g_signal_connect (G_OBJECT(kst.layout.canvas), "button-press-event",
			G_CALLBACK(kbd_click_cb),GINT_TO_POINTER (0));
	g_signal_connect (G_OBJECT(kst.layout.canvas), "button-release-event",
			G_CALLBACK(kbd_click_cb),GINT_TO_POINTER (1));
	g_signal_connect(G_OBJECT(kst.layout.canvas),"draw",
			G_CALLBACK(kbd_draw),NULL);
}

static void kbd_main_show(int b)
{
	if(!b)
	{
		// layer shell下用gtk_widget_hide会导致程序崩溃
		// gtk_widget_hide(kst.layout.win);
		ui_win_show(kst.layout.win,0);
	}
	else
	{
		ui_win_show(kst.layout.win,1);
		gtk_window_deiconify(GTK_WINDOW(kst.layout.win));
	}
}

#endif

