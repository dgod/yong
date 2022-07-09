#include "common.h"
#include "ui.h"

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
typedef HWND KBD_WIDGET;
#include "draw.h"
#else
#include <gtk/gtk.h>
typedef GtkWidget *KBD_WIDGET;
#endif
#include <assert.h>

#define FIX_CAIRO_LINETO

/*
[keyboard]
layout=layout0
data=ascii
default=ascii
[layout0]
name=normal
font=Monospace 12
main=w,h #bg tran
shift=x,y,w,h #1,#2 #1,#2 #1,#2
normal=#1,#2 #1,#2 #1,#2
line0=y h x,w ...
[ascii]
name=ascii
layout=layout0
line0=` 1 2 3 4 5 6 7 8 9 0
*/

typedef struct{
	short x;
	short y;
	short w;
	short h;
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
	char bg[2][8];
	char fg[2][8];
	char border[2][8];
}BTN_DESC;

typedef struct{
	void *next;
	char *data[2];
	KBD_RECT rc;
	short type;
	short state;
}KBD_BTN;

typedef struct{
	char *name;
	char *font_desc;
#ifdef _WIN32
	HFONT font;
#else
	PangoLayout *font;
#endif
	int tran;
	double scale;
	BTN_DESC desc[3];
	KBD_BTN main;
	KBD_BTN title;
	KBD_BTN shift;
	KBD_BTN *line[5];
	KBD_WIDGET win;
	KBD_BTN *psel;
}Y_KBD_LAYOUT;

typedef struct{
	char *file;
	LKeyFile *config;
	Y_KBD_LAYOUT layout;
	int cur;
	int xim;
}Y_KBD_STATE;

static Y_KBD_STATE kst;

#ifdef _WIN32
static DWRITE_CONTEXT *draw_ctx;
#endif

static void kbd_main_new(void);
static void kbd_main_show(int b);
int y_kbd_init(const char *fn)
{
	char *line,**list;

	if(getenv("FBTERM_IM_SOCKET"))
		return 0;
	memset(&kst,0,sizeof(kst));
	kst.config=l_key_file_open(fn,0,y_im_get_path("HOME"),y_im_get_path("DATA"),0);
	if(!kst.config) return -1;
	kst.file=l_strdup(fn);
	line=l_key_file_get_string(kst.config,"keyboard","data");
	if(!line)
	{
		l_free(kst.file);
		l_key_file_free(kst.config);
		return -1;
	}
	list=l_strsplit(line,' ');
	l_free(line);
	if(list)
	{
		int i;
		line=y_im_get_config_string("IM","keyboard");
		if(!line) line=l_key_file_get_string(kst.config,"keyboard","default");
		for(i=0;line && list[i];i++)
		{
			if(!strcmp(list[i],line))
			{
				kst.cur=i;
				break;
			}
		}
		l_free(line);
		l_strfreev(list);
	}
	kbd_main_new();
	return 0;
}

#ifndef _WIN32
static PangoLayout *font_parse(char *font)
{
	PangoFontDescription *desc;
	PangoLayout *layout;

	desc=pango_font_description_from_string(font);
	assert(desc!=NULL);

#if GTK_CHECK_VERSION(3,0,0)
	cairo_t *cr=gdk_cairo_create(gtk_widget_get_window(kst.layout.win));
#else
	cairo_t *cr=gdk_cairo_create(kst.layout.win->window);
#endif
	layout=pango_cairo_create_layout(cr);
	cairo_destroy(cr);

	assert(layout);
	pango_layout_set_font_description(layout,desc);
	pango_font_description_free(desc);
	return layout;
}

#if GTK_CHECK_VERSION(3,0,0)
static void DrawText(cairo_t *cr,char *text,KBD_RECT *rc,char *color)
{
	GdkRGBA clr;
	int x,y,w,h;
	PangoLayout *layout=kst.layout.font;
	pango_layout_set_text (layout, text, -1);
	pango_layout_get_pixel_size(layout,&w,&h);
	x=rc->x+(rc->w-w)/2;
	y=rc->y+(rc->h-h)/2;
	cairo_move_to(cr,x,y);
	gdk_rgba_parse(&clr,color);
	gdk_cairo_set_source_rgba(cr,&clr);
	pango_cairo_show_layout(cr,layout);
}
#else
static void DrawText(cairo_t *cr,char *text,KBD_RECT *rc,char *color)
{
	GdkColor clr;
	int x,y,w,h;
	PangoLayout *layout=kst.layout.font;
	pango_layout_set_text (layout, text, -1);
	pango_layout_get_pixel_size(layout,&w,&h);
	x=rc->x+(rc->w-w)/2;
	y=rc->y+(rc->h-h)/2;
	cairo_move_to(cr,x,y);
	gdk_color_parse(color,&clr);
	gdk_cairo_set_source_color(cr,&clr);
	pango_cairo_show_layout(cr,layout);
}
#endif

#else

static HFONT font_parse(char *s)
{
	int size;
	char face[96];
	HDC hdc;
	HFONT ret;
	int res;
	int weight;
	LOGFONT lf;
	char **list;
	
	list=l_strsplit(s,' ');
	res=l_strv_length(list);
	if(res>=2 && res<=6)
	{
		int i,len;
		size=atoi(list[res-1]);
		if(!stricmp(list[res-2],"Bold"))
		{
			weight=FW_BOLD;
			len=res-2;
		}
		else
		{
			weight=FW_REGULAR;
			len=res-1;
		}
		for(face[0]=0,i=0;i<len;i++)
		{
			strcat(face,list[i]);
			if(i!=len-1) strcat(face," ");
		}
	}
	else
	{
		size=12;
		weight=FW_REGULAR;
		strcpy(face,"Fixedsys");
	}
	l_strfreev(list);
	
	hdc=GetDC(NULL);
	size = -(size*GetDeviceCaps(hdc, LOGPIXELSY)/72);
	ReleaseDC(NULL,hdc);
	memset(&lf,0,sizeof(lf));
	lf.lfHeight=size;
	lf.lfWeight=weight;
	lf.lfCharSet=DEFAULT_CHARSET;
	lf.lfOutPrecision=CLIP_DEFAULT_PRECIS;
	lf.lfClipPrecision=CLIP_DEFAULT_PRECIS;
	lf.lfQuality=DEFAULT_QUALITY;
	lf.lfPitchAndFamily=DEFAULT_PITCH;
	l_utf8_to_utf16(face,lf.lfFaceName,sizeof(lf.lfFaceName));
	ret=CreateFontIndirect(&lf);
	return ret;
}

#endif

static int kbd_select(int pos)
{
	int i;
	KBD_BTN *btn;
	char *layout;
	char *data;
	char *line;
	char **list;
	int len;
	int x,y,w,h;
	int ret;
	double scale;
	
	if(pos==kst.cur && kst.layout.name)
		return 0;
		
	scale=y_ui_get_scale();
	kst.layout.scale=scale;
	l_free(kst.layout.name);
	kst.layout.name=0;
	kst.layout.psel=0;
	l_free(kst.layout.main.data[0]);
	kst.layout.main.data[0]=0;
	l_free(kst.layout.main.data[1]);
	kst.layout.main.data[1]=0;
	l_free(kst.layout.shift.data[0]);
	kst.layout.shift.data[0]=0;
	l_free(kst.layout.shift.data[1]);
	kst.layout.shift.data[1]=0;
	for(i=0;i<5;i++)
	{
		KBD_BTN *next;
		for(btn=kst.layout.line[i];btn!=0;btn=next)
		{
			next=btn->next;
			l_free(btn->data[0]);
			l_free(btn->data[1]);
			l_free(btn);
		}
		kst.layout.line[i]=0;
	}
	line=l_key_file_get_string(kst.config,"keyboard","data");
	if(!line)
	{
		printf("yong: keyboard config have no data config\n");
		return -1;
	}
	list=l_strsplit(line,' ');
	l_free(line);
	if(!list)
	{
		printf("yong: keyboard data line bad\n");
		return -1;
	}
	len=l_strv_length(list);
	if(pos<0 || pos>=len)
	{
		l_strfreev(list);
		//printf("yong: keyboard select not in range\n");
		return 0;
	}
	data=l_strdup(list[pos]);
	l_strfreev(list);
	layout=l_key_file_get_string(kst.config,data,"layout");
	if(!layout)
	{
		printf("yong: no layout of %s found\n",data);
		l_free(data);
		return -1;
	}

	/* main button desc */
	line=l_key_file_get_string(kst.config,layout,"main");
	if(!line)
	{
		printf("yong: keyboard can't found main of layout %s\n",layout);
		l_free(data);
		l_free(layout);
		return -1;
	}
	ret=l_sscanf(line,"%d,%d %s %d",&w,&h,
		kst.layout.desc[KBT_MAIN].bg[KBTN_NORMAL],
		&kst.layout.tran);
	l_free(line);
	if(ret!=4)
	{
		l_free(data);
		l_free(layout);
		printf("yong: keyboard can't get main desc\n");
		return -1;
	}
	kst.layout.main.rc.w=(short)(scale*w);
	kst.layout.main.rc.h=(short)(scale*h);
	kst.layout.main.type=KBT_MAIN;
	kst.layout.main.state=KBTN_NORMAL;
	
	/* shift button desc */
	line=l_key_file_get_string(kst.config,layout,"shift");
	if(!line)
	{
		l_free(data);
		l_free(layout);
		printf("yong: keyboard can't get shift desc\n");
		return -1;
	}
	for(i=0;line[i];i++) if(line[i]==',') line[i]=' ';
	ret=l_sscanf(line,"%d %d %d %d %s %s %s %s %s %s",
		&x,&y,&w,&h,
		kst.layout.desc[KBT_SHIFT].bg[KBTN_NORMAL],
		kst.layout.desc[KBT_SHIFT].bg[KBTN_SELECT],
		kst.layout.desc[KBT_SHIFT].fg[KBTN_NORMAL],
		kst.layout.desc[KBT_SHIFT].fg[KBTN_SELECT],
		kst.layout.desc[KBT_SHIFT].border[KBTN_NORMAL],
		kst.layout.desc[KBT_SHIFT].border[KBTN_SELECT]);
	l_free(line);
	if(ret!=10)
	{
		l_free(data);
		l_free(layout);
		printf("yong: keyboard can't get shift desc\n");
		return -1;
	}
	line=l_key_file_get_string(kst.config,data,"shift");
	if(line)
	{
		char gb[8];
		l_utf8_to_gb(line,gb,8);
		l_free(line);
		kst.layout.shift.data[0]=l_strdup(gb);
	}
	else
	{
		kst.layout.shift.data[0]=l_strdup("");
	}
	
	kst.layout.shift.rc.x=(short)(scale*x);
	kst.layout.shift.rc.y=(short)(scale*y);
	kst.layout.shift.rc.w=(short)(scale*w);
	kst.layout.shift.rc.h=(short)(scale*h);
	kst.layout.shift.type=KBT_SHIFT;
	kst.layout.shift.state=KBTN_NORMAL;
	
	/* normal button desc */
	line=l_key_file_get_string(kst.config,layout,"normal");
	if(!line)
	{
		l_free(data);
		l_free(layout);
		printf("yong: keyboard can't get normal desc\n");
		return -1;
	}
	for(i=0;line[i];i++) if(line[i]==',') line[i]=' ';
	ret=l_sscanf(line,"%s %s %s %s %s %s",
		kst.layout.desc[KBT_NORMAL].bg[KBTN_NORMAL],
		kst.layout.desc[KBT_NORMAL].bg[KBTN_SELECT],
		kst.layout.desc[KBT_NORMAL].fg[KBTN_NORMAL],
		kst.layout.desc[KBT_NORMAL].fg[KBTN_SELECT],
		kst.layout.desc[KBT_NORMAL].border[KBTN_NORMAL],
		kst.layout.desc[KBT_NORMAL].border[KBTN_SELECT]);
	l_free(line);
	if(ret!=6)
	{
		l_free(data);
		l_free(layout);
		printf("yong: keyboard can't get normal desc\n");
		return -1;
	}

	for(i=0;i<5;i++)
	{
		char llll[8]="line0";
		int j;
		llll[4]='0'+i;
		line=l_key_file_get_string(kst.config,layout,llll);
		if(!line) continue;
		list=l_strsplit(line,' ');
		l_free(line);
		if(!list) continue;
		len=l_strv_length(list);
		if(len<=2)
		{
			l_strfreev(list);
			continue;
		}
		y=atoi(list[0]);h=atoi(list[1]);
		for(j=2;j<len;j++)
		{
			KBD_BTN *btn=l_alloc0(sizeof(KBD_BTN));
			ret=l_sscanf(list[j],"%d,%d",&x,&w);
			if(ret!=2)
			{
				l_free(btn);
				l_strfreev(list);
				l_free(layout);
				l_free(data);
				return -1;
			}
			btn->rc.x=(short)(scale*x);btn->rc.y=(short)(scale*y);
			btn->rc.w=(short)(scale*w);btn->rc.h=(short)(scale*h);
			btn->state=KBTN_NORMAL;
			btn->type=KBT_NORMAL;
			kst.layout.line[i]=l_slist_append(kst.layout.line[i],btn);
		}
		l_strfreev(list);
	}
	line=l_key_file_get_string(kst.config,layout,"font");
	if(!line) line=l_strdup("Monosapce 12");
	if(kst.layout.font)
	{
#ifdef _WIN32
		DeleteObject(kst.layout.font);
#else
		g_object_unref(kst.layout.font);
#endif
		l_free(kst.layout.font_desc);
	}
	kst.layout.font=font_parse(line);
	kst.layout.font_desc=line;
	l_free(layout);
	
	/* load button data */
	for(i=0;i<5;i++)
	{
		char llll[8]="line0";
		int j;
		KBD_BTN *btn;
		if(!kst.layout.line[i]) continue;
		llll[4]='0'+i;
		line=l_key_file_get_string(kst.config,data,llll);
		if(!line) continue;
		list=l_strsplit(line,' ');
		l_free(line);
		if(!list) continue;
		for(j=0,btn=kst.layout.line[i];list[j] && btn!=0;btn=btn->next,j++)
		{
			char **tmp;
			char gb[64];
			tmp=l_strsplit(list[j],',');
			if(!tmp) continue;
			if(!tmp[0])
			{
				l_strfreev(tmp);
				continue;
			}
			l_utf8_to_gb(tmp[0],gb,sizeof(gb));
			if(!strcmp(gb,"$COMMA"))
				btn->data[0]=l_strdup(",");
			else if(!strcmp(gb,"$NONE"))
				btn->data[0]=0;
			else
				btn->data[0]=l_strdup(gb);
			if(tmp[1])
			{
				l_utf8_to_gb(tmp[1],gb,sizeof(gb));
				if(!strcmp(gb,"$COMMA"))
					btn->data[0]=l_strdup(",");
				else if(!strcmp(gb,"$NONE"))
					btn->data[0]=0;
				btn->data[1]=l_strdup(gb);
			}
			l_strfreev(tmp);
		}
		l_strfreev(list);
	}
	kst.layout.name=l_key_file_get_string(kst.config,data,"name");
	if(!kst.layout.name) kst.layout.name=l_strdup("");
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
		MoveWindow(kst.layout.win,
				(w-cx-kst.layout.main.rc.w)/2,
				(h-cy-kst.layout.main.rc.h)/2,
				cx+kst.layout.main.rc.w,
				cy+kst.layout.main.rc.h,
				TRUE);
		GetClientRect(kst.layout.win,&rc);
	}
#else
	gtk_window_set_title(GTK_WINDOW(kst.layout.win),kst.layout.name);
	gtk_widget_queue_draw(kst.layout.win);
	gtk_window_set_position(GTK_WINDOW(kst.layout.win),GTK_WIN_POS_CENTER);
	gtk_window_resize(GTK_WINDOW(kst.layout.win),
			kst.layout.main.rc.w,
			kst.layout.main.rc.h);
	gtk_widget_set_size_request(GTK_WIDGET(kst.layout.win),
			kst.layout.main.rc.w,
			kst.layout.main.rc.h);
#endif
	kst.xim=l_key_file_get_int(kst.config,data,"xim");
	l_free(data);
	kst.cur=pos;
	
	return 0;
}

int y_kbd_show(int b)
{
	if(!kst.file || !kst.config || !kst.layout.win)
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
		if(!kbd_select(kst.cur))
		{
			kbd_main_show(b);
		}
	}
	else
	{
		kbd_main_show(b);
		kbd_select(-1);
	}
	return 0;
}

static int kbd_click(int x,int y,int up)
{
	if(!kst.file || !kst.config || !kst.layout.win)
	{
		return -1;
	}
	if(!up)
	{
		int i;
		kst.layout.psel=0;
		
		for(i=0;i<5;i++)
		{
			KBD_BTN *btn=kst.layout.line[i];
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
				if(btn->state==KBTN_NORMAL)
					btn->state=KBTN_SELECT;
				else
					btn->state=KBTN_NORMAL;
			}
			else
			{
				int sh=kst.layout.shift.state==KBTN_SELECT;
				char *text=0;
				int key=-1;
				btn->state=KBTN_NORMAL;
				if(btn->data[sh])
					text=btn->data[sh];
				else if(btn->data[0])
					text=btn->data[0];
				if(text) text+=y_im_str_desc(text,0);
				if(text && kst.xim)
				{
					if(!strcmp(text,"$_"))
						key=' ';
					else if(text[0]=='$')
						key=y_im_str_to_key(text+1);
					else if(text[0] && !text[1])
						key=text[0];
				}
				if((key<=0 || !y_xim_input_key(key)) && text)
					y_xim_send_string(text);
			}
		}
		kst.layout.psel=0;
	}
#ifdef _WIN32
	InvalidateRect(kst.layout.win,NULL,TRUE);
#else
	gtk_widget_queue_draw(kst.layout.win);
#endif
	return 0;
}

#ifndef _WIN32
static void menu_activate(GtkMenuItem *item,gpointer data)
{
	int id=GPOINTER_TO_INT(data);
	if(0==kbd_select(id))
		y_kbd_show(1);
}
#else
#define IDC_KEYBOARD	1000
HWND GetFocusedWindow(void);
#endif

static void y_kbd_popup_menu_real(void)
{
#ifdef _WIN32
	POINT pos;
	static HMENU MainMenu;
#else
	static GtkWidget *MainMenu;
#endif
	char *line,**list;
	int i;
	
	if(!kst.file || !kst.config || !kst.layout.win)
		return;
		
	if(MainMenu)
	{
#ifdef _WIN32
		DestroyMenu(MainMenu);
#else
		gtk_widget_destroy(MainMenu);
#endif
		MainMenu=0;
	}
	line=l_key_file_get_string(kst.config,"keyboard","data");
	if(!line) return;
	list=l_strsplit(line,' ');
	l_free(line);
	if(!list || !list[0])
	{
		if(list) l_strfreev(list);
		return;
	}
#ifdef _WIN32
	MainMenu=CreatePopupMenu();
#else
	MainMenu=gtk_menu_new();
#endif
	for(i=0;list[i];i++)
	{
		char *name=l_key_file_get_string(kst.config,list[i],"name");
		if(!name || !name[0])
		{
			l_free(name);
			break;
		}
#ifdef _WIN32
		WCHAR tmp[64];
		l_utf8_to_utf16(name,tmp,sizeof(tmp));
		l_free(name);
		int flag=MF_STRING;
		if(kst.cur==i) flag|=MF_CHECKED;
		if(list[i+1] && i%7==0 && i!=0) flag|=MF_MENUBARBREAK;
		AppendMenu(MainMenu,flag,IDC_KEYBOARD+i,tmp);
		if(i%7!=6)
			AppendMenu(MainMenu,MF_SEPARATOR,0,0);
#else
		GtkWidget *item;
		int x,y;
		item=gtk_check_menu_item_new_with_label(name);
		l_free(name);
		if(kst.cur==i)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),TRUE);
		g_signal_connect(G_OBJECT(item),"activate",
				G_CALLBACK(menu_activate),GINT_TO_POINTER(i));
		y=i%7;x=i/7;
		gtk_menu_attach(GTK_MENU(MainMenu),item,x,x+1,y,y+1);
		gtk_widget_show(item);
#endif
	}
	l_strfreev(list);
#ifdef _WIN32
	HWND hWnd=GetFocusedWindow();
	GetCursorPos(&pos);
	SetForegroundWindow(kst.layout.win);
	PostMessage(kst.layout.win,WM_USER,0,0);
	int id=TrackPopupMenu(MainMenu,TPM_RETURNCMD|TPM_NONOTIFY,pos.x,pos.y,
		0,kst.layout.win,NULL);
	if(id>0)
		PostMessage(kst.layout.win,WM_COMMAND,id,0);
#ifndef _WIN32_WCE
	SetForegroundWindow(hWnd);
#endif
#else
	gtk_menu_popup(GTK_MENU(MainMenu),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
#endif
}

void y_kbd_popup_menu(void)
{
#ifdef _WIN32
	PostMessage(kst.layout.win,WM_RBUTTONUP,0,0);
#else
	y_kbd_popup_menu_real();
#endif
}

#ifdef _WIN32
static COLORREF color_parse(char *color)
{
	COLORREF ret;
	char tmp[7];
	tmp[0]=color[5];tmp[1]=color[6];
	tmp[2]=color[3];tmp[3]=color[4];
	tmp[4]=color[1];tmp[5]=color[2];
	tmp[6]=0;
	ret=strtoul(tmp,0,16);
	return ret;
}

static HBRUSH brush_parse(char *color)
{
	COLORREF clr;
	if(color[0]!='#')
		return CreateSolidBrush(RGB(255,255,255));
	clr=color_parse(color);
	return CreateSolidBrush(clr);
}

static HPEN pen_parse(char *color)
{
	COLORREF clr;
	if(color[0]!='#')
		return CreatePen(PS_SOLID,1,RGB(0,0,0));
	clr=color_parse(color);
	return CreatePen(PS_SOLID,1,clr);
}

static void draw_button(HDC hdc,KBD_BTN *btn,int sh)
{
	RECT rc;
	HGDIOBJ hPenOld,hBrushOld,hPen,hBrush;
	TCHAR temp[64];
	int state=btn->state;
	COLORREF clr;
	
	if(btn==kst.layout.psel) state=KBTN_SELECT;
	rc.left=btn->rc.x;rc.top=btn->rc.y;
	rc.right=btn->rc.x+btn->rc.w;
	rc.bottom=btn->rc.y+btn->rc.h;
	hBrush=brush_parse(kst.layout.desc[btn->type].bg[state]);
	hPen=pen_parse(kst.layout.desc[btn->type].border[state]);
	hBrushOld=SelectObject(hdc,hBrush);
	hPenOld=SelectObject(hdc,hPen);
	Rectangle(hdc,rc.left,rc.top,rc.right,rc.bottom);
	SelectObject(hdc,hPenOld);
	DeleteObject(hPen);
	clr=color_parse(kst.layout.desc[btn->type].fg[state]);
	SetTextColor(hdc,clr);
	if(btn->data[1] && sh)
		y_im_disp_cand(btn->data[1],(char*)temp,8,0);		
	else if(btn->data[0])
		y_im_disp_cand(btn->data[0],(char*)temp,8,0);
	else
		temp[0]=0;
	DrawText(hdc,temp,-1,&rc,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
	SelectObject(hdc,hBrushOld);
	DeleteObject(hBrush);
}

static void OnKeyboardPaint(HDC hdc,int w,int h)
{
	KBD_BTN *sh=&kst.layout.shift;
	HGDIOBJ hPenOld,hBrushOld,hPen,hBrush,hFontOld;
	int i;
		
	SetBkMode(hdc,TRANSPARENT);
	
	hBrush=brush_parse(kst.layout.desc[KBT_MAIN].bg[KBTN_NORMAL]);
	hPen=pen_parse(kst.layout.desc[KBT_MAIN].bg[KBTN_NORMAL]);
	hBrushOld=SelectObject(hdc,hBrush);
	hPenOld=SelectObject(hdc,hPen);
	Rectangle(hdc,0,0,w,h);
	SelectObject(hdc,hBrushOld);
	SelectObject(hdc,hPenOld);
	DeleteObject(hBrush);
	DeleteObject(hPen);
	
	hFontOld=SelectObject(hdc,kst.layout.font);

	if(sh->rc.w)
	{
		draw_button(hdc,sh,0);
	}
	for(i=0;i<5;i++)
	{
		KBD_BTN *btn=kst.layout.line[i];
		for(;btn;btn=btn->next)
		{
			if(sh->state==KBTN_SELECT) continue;
			draw_button(hdc,btn,0);
		}
	}
	for(i=0;i<5;i++)
	{
		KBD_BTN *btn=kst.layout.line[i];
		for(;btn;btn=btn->next)
		{
			if(sh->state!=KBTN_SELECT) continue;
			draw_button(hdc,btn,1);
		}
	}
	SelectObject(hdc,hFontOld);
}

static void draw_button_draw(DWRITE_CONTEXT *ctx,KBD_BTN *btn,int sh)
{
	RECT rc;
	void *hPen,*hBrush,*clr;	
	TCHAR temp[64];
	int state=btn->state;
	
	if(btn==kst.layout.psel) state=KBTN_SELECT;
	rc.left=btn->rc.x;rc.top=btn->rc.y;
	rc.right=btn->rc.x+btn->rc.w;
	rc.bottom=btn->rc.y+btn->rc.h;
	hBrush=dw_brush_parse(ctx,kst.layout.desc[btn->type].bg[state]);
	hPen=dw_brush_parse(ctx,kst.layout.desc[btn->type].border[state]);
	dw_select_brush(ctx,hBrush);
	dw_select_pen(ctx,hPen);
	dw_fill_rect(ctx,btn->rc.x,btn->rc.y,btn->rc.w,btn->rc.h);
	dw_draw_rect(ctx,btn->rc.x,btn->rc.y,btn->rc.w,btn->rc.h);
	clr=dw_brush_parse(ctx,kst.layout.desc[btn->type].fg[state]);
	if(btn->data[1] && sh)
		y_im_disp_cand(btn->data[1],(char*)temp,8,0);		
	else if(btn->data[0])
		y_im_disp_cand(btn->data[0],(char*)temp,8,0);
	else
		temp[0]=0;
	dw_draw_text2(ctx,temp,&rc,clr);
	dw_object_delete(hPen);
	dw_object_delete(hBrush);
	dw_object_delete(clr);

}

static void OnKeyboardPaint_draw(DWRITE_CONTEXT *ctx,int w,int h)
{
	KBD_BTN *sh=&kst.layout.shift;
	void *hPen,*hBrush,*hFont;
	int i;

	hFont=dw_font_parse(ctx,kst.layout.font_desc);
	hBrush=dw_brush_parse(ctx,kst.layout.desc[KBT_MAIN].bg[KBTN_NORMAL]);
	hPen=dw_brush_parse(ctx,kst.layout.desc[KBT_MAIN].bg[KBTN_NORMAL]);
	dw_select_font(ctx,hFont);
	dw_select_brush(ctx,hBrush);
	dw_select_pen(ctx,hPen);
	dw_fill_rect(ctx,0,0,w,h);
	dw_draw_rect(ctx,0,0,w,h);
	
	if(sh->rc.w)
	{
		draw_button_draw(ctx,sh,0);
	}
	for(i=0;i<5;i++)
	{
		KBD_BTN *btn=kst.layout.line[i];
		for(;btn;btn=btn->next)
		{
			if(sh->state==KBTN_SELECT) continue;
			draw_button_draw(ctx,btn,0);
		}
	}
	for(i=0;i<5;i++)
	{
		KBD_BTN *btn=kst.layout.line[i];
		for(;btn;btn=btn->next)
		{
			if(sh->state!=KBTN_SELECT) continue;
			draw_button_draw(ctx,btn,1);
		}
	}
	
	dw_object_delete(hPen);
	dw_object_delete(hBrush);
	dw_object_delete(hFont);
}

BOOL CALLBACK EnumMenuWin(HWND hWnd ,LPARAM lParam)
{
	char name[64];
	GetClassNameA(hWnd,name,sizeof(name));
	//printf("%p %s\n",hWnd,name);
	//if(!strcmp(name,"#32768"))
	//	SetActiveWindow(hWnd);
	return TRUE;
}

LRESULT WINAPI kbd_win_proc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg){
	case WM_CREATE:
		if(l_key_file_get_data(kst.config,"keyboard","color") && l_key_file_get_int(kst.config,"keyboard","color")==0)
			break;
		draw_ctx=dw_context_new();
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
		if(0==kbd_select(wParam-IDC_KEYBOARD))
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
	case WM_RBUTTONUP:
		y_kbd_popup_menu_real();
		break;
	case WM_ERASEBKGND:
	{
		return 1;
	}
	case WM_USER:
	{
		EnumThreadWindows(GetCurrentThreadId(),EnumMenuWin,0);
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT rc;
		HDC hdc;
		int w,h;
		GetClientRect(hWnd,&rc);
		w=rc.right-rc.left;h=rc.bottom-rc.top;
		hdc=BeginPaint(hWnd,&ps);
		if(draw_ctx)
		{
			dw_begin_paint(draw_ctx,hWnd,hdc);
			OnKeyboardPaint_draw(draw_ctx,w,h);
			dw_end_paint(draw_ctx);
		}
		else
		{		
			HBITMAP bmp,oldbmp;			
			HDC hmem=CreateCompatibleDC(hdc);			
			bmp=CreateCompatibleBitmap(hdc,w,h);
			oldbmp=SelectObject(hmem,bmp);
			OnKeyboardPaint(hmem,w,h);
			BitBlt(hdc,0,0,w,h,hmem,0,0,SRCCOPY);
			SelectObject(hmem,oldbmp);
			DeleteObject(bmp);
			DeleteDC(hmem);
		}
		EndPaint(hWnd,&ps);
		break;
	}
	case WM_DESTROY:
		dw_context_free(draw_ctx);
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
		0,0,0,0,0,0,GetModuleHandle(0),0);
		
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

static void draw_button(cairo_t *cr,KBD_BTN *btn,int sh)
{
	char temp[64];
	int state=btn->state;
#if GTK_CHECK_VERSION(3,0,0)
	GdkRGBA clr;
#else
	GdkColor clr;
#endif

	if(btn==kst.layout.psel) state=KBTN_SELECT;

#if GTK_CHECK_VERSION(3,0,0)
	gdk_rgba_parse(&clr,kst.layout.desc[btn->type].bg[state]);
	gdk_cairo_set_source_rgba(cr,&clr);
#else
	gdk_color_parse(kst.layout.desc[btn->type].bg[state],&clr);
	gdk_cairo_set_source_color(cr,&clr);
#endif
#ifdef FIX_CAIRO_LINETO
	cairo_rectangle(cr,btn->rc.x+0.5,btn->rc.y+0.5,btn->rc.w,btn->rc.h);
#else
	cairo_rectangle(cr,btn->rc.x,btn->rc.y,btn->rc.w,btn->rc.h);
#endif
	cairo_fill(cr);
#if GTK_CHECK_VERSION(3,0,0)
	gdk_rgba_parse(&clr,kst.layout.desc[btn->type].border[state]);
	gdk_cairo_set_source_rgba(cr,&clr);
#else
	gdk_color_parse(kst.layout.desc[btn->type].border[state],&clr);
	gdk_cairo_set_source_color(cr,&clr);
#endif
#ifdef FIX_CAIRO_LINETO
	cairo_rectangle(cr,btn->rc.x+0.5,btn->rc.y+0.5,btn->rc.w-1,btn->rc.h-1);
#else
	cairo_rectangle(cr,btn->rc.x,btn->rc.y,btn->rc.w,btn->rc.h);
#endif
	cairo_stroke(cr);

	if(btn->data[1] && sh)
		y_im_disp_cand(btn->data[1],(char*)temp,8,0);		
	else if(btn->data[0])
		y_im_disp_cand(btn->data[0],(char*)temp,8,0);
	else
		temp[0]=0;
	DrawText(cr,temp,&btn->rc,kst.layout.desc[btn->type].fg[state]);
}

#if GTK_CHECK_VERSION(3,0,0)
static gboolean kbd_draw(GtkWidget *window,cairo_t *cr)
{
	KBD_BTN *sh=&kst.layout.shift;
	KBD_BTN *btn;
	GdkRGBA clr;
	int i;
	
	cairo_set_line_width(cr,1.0);
	cairo_set_antialias(cr,CAIRO_ANTIALIAS_NONE);
	
	btn=&kst.layout.main;
	gdk_rgba_parse(&clr,kst.layout.desc[btn->type].bg[btn->state]);
	gdk_cairo_set_source_rgba(cr,&clr);
	cairo_rectangle(cr,0,0,btn->rc.w,btn->rc.h);
	cairo_fill(cr);	
	
	if(sh->rc.w)
	{
		draw_button(cr,sh,0);
	}

	for(i=0;i<5;i++)
	{
		KBD_BTN *btn=kst.layout.line[i];
		for(;btn;btn=btn->next)
		{
			if(sh->state==KBTN_SELECT) continue;
			draw_button(cr,btn,0);
		}
	}
	for(i=0;i<5;i++)
	{
		KBD_BTN *btn=kst.layout.line[i];
		for(;btn;btn=btn->next)
		{
			if(sh->state!=KBTN_SELECT) continue;
			draw_button(cr,btn,1);
		}
	}
	return TRUE;
}
#else
static gboolean kbd_expose(GtkWidget *window,GdkEventExpose *event)
{
	KBD_BTN *sh=&kst.layout.shift;
#if defined(GSEAL_ENABLE)
	cairo_t *cr=gdk_cairo_create(gtk_widget_get_window(window));
#else
	cairo_t *cr=gdk_cairo_create(window->window);
#endif
	KBD_BTN *btn;
	GdkColor clr;
	int i;
	
	cairo_set_line_width(cr,1.0);
	cairo_set_antialias(cr,CAIRO_ANTIALIAS_NONE);
	
	btn=&kst.layout.main;
	gdk_color_parse(kst.layout.desc[btn->type].bg[btn->state],&clr);
	gdk_cairo_set_source_color(cr,&clr);
	cairo_rectangle(cr,0,0,btn->rc.w,btn->rc.h);
	cairo_fill(cr);	
	
	if(sh->rc.w)
	{
		draw_button(cr,sh,0);
	}

	for(i=0;i<5;i++)
	{
		KBD_BTN *btn=kst.layout.line[i];
		for(;btn;btn=btn->next)
		{
			if(sh->state==KBTN_SELECT) continue;
			draw_button(cr,btn,0);
		}
	}
	for(i=0;i<5;i++)
	{
		KBD_BTN *btn=kst.layout.line[i];
		for(;btn;btn=btn->next)
		{
			if(sh->state!=KBTN_SELECT) continue;
			draw_button(cr,btn,1);
		}
	}

	cairo_destroy(cr);
	return TRUE;
}
#endif

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
	gtk_widget_realize(kst.layout.win);
#if GTK_CHECK_VERSION(3,0,0)
	gdk_window_set_functions(gtk_widget_get_window(kst.layout.win),GDK_FUNC_MOVE|GDK_FUNC_MINIMIZE|GDK_FUNC_CLOSE);
#else
	gdk_window_set_functions(kst.layout.win->window,GDK_FUNC_MOVE|GDK_FUNC_MINIMIZE|GDK_FUNC_CLOSE);
#endif
	gtk_window_set_accept_focus(GTK_WINDOW(kst.layout.win),FALSE);
#if GTK_CHECK_VERSION(3,2,0)
	InputBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
#else
	InputBox = gtk_hbox_new(FALSE,0);
#endif
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
#if GTK_CHECK_VERSION(3,0,0)
	g_signal_connect(G_OBJECT(kst.layout.win),"draw",
			G_CALLBACK(kbd_draw),0);
#else
	g_signal_connect(G_OBJECT(kst.layout.win),"expose-event",
			G_CALLBACK(kbd_expose),0);
#endif
}

static void kbd_main_show(int b)
{
	if(!b)
		gtk_widget_hide(kst.layout.win);
	else
	{
		gtk_widget_show(kst.layout.win);
		gtk_window_deiconify(GTK_WINDOW(kst.layout.win));
	}
}
#endif
