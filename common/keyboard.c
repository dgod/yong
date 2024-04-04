#include "common.h"
#include "ui.h"
#include "ui-draw.h"

#include <math.h>
#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#else
#include <gtk/gtk.h>
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
	KBD_BTN title;
	KBD_BTN shift;
	LPtrArray *line;
	UI_WINDOW win;
	KBD_BTN *psel;
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
	l_free(kst.layout.shift.data);
	kst.layout.shift.data=NULL;
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

	LXmlNode *button=l_xml_get_child(layout,"shift");
	if(!button)
		return 0;
	kst.layout.shift.rc.x=(short)(scale*l_xml_get_prop_int(button,"x"));
	kst.layout.shift.rc.y=(short)(scale*l_xml_get_prop_int(button,"y"));
	kst.layout.shift.rc.w=(short)(scale*l_xml_get_prop_int(button,"w"));
	kst.layout.shift.rc.h=(short)(scale*l_xml_get_prop_int(button,"h"));
	kst.layout.shift.type=KBT_SHIFT;
	LXmlNode *color=l_xml_get_child(button,"normal");
	kst.layout.desc[KBT_SHIFT].bg[KBTN_NORMAL]=l_xml_get_prop_color(color,"bg");
	kst.layout.desc[KBT_SHIFT].fg[KBTN_NORMAL]=l_xml_get_prop_color(color,"fg");
	kst.layout.desc[KBT_SHIFT].border[KBTN_NORMAL]=l_xml_get_prop_color(color,"border");
	color=l_xml_get_child(button,"select");
	kst.layout.desc[KBT_SHIFT].bg[KBTN_SELECT]=l_xml_get_prop_color(color,"bg");
	kst.layout.desc[KBT_SHIFT].fg[KBTN_SELECT]=l_xml_get_prop_color(color,"fg");
	kst.layout.desc[KBT_SHIFT].border[KBTN_SELECT]=l_xml_get_prop_color(color,"border");

	button=l_xml_get_child(layout,"key");
	if(!button)
		return -1;
	color=l_xml_get_child(button,"normal");
	kst.layout.desc[KBT_NORMAL].bg[KBTN_NORMAL]=l_xml_get_prop_color(color,"bg");
	kst.layout.desc[KBT_NORMAL].fg[KBTN_NORMAL]=l_xml_get_prop_color(color,"fg");
	kst.layout.desc[KBT_NORMAL].border[KBTN_NORMAL]=l_xml_get_prop_color(color,"border");
	color=l_xml_get_child(button,"select");
	kst.layout.desc[KBT_NORMAL].bg[KBTN_SELECT]=l_xml_get_prop_color(color,"bg");
	kst.layout.desc[KBT_NORMAL].fg[KBTN_SELECT]=l_xml_get_prop_color(color,"fg");
	kst.layout.desc[KBT_NORMAL].border[KBTN_SELECT]=l_xml_get_prop_color(color,"border");

	kst.layout.line=l_ptr_array_new(5);
	LXmlNode *rows=l_xml_get_child(layout,"rows");
	if(!rows)
		return -1;
	for(LXmlNode *row=rows->child;row!=NULL;row=row->next)
	{
		int y,h;
		y=l_xml_get_prop_int(row,"y");
		h=l_xml_get_prop_int(row,"h");
		KBD_BTN *head=NULL;
		for(LXmlNode *key=row->child;key!=NULL;key=key->next)
		{
			int x,w;
			x=l_xml_get_prop_int(key,"x");
			w=l_xml_get_prop_int(key,"w");
			KBD_BTN *btn=l_alloc0(sizeof(KBD_BTN));
			btn->rc.x=(short)(scale*x);btn->rc.y=(short)(scale*y);
			btn->rc.w=(short)(scale*w);btn->rc.h=(short)(scale*h);
			btn->state=KBTN_NORMAL;
			btn->type=KBT_NORMAL;
			head=l_slist_append(head,btn);
		}
		l_ptr_array_append(kst.layout.line,head);
	}
	LXmlNode *sub_keyboard=l_slist_nth(keyboard->child,sub);
	if(!sub_keyboard)
		return -1;
	kst.layout.shift.state=l_xml_get_prop_int(sub_keyboard,"select");
	const char *name=l_xml_get_prop(sub_keyboard,"name");
	if(name)
	{
		char gb[64];
		l_utf8_to_gb(name,gb,sizeof(gb));
		kst.layout.shift.data=l_strdup(gb);
	}
	else
	{
		kst.layout.shift.data=l_strdup("Shift");
	}
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
			else
				btn->data=l_strdup(gb);
		}
		l_strfreev(arr);
	}
	kst.layout.name=l_xml_get_prop(keyboard,"name");
	if(!kst.layout.name) kst.layout.name="";
	kst.xim=l_xml_get_prop_int(keyboard,"xim");
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
	if(kst.first)
	{
		gtk_window_set_position(GTK_WINDOW(kst.layout.win),GTK_WIN_POS_CENTER);
		kst.first=0;
	}
	gtk_window_resize(GTK_WINDOW(kst.layout.win),
			kst.layout.main.rc.w,
			kst.layout.main.rc.h);
	gtk_widget_set_size_request(GTK_WIDGET(kst.layout.win),
			kst.layout.main.rc.w,
			kst.layout.main.rc.h);
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

int y_kbd_show_with_main(int b)
{
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
		if(!kst.layout.psel)
		{
			KBD_BTN *btn=&kst.layout.shift;
			if(x>btn->rc.x && x<btn->rc.x+btn->rc.w &&
				y>btn->rc.y && y<btn->rc.h+btn->rc.y)
			{
				kst.layout.psel=btn;
			}
		}
	}
	else
	{
		KBD_BTN *btn=kst.layout.psel;
		if(!btn)
			return 0;
		if(x>btn->rc.x && x<btn->rc.x+btn->rc.w &&
				y>btn->rc.y && y<btn->rc.h+btn->rc.y)
		{
			if(btn->type==KBT_SHIFT)
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
				}
				if((key<=0 || !y_xim_input_key(key)) && text)
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
static void kbd_popup_menu(LPtrArray *arr,int cur,void (*cb)(int),int)
{
	static GtkWidget *MainMenu;
	int half=(l_ptr_array_length(arr)+1)/2;
	if(MainMenu)
	{
		gtk_widget_destroy(MainMenu);
	}
	_menu_click_cb=cb;
	MainMenu=gtk_menu_new();
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
		const char *name=l_xml_get_prop(keyboard,"name");
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
		for(int i=0;node!=NULL;i++,node=node->next)
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
	KBD_BTN *sh=&kst.layout.shift;
		
	ui_fill_rect(ctx,0,0,main->rc.w,main->rc.h,kst.layout.desc[KBT_MAIN].bg[KBTN_NORMAL]);

#ifdef _WIN32
	if(sh->rc.w)
	{
		draw_button(ctx,sh,1);
	}
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
	if(sh->rc.w)
	{
		draw_button(ctx,sh,2);
	}
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
	if(sh->rc.w)
	{
		draw_button(ctx,sh,0);
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
	return TRUE;
}

static gboolean kbd_click_cb (GtkWidget *window,GdkEventButton *event,gpointer user_data)
{
	gint click=GPOINTER_TO_INT(user_data);
	if(event->button==3 && click==1)
	{
		y_kbd_popup_menu();
	}
	else if(event->button==1)
	{
		kbd_click(event->x,event->y,click);
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
	GtkWidget *InputBox,*InputEvent;
	kst.layout.win=gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_keep_above(GTK_WINDOW(kst.layout.win),TRUE);
	gtk_window_set_modal(GTK_WINDOW(kst.layout.win),FALSE);
	gtk_window_set_resizable(GTK_WINDOW(kst.layout.win),FALSE);
	gtk_widget_realize(kst.layout.win);
	gdk_window_set_functions(gtk_widget_get_window(kst.layout.win),GDK_FUNC_MOVE|GDK_FUNC_MINIMIZE|GDK_FUNC_CLOSE);
	gtk_window_set_accept_focus(GTK_WINDOW(kst.layout.win),FALSE);
	InputBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_container_add(GTK_CONTAINER(kst.layout.win),GTK_WIDGET(InputBox));
	InputEvent=gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(InputBox),InputEvent);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(InputEvent),FALSE);
	gtk_widget_show(InputEvent);gtk_widget_show(InputBox);
	g_signal_connect(kst.layout.win,"delete-event",
			G_CALLBACK(kbd_hide_cb),NULL);
	g_signal_connect (G_OBJECT(kst.layout.win), "button-press-event",
			G_CALLBACK(kbd_click_cb),GINT_TO_POINTER (0));
	g_signal_connect (G_OBJECT(kst.layout.win), "button-release-event",
			G_CALLBACK(kbd_click_cb),GINT_TO_POINTER (1));
	g_signal_connect(G_OBJECT(kst.layout.win),"draw",
			G_CALLBACK(kbd_draw),0);
}

static void kbd_main_show(int b)
{
	if(!b)
	{
		gtk_widget_hide(kst.layout.win);
	}
	else
	{
		gtk_widget_show(kst.layout.win);
		gtk_window_deiconify(GTK_WINDOW(kst.layout.win));
	}
}

#endif

