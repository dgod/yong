#include "llib.h"
#include "ui.h"
#include "common.h"
#include "ui-draw.h"

#include <assert.h>
#include <stdarg.h>

typedef struct{
	int event,x,y,which;
	void *priv;
}UI_EVENT;

static char skin_path[64];
static double ui_scale=1.0;

bool ui_image_path(const char *file,char path[],int where)
{
	if((where&IMAGE_SKIN)!=0)
	{
		sprintf(path,"%s/%s/%s",y_im_get_path("HOME"),skin_path,file);
		if(l_file_exists(path))
			return true;
	}
	if(strcmp(skin_path,"skin") && (where&IMAGE_SKIN_DEF)!=0)
	{
		sprintf(path,"%s/skin/%s",y_im_get_path("HOME"),file);
		if(l_file_exists(path))
			return true;
	}
	if((where&IMAGE_SKIN)!=0)
	{
		sprintf(path,"%s/%s/%s",y_im_get_path("DATA"),skin_path,file);
		if(l_file_exists(path))
			return true;
	}
	if(strcmp(skin_path,"skin") && (where&IMAGE_SKIN_DEF)!=0)
	{
		sprintf(path,"%s/skin/%s",y_im_get_path("DATA"),file);
		if(l_file_exists(path))
			return true;
	}
	if((where&IMAGE_ROOT)!=0)
	{
		sprintf(path,"%s/%s",y_im_get_path("HOME"),file);
		if(l_file_exists(path))
			return true;
		sprintf(path,"%s/%s",y_im_get_path("DATA"),file);
		if(l_file_exists(path))
			return true;
	}
	return false;
}

UI_IMAGE ui_image_load_scale(const char *file,double scale,int width,int height,int where)
{
	const char *scale_str=NULL;
	UI_IMAGE p=NULL;
	if(scale>0.99 && scale<1.01)
		scale_str="1";
	else if(scale>1.24 && scale<1.26)
		scale_str="1.25";
	else if(scale>1.49 && scale<1.51)
		scale_str="1.5";
	else if(scale>1.74 && scale<1.76)
		scale_str="1.75";
	else if(scale>1.99 && scale<2.01)
		scale_str="2";
	if(scale_str)
	{
		char temp[64];
		snprintf(temp,sizeof(temp),"%s/%s",scale_str,file);
		p=ui_image_load(temp,where);
	}
	if(!p)
	{
		if(width>0 && height>0)
		{
			width=(int)round(scale*width);
			height=(int)round(scale*height);
			p=ui_image_load_at_size(file,width,height,where);
		}
		else
		{
			p=ui_image_load(file,where);
		}
	}
	return p;
}

#ifdef _WIN32



#else

#include <gtk/gtk.h>
typedef char UI_CHAR;
typedef cairo_region_t *UI_REGION;

UI_WINDOW MainWin;

#define ui_icon_load(x) ui_image_load(x)
#define ui_icon_free(x) ui_icon_free(x)

UI_IMAGE ui_image_part(UI_IMAGE img,int x,int y,int w,int h)
{
	return gdk_pixbuf_new_subpixbuf(img,x,y,w,h);
}

static int ui_image_size(UI_IMAGE img,int *w,int *h)
{
	*w=gdk_pixbuf_get_width(img);
	*h=gdk_pixbuf_get_height(img);
	return 0;
}

static void ui_image_draw(UI_DC dc,UI_IMAGE img,int x,int y)
{
	int w,h;
	if(!img)
		return;
	ui_image_size(img,&w,&h);
	cairo_rectangle(dc,x,y,w,h);
	cairo_clip(dc);
	gdk_cairo_set_source_pixbuf(dc,img,x,y);
	cairo_paint(dc);
	cairo_reset_clip(dc);
}

static void UpdateMainWindow(void)
{
	gtk_widget_queue_draw(MainWin);
}

static void ui_region_destroy(UI_REGION r)
{
	cairo_region_destroy(r);
}

#endif

enum{
	UI_STATE_NORMAL=0,
	UI_STATE_DOWN,
	UI_STATE_OVER,
	UI_STATE_DISABLE,
};

enum{
	UI_EVENT_DOWN=0,
	UI_EVENT_UP,
	UI_EVENT_MOVE,
	UI_EVENT_LEAVE,
};

enum{
	UI_BUTTON_LEFT=0,
	UI_BUTTON_MIDDLE,
	UI_BUTTON_RIGHT,
};

typedef struct{
	bool visible;
	int state;
	UI_RECT rc;
	UI_IMAGE bmp[3];
	void (*click)(void *);
	void *arg;

	UI_CHAR text[32];
	UI_FONT font;
	UI_COLOR color;
}UI_BTN_REAL;

static UI_BTN_REAL btns[UI_BTN_COUNT];

static UI_IMAGE MainWin_bg;
static UI_COLOR MainWin_bgc;
static UI_COLOR MainWin_border;
static UI_RECT MainWin_move;
static int MainWin_X,MainWin_Y,MainWin_W,MainWin_H;
static bool MainWin_pos_custom;
static int MainWin_Drag;
static int MainWin_Drag_X,MainWin_Drag_Y;
static int MainWin_tran;
static int MainWin_auto_tran;

UI_WINDOW InputWin;
static int InputWin_X,InputWin_Y;
static int InputWin_Drag;
static int InputWin_Drag_X,InputWin_Drag_Y;

struct{
	int scale;
	int CodeX,CodeY;
	int CandX,CandY;
	int OffX,OffY;
	int Height,Width;
	int mWidth,mHeight;
	int RealHeight,RealWidth;
	int Left,Right;
	int Top,Bottom;
	int WorkLeft,WorkRight,WorkBottom;
	int MaxHeight;
	UI_IMAGE bg[3];
#ifndef _WIN32
	UI_REGION rgn[3];
	GdkRectangle clip;
#endif
	UI_COLOR bg_color;
	UI_COLOR bg_first;
	UI_COLOR border;
	UI_COLOR sep;
	UI_COLOR text[7];
	UI_FONT layout;
	int space;
	int root;
	int noshow;
	int line;
	int caret;
	int no;
	int strip;
	int x,y;
	int tran;
	double line_width;
	int onspot;
	int radius;
	uint8_t pad[4];
	struct{
		int show;
		uint32_t text[2];
		double scale;
		UI_FONT layout;
		UI_COLOR color;
	}page;
}InputTheme;

struct{
	int scale;
	double line_width;
	int move_style;
	int radius;
}MainTheme;

static bool ui_pt_in_rect(const UI_RECT *rc,int x,int y)
{
	return x>=rc->x && y>=rc->y &&
		x<rc->x+rc->w && y<rc->y+rc->h;
}

UI_COLOR ui_color_parse(const char *s)
{
	UI_COLOR clr;
	int len=strlen(s);
	int a=255,r,g,b;
	if(len==9)
		l_sscanf(s,"#%02x%02x%02x%02x",&a,&r,&g,&b);
	else
		l_sscanf(s,"#%02x%02x%02x",&r,&g,&b);
	clr.a=a;clr.g=g;clr.b=b;clr.r=r;
	return clr;
}

static void ui_popup_menu(UI_EVENT *event);
static int ui_button_event(UI_WINDOW win,UI_EVENT *event,UI_BTN_REAL **under)
{
	double scale=MainTheme.scale!=1?ui_scale:1;
	UI_BTN_REAL *cur=NULL;
	UI_BTN_REAL *last=NULL;
	int dirty=0;
	int i;

	event->x=(int)(event->x/scale);
	event->y=(int)(event->y/scale);

	if(event->event!=UI_EVENT_LEAVE)
	for(i=0;i<UI_BTN_COUNT;i++)
	{
		if(btns[i].visible==false)
		{
			continue;
		}
		if(ui_pt_in_rect(&btns[i].rc,event->x,event->y))
			cur=btns+i;
		if(btns[i].state!=UI_STATE_NORMAL)
			last=btns+i;
	}
	if(last!=cur)
		dirty++;
	switch(event->event){
	case UI_EVENT_DOWN:
		if(last && last!=cur && last->state!=UI_STATE_NORMAL)
		{			
			last->state=UI_STATE_NORMAL;
			dirty++;
		}
		if(cur && cur->state!=UI_STATE_DOWN)
		{
			cur->state=UI_STATE_DOWN;
			dirty++;
		}
		break;
	case UI_EVENT_UP:
		if(last && last!=cur && last->state!=UI_STATE_NORMAL)
		{
			last->state=UI_STATE_NORMAL;
			dirty++;
		}
		if(cur && cur->state!=UI_STATE_OVER && event->which==UI_BUTTON_LEFT)
		{
			cur->state=UI_STATE_OVER;
			dirty++;
			if(last==cur)
			{
				if(cur->click)
				{
					cur->click(cur->arg);
				}
				else if(cur==btns+UI_BTN_MENU)
				{
					y_im_setup_config();
				}
			}
		}
		if(cur && event->which==UI_BUTTON_RIGHT)
		{
			if(cur==btns+UI_BTN_MENU)
				ui_popup_menu(event);
			else if(cur==btns+UI_BTN_KEYBOARD)
				y_kbd_popup_menu();
		}
		break;
	case UI_EVENT_MOVE:
		if(last && last!=cur)
		{
			last->state=UI_STATE_NORMAL;
		}
		if(cur && cur->state!=UI_STATE_DOWN)
		{
			cur->state=UI_STATE_OVER;
		}
		break;
	case UI_EVENT_LEAVE:
		if(last)
		{
			last->state=UI_STATE_NORMAL;
			dirty++;
		}
		for(i=0;i<UI_BTN_COUNT;i++)
		{
			if(btns[i].visible==false)
			{
				continue;
			}
			if(btns[i].state!=UI_STATE_NORMAL)
			{
				btns[i].state=UI_STATE_NORMAL;
				dirty++;
			}
		}
		break;
	}
	if(dirty)
	{
		UpdateMainWindow();
	}
	if(under)
		*under=cur;
	return dirty;
}

int ui_button_update(int id,UI_BUTTON *param)
{
	UI_BTN_REAL *btn=btns+id;
	int i;

	for(i=0;i<3;i++)
	{
		if(btn->bmp[i])
		{
			ui_image_free(btn->bmp[i]);
			btn->bmp[i]=0;
		}
	}
	if(btn->font)
	{
		ui_font_free(btn->font);
		btn->font=0;
	}
	if(!param)
	{
		btn->visible=false;
		btn->rc.w=0;
		return 0;
	}
	if(MainTheme.scale!=1)
	{
		btn->bmp[0]=ui_image_load_scale(param->normal,ui_scale,param->w,param->h,IMAGE_SKIN);
		if(param->down)
			btn->bmp[1]=ui_image_load_scale(param->down,ui_scale,param->w,param->h,IMAGE_SKIN);
		if(param->over)
			btn->bmp[2]=ui_image_load_scale(param->over,ui_scale,param->w,param->h,IMAGE_SKIN);
	}
	else
	{
		btn->bmp[0]=ui_image_load(param->normal,IMAGE_SKIN);
		if(param->down)
			btn->bmp[1]=ui_image_load(param->down,IMAGE_SKIN);
		if(param->over)
			btn->bmp[2]=ui_image_load(param->over,IMAGE_SKIN);
	}
	if(btn->bmp[0])
	{
		int w,h;
		ui_image_size(btn->bmp[0],&w,&h);
		btn->rc.w=w;btn->rc.h=h;
	}
	else
	{
		btn->rc.w=0;
		btn->rc.h=0;
	}
	btn->visible=false;
	btn->rc.x=param->x;
	btn->rc.y=param->y;
	if(param->w>0 && param->h>=0)
	{
		btn->rc.w=param->w;
		btn->rc.h=param->h;
	}
	if(MainTheme.scale==1 && param->font)
	{
		double save=ui_scale;
		ui_scale=1;
		btn->font=ui_font_parse(MainWin,param->font,ui_scale);
		ui_scale=save;
	}
	else
	{
		btn->font=param->font?ui_font_parse(MainWin,param->font,ui_scale):0;
	}
	btn->color=param->color?ui_color_parse(param->color):(UI_COLOR){{0,0,0,0}};
	btn->click=param->click;
	btn->arg=param->arg;
	if(id==UI_BTN_MENU || id==UI_BTN_NAME || id==UI_BTN_KEYBOARD)
		ui_button_show(id,1);
	return 0;
}

static int ui_button_show(int id,int show)
{
	btns[id].visible=show;
	if(!show)
		btns[id].state=UI_STATE_NORMAL;
	UpdateMainWindow();
	return 0;
}

static int ui_button_label(int id,const char *text)
{
	if(!btns[id].rc.w)
		return 0;
	y_im_str_encode(text,btns[id].text,0);
	UpdateMainWindow();
	return 0;
}

static void ui_skin_path(const char *p)
{
	strncpy(skin_path,p,63);
	skin_path[63]=0;
}

static void ui_draw_main_win(DRAW_CONTEXT1 *ctx)
{
	double scale=MainTheme.scale!=1?ui_scale:1;
	int i;

	if(MainWin_bg)
	{
		ui_stretch_image(ctx,MainWin_bg,0,0,MainWin_W,MainWin_H);
	}
	else
	{
		double w=MainWin_W;
		double h=MainWin_H;
	
		if(MainTheme.radius)
		{
			ui_draw_round_rect(ctx,0,0,w,h,MainTheme.radius,MainWin_border,MainWin_bgc,MainTheme.line_width*scale);
		}
		else
		{	
			ui_fill_rect(ctx,0,0,w,h,MainWin_bgc);
			ui_draw_rect(ctx,0,0,w,h,MainWin_border,MainTheme.line_width*scale);
		}
		if(MainWin_move.w>=3 && MainWin_move.h>=3)
		{
			if(MainTheme.move_style==0)
			{
				double x=MainWin_move.x*scale+MainWin_move.w*scale/2+0.5;
				double y=MainWin_move.y*scale+0.5;
			
				ui_draw_line(ctx,x,y,x,MainWin_move.h*scale+y,MainWin_border,MainTheme.line_width*scale);
				ui_draw_line(ctx,x+2,y,x+2,MainWin_move.h*scale+y,MainWin_border,MainTheme.line_width*scale);
			}
			else
			{
				double x=MainWin_move.x*scale+MainWin_move.w*scale/3+0.5;
				double y=MainWin_move.y*scale+0.5;
				double d=MainWin_move.w*scale/3;

				ui_draw_line(ctx,x,y,x,MainWin_move.h*scale+y,MainWin_border,MainTheme.line_width*scale);
				ui_draw_line(ctx,x+d,y,x+d,MainWin_move.h*scale+y,MainWin_border,MainTheme.line_width*scale);
			}				
		}
	}
	for(i=0;i<UI_BTN_COUNT;i++)
	{
		UI_BTN_REAL *btn=btns+i;
		UI_RECT *r=&btn->rc;
		if(!btn->visible || !r->w) continue;
		
		int x=(int)round(r->x*scale);
		int y=(int)round(r->y*scale);
		int w=(int)round(r->w*scale);
		int h=(int)round(r->h*scale);

		if((btn->state==UI_STATE_DOWN) && btn->bmp[1])
			ui_stretch_image(ctx,btn->bmp[1],x,y,w,h);
		else if(btn->state==UI_STATE_OVER && btn->bmp[2])
			ui_stretch_image(ctx,btn->bmp[2],x,y,w,h);
		else
			ui_stretch_image(ctx,btn->bmp[0],x,y,w,h);

		if(btn->text[0])
		{
			int tw,th;
#ifdef _WIN32
			UI_FONT old;
			old=SelectObject(ctx->dc,btn->font->gdi);
#endif
			ui_text_size(ctx->dc,btn->font,btn->text,&tw,&th);
#ifdef _WIN32
			SelectObject(ctx->dc,old);
#endif
			x=x+(w-tw)/2;y=y+(h-th)/2;
			ui_draw_text_begin(ctx);
			ui_draw_text(ctx,btn->font,x,y,btn->text,btn->color);
			ui_draw_text_end(ctx);
		}
	}
}

static void ui_draw_input_win(DRAW_CONTEXT1 *ctx)
{
	double scale=InputTheme.scale!=1?ui_scale:1;
	UI_COLOR color;
	int count=0,i;
	EXTRA_IM *eim=CURRENT_EIM();
		
	if(eim)
		count=eim->CandWordCount;

	if(!InputTheme.bg[1])
	{
		double w=InputTheme.RealWidth;
		double h=InputTheme.RealHeight;
		if(InputTheme.radius)
		{
			ui_draw_round_rect(ctx,0,0,w,h,InputTheme.radius,InputTheme.border,InputTheme.bg_color,InputTheme.line_width*scale);
		}
		else
		{
			ui_fill_rect(ctx,0,0,w,h,InputTheme.bg_color);
			ui_draw_rect(ctx,0,0,w,h,InputTheme.border,InputTheme.line_width*scale);
		}
	}
	else/* if(InputTheme.line!=2)*/
	{
		if(InputTheme.bg[0])
		{
			ui_draw_image(ctx,InputTheme.bg[0],0,0);
		}
		if(InputTheme.bg[2])
		{
			ui_draw_image(ctx,InputTheme.bg[2],InputTheme.RealWidth-InputTheme.Right,0);
		}
		ui_stretch_image(ctx,InputTheme.bg[1],
				InputTheme.Left,0,
				InputTheme.RealWidth-InputTheme.Left-InputTheme.Right,
				InputTheme.RealHeight);
	}
	
	if(InputTheme.line!=1 && InputTheme.sep.a!=0)
	{
		double w=InputTheme.RealWidth;
		double h=InputTheme.Height;
		ui_draw_line(ctx,4,h/2-1,w-5,h/2-1,InputTheme.sep,InputTheme.line_width*scale);
	}
	
	if(InputTheme.caret && !(InputTheme.onspot && InputTheme.line==1 && im.Preedit==1))
	{
		ui_draw_line(ctx,im.CodePos[2],InputTheme.CodeY+2,
					im.CodePos[2],InputTheme.CodeY+im.cursor_h,
					InputTheme.text[3],InputTheme.line_width);
	}
	if(eim && eim->SelectIndex>=0 && eim->SelectIndex<count && InputTheme.bg_first.a!=0)
	{
		i=eim->SelectIndex;
		double *posx=im.CandPosX+3*i;
		double *posy=im.CandPosY+3*i;
		int x,y,w,h;
		int border=(int)ceil(InputTheme.line_width*scale);
		if(InputTheme.line!=2)
		{
			x=posx[0];
			y=MIN(MIN(posy[0],posy[1]),posy[2]);
			w=im.CandWidth[i];
			h=im.CandHeight[i];
		}
		else
		{
			x=InputTheme.WorkLeft;
			y=MIN(MIN(posy[0],posy[1]),posy[2]);
			w=InputTheme.RealWidth-InputTheme.WorkLeft-InputTheme.WorkRight;
			h=im.CandHeight[i];
		}
		color=InputTheme.bg_first;
		x-=InputTheme.pad[3];
		y-=InputTheme.pad[0];
		w+=InputTheme.pad[1]+InputTheme.pad[3];
		h+=InputTheme.pad[0]+InputTheme.pad[2];
		if(x<border)
		{
			w-=border-x;
			x=border;
		}
		if(x+w>InputTheme.RealWidth-border)
		{
			w=InputTheme.RealWidth-border-x;
		}
		if(InputTheme.line==1 && y<border)
		{
			h-=border-y;
			y=border;
		}
		if(InputTheme.line!=1 && y<InputTheme.Height/2-1+border)
		{
			double t=InputTheme.Height/2-1+border;
			h-=t-y;
			y=t;
		}
		if(y+h>InputTheme.RealHeight-border)
		{
			h=InputTheme.RealHeight-border-y;
		}
		ui_fill_rect(ctx,x,y,w,h,color);
	}

	ui_draw_text_begin(ctx);

	if(eim && eim->StringGet[0] && !(InputTheme.onspot && InputTheme.line==1 && im.Preedit==1))
	{
		ui_draw_text(ctx,InputTheme.layout,im.CodePos[0],InputTheme.CodeY,
			im.StringGet,InputTheme.text[6]);

	}
	if(im.CodeInput[0] && !(InputTheme.onspot && InputTheme.line==1 && im.Preedit==1))
	{
		ui_draw_text(ctx,InputTheme.layout,im.CodePos[1],InputTheme.CodeY,
			im.CodeInput,InputTheme.text[5]);
	}
	if(/*!im.EnglishMode && */eim && eim->CandPageCount>1 && InputTheme.page.show)
	{
		double pos_x,pos_y;
		pos_x=im.PagePosX;
		pos_y=im.PagePosY;
		UI_FONT font=InputTheme.page.layout?InputTheme.page.layout:InputTheme.layout;
		if(InputTheme.page.text[0])
		{
#ifdef _WIN32
			WCHAR temp[8];
#else
			char temp[8];
#endif
			int pos;
			UI_COLOR color;
#ifdef _WIN32
			pos=l_unichar_to_utf16(InputTheme.page.text[0],(void*)temp)/2;
#else
			pos=l_unichar_to_utf8(InputTheme.page.text[0],(void*)temp);
#endif
			temp[pos]=0;
			color=eim->CurCandPage>0?InputTheme.text[4]:InputTheme.page.color;
			ui_draw_text(ctx,font,pos_x,pos_y,temp,color);
#ifdef _WIN32
			pos=l_unichar_to_utf16(InputTheme.page.text[1],(void*)temp)/2;
#else
			pos=l_unichar_to_utf8(InputTheme.page.text[1],(void*)temp);
#endif
			temp[pos]=0;
			color=eim->CurCandPage<eim->CandPageCount-1?InputTheme.text[4]:InputTheme.page.color;
			ui_draw_text(ctx,font,pos_x+im.PageLen[0]+im.PageLen[1],pos_y,temp,color);
		}
		else
		{
			ui_draw_text(ctx,font,pos_x,pos_y,im.Page,InputTheme.text[4]);
		}
	}
	
	for(i=0;i<count;i++)
	{
		double *posx=im.CandPosX+3*i;
		double *posy=im.CandPosY+3*i;

		if(InputTheme.no==0)
		{
			color=InputTheme.text[0];
			if(i==eim->SelectIndex && InputTheme.bg_first.a!=0)
				color=InputTheme.text[1];
			ui_draw_text(ctx,InputTheme.layout,posx[0],posy[0],YongGetSelectNumber(i),color);
		}
		else
		{
			color=InputTheme.text[0];
		}
		
		if(i==eim->SelectIndex) color=InputTheme.text[1];
		ui_draw_text(ctx,InputTheme.layout,posx[1],posy[1],im.CandTable[i],color);

		if(im.Hint && eim->CodeTips && eim->CodeTips[i][0])
		{
			color=InputTheme.text[2];
			ui_draw_text(ctx,InputTheme.layout,posx[2],posy[2],im.CodeTips[i],color);
		}
	}
	ui_draw_text_end(ctx);
}

