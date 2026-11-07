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
static double ui_scale=1.0;			// ui windows scale to
static double ui_res_scale=1.0;		// scale used when load images, @win scale=res_scale
static double ui_surface_scale=1.0;	// scale of surface internal

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

double ui_image_file_scale(const char *file,char scale[])
{
	const char *b,*e;
	int i;
	scale[0]='1';scale[1]=0;
	b=strchr(file,'@');
	if(!b)
		return 1.0;
	b++;
	e=strrchr(file,'.');
	if(!e)
		return 1.0;
	for(i=0;i<5 && b+i<e;i++)
	{
		scale[i]=b[i];
	}
	scale[i]=0;
	double r=strtod(scale,NULL);
	if(r<=0)
	{
		scale[0]='1';
		scale[1]=0;
		return 1;
	}
	return r;
}

UI_IMAGE ui_image_load_scale(const char *file,double scale,int width,int height,int where)
{
	const char *scale_str=NULL;
	char orig_scale_str[8];
	double orig_scale;
	UI_IMAGE p=NULL;
	if(l_str_has_suffix(file,".svg"))
	{
		if(width>0 && height>0)
		{
			width=(int)round(scale*width);
			height=(int)round(scale*height);
		}
		p=ui_image_load_at_size(file,width,height,where);
		return p;
	}
	orig_scale=ui_image_file_scale(file,orig_scale_str);
	if(scale==1)
		scale_str="1";
	else if(scale>1.24 && scale<1.26)
		scale_str="1.25";
	else if(scale>1.49 && scale<1.51)
		scale_str="1.5";
	else if(scale>1.74 && scale<1.76)
		scale_str="1.75";
	else if(scale==2)
		scale_str="2";
	if(scale_str)
	{
		if(!strcmp(scale_str,orig_scale_str))
		{
			p=ui_image_load(file,where);
		}
		else
		{
			char temp[64];
			snprintf(temp,sizeof(temp),"%s/%s",scale_str,file);
			p=ui_image_load(temp,where);
		}
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
			width=-(int)round(scale/orig_scale*10000);
			p=ui_image_load_at_size(file,width,-1,where);
		}
	}
	return p;
}

UI_IMAGE ui_image_load_page(int size,UI_COLOR color,bool up)
{
	static const char *path_up="M 56.1875 5.953125 L 56.1875 -54.203125 L 4.09375 -24.125 Z";
	static const char *path_down="M 7.8125 5.953125 L 59.90625 -24.125 L 7.8125 -54.203125 Z";
	char svg[1024];
	int pos=sprintf(svg,"<svg width=\"64\" height=\"64\" viewBox=\"0 0 64 64\">");
	pos+=sprintf(svg+pos,"<path fill=\"#%02x%02x%02x\" fill-opacity=\"1\" transform=\"translate(0,56.240234)\" d=\"%s\"/>",
			color.r,color.g,color.b,(up?path_up:path_down));
	pos+=sprintf(svg+pos,"</svg>");
	return ui_image_load_at_size(svg,size,size,0);
}

#ifdef _WIN32


#elif defined(USE_WUI)
typedef char UI_CHAR;
UI_WINDOW MainWin;

static void UpdateMainWindow(void)
{
	wui->win_redraw(MainWin);
}

static void ui_region_destroy(UI_REGION r)
{
	cairo_region_destroy(r);
}

#else

#include <gtk/gtk.h>
typedef char UI_CHAR;

UI_WINDOW MainWin;

#define ui_icon_load(x) ui_image_load(x)
#define ui_icon_free(x) ui_icon_free(x)

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
	int Middle;
	int Left,Right;
	int Top,Bottom;
	int WorkLeft,WorkRight,WorkBottom;
	int MaxHeight;
	UI_IMAGE bg[3];
#ifndef _WIN32
	UI_REGION rgn[3];
	cairo_rectangle_int_t clip;
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
	uint8_t radius[2];
	uint8_t pad[4];
	struct{
		int show;
		uint32_t text[2];
		double scale;
		UI_FONT layout;
		UI_COLOR color;
		UI_IMAGE up[2];
		UI_IMAGE down[2];
		int space;
		int size;
	}page;
	UI_SHADOW shadow;
	UI_PET *pets;
}InputTheme;

struct{
	int scale;
	double line_width;
	int move_style;
	int radius;

	UI_IMAGE bg;
	UI_COLOR bg_color;
	UI_COLOR border;
	UI_RECT move;

	UI_SHADOW shadow;
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
	{
		l_sscanf(s,"#%02x%02x%02x%02x",&r,&g,&b,&a);
	}
	else if(len==7)
	{
		l_sscanf(s,"#%02x%02x%02x",&r,&g,&b);
	}
	else
	{
		l_sscanf(s,"#%1x%1x%1x",&r,&g,&b);
		r*=17;g*=17;b*=17;
	}
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

	event->x=(int)(event->x/scale);
	event->y=(int)(event->y/scale);

	if(event->event!=UI_EVENT_LEAVE)
	for(int i=0;i<UI_BTN_COUNT;i++)
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
				y_kbd_popup_menu(1);
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
		for(int i=0;i<UI_BTN_COUNT;i++)
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

	for(int i=0;i<3;i++)
	{
		if(btn->bmp[i])
		{
			ui_image_free(btn->bmp[i]);
			btn->bmp[i]=NULL;
		}
	}
	if(btn->font)
	{
		ui_font_free(btn->font);
		btn->font=NULL;
	}
	if(!param)
	{
		btn->visible=false;
		btn->rc.w=0;
		return 0;
	}
	if(MainTheme.scale!=1)
	{
		if(param->normal)
		{
			btn->bmp[0]=ui_image_load_scale(param->normal,ui_res_scale,param->w,param->h,IMAGE_SKIN);
		}
		if(param->down)
		{
			btn->bmp[1]=ui_image_load_scale(param->down,ui_res_scale,param->w,param->h,IMAGE_SKIN);
		}
		if(param->over)
		{
			btn->bmp[2]=ui_image_load_scale(param->over,ui_res_scale,param->w,param->h,IMAGE_SKIN);
		}
	}
	else
	{
		if(param->normal)
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
		if(param->w<=0 || param->h<=0)
		{	
			double scale=MainTheme.scale!=1?ui_res_scale:1;
			if(scale==1)
			{
				btn->rc.w=w;btn->rc.h=h;
			}
			else
			{
				btn->rc.w=(int)round(w/scale);
				btn->rc.h=(int)round(h/scale);
			}
		}
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
		btn->font=ui_font_parse(MainWin,param->font,ui_scale,y_ui_get_scale(1));
		ui_scale=save;
	}
	else
	{
		btn->font=param->font?ui_font_parse(MainWin,param->font,ui_scale,y_ui_get_scale(1)):0;
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
	l_strcpy(skin_path,sizeof(skin_path),p);
}

static void ui_draw_main_win(DRAW_CONTEXT1 *ctx)
{
	double scale=MainTheme.scale!=1?ui_scale:1;

	if(MainTheme.bg)
	{
		ui_stretch_image(ctx,MainTheme.bg,0,0,MainWin_W,MainWin_H);
	}
	else
	{
		double w=MainWin_W;
		double h=MainWin_H;
	
		if(MainTheme.radius)
		{
			ui_draw_round_rect(ctx,0,0,w,h,MainTheme.radius,MainTheme.border,MainTheme.bg_color,MainTheme.line_width*scale);
		}
		else
		{	
			ui_fill_rect(ctx,0,0,w,h,MainTheme.bg_color);
			ui_draw_rect(ctx,0,0,w,h,MainTheme.border,MainTheme.line_width*scale);
		}
		if(MainTheme.move.w>=3 && MainTheme.move.h>=3)
		{
			if(MainTheme.move_style==0)
			{
				double x=MainTheme.move.x*scale+MainTheme.move.w*scale/2+0.5;
				double y=MainTheme.move.y*scale+0.5;
			
				ui_draw_line(ctx,x,y,x,MainTheme.move.h*scale+y,MainTheme.border,MainTheme.line_width*scale);
				ui_draw_line(ctx,x+2,y,x+2,MainTheme.move.h*scale+y,MainTheme.border,MainTheme.line_width*scale);
			}
			else
			{
				double x=MainTheme.move.x*scale+MainTheme.move.w*scale/3+0.5;
				double y=MainTheme.move.y*scale+0.5;
				double d=MainTheme.move.w*scale/3;

				ui_draw_line(ctx,x,y,x,MainTheme.move.h*scale+y,MainTheme.border,MainTheme.line_width*scale);
				ui_draw_line(ctx,x+d,y,x+d,MainTheme.move.h*scale+y,MainTheme.border,MainTheme.line_width*scale);
			}				
		}
	}

	for(int i=0;i<UI_BTN_COUNT;i++)
	{
		UI_BTN_REAL *btn=btns+i;
		UI_RECT *r=&btn->rc;
		if(!btn->visible || !r->w)
			continue;
		
		int x=(int)round(r->x*scale);
		int y=(int)round(r->y*scale);
		int w=(int)round(r->w*scale);
		int h=(int)round(r->h*scale);

		if((btn->state==UI_STATE_DOWN) && btn->bmp[1])
			ui_stretch_image(ctx,btn->bmp[1],x,y,w,h);
		else if(btn->state==UI_STATE_OVER && btn->bmp[2])
			ui_stretch_image(ctx,btn->bmp[2],x,y,w,h);
		else if(btn->bmp[0])
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

static void ui_pets_draw(DRAW_CONTEXT1 *ctx,int z_index);
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
		if(InputTheme.radius[0])
		{
			ui_draw_round_rect(ctx,0,0,w,h,InputTheme.radius[0],InputTheme.border,InputTheme.bg_color,InputTheme.line_width*scale);
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
			ui_stretch_image(ctx,InputTheme.bg[0],
					0,0,
					InputTheme.Left,InputTheme.RealHeight);
		}
		if(InputTheme.bg[2])
		{
			ui_stretch_image(ctx,InputTheme.bg[2],
					InputTheme.RealWidth-InputTheme.Right,0,
					InputTheme.Right,InputTheme.RealHeight);
		}
		ui_stretch_image(ctx,InputTheme.bg[1],
				InputTheme.Left,0,
				InputTheme.RealWidth-InputTheme.Left-InputTheme.Right,
				InputTheme.RealHeight);
	}

	if(InputTheme.line!=1 && InputTheme.sep.a!=0)
	{
		double w=InputTheme.RealWidth;
		double m=InputTheme.Middle;
		if(!InputTheme.bg[1])
		{
			// ui_draw_line(ctx,4,m-1,w-5,m-1,InputTheme.sep,InputTheme.line_width*scale);
			int margin=(int)round(4*ui_scale);
			ui_draw_line(ctx,margin,m-1,w-1-margin,m-1,InputTheme.sep,InputTheme.line_width*scale);
		}
		else
		{
			double l=InputTheme.WorkLeft;
			double r=InputTheme.WorkRight;
			ui_draw_line(ctx,l,m-1,w-r,m-1,InputTheme.sep,InputTheme.line_width*scale);
		}
	}

	ui_pets_draw(ctx,0);
	
	if(InputTheme.caret && !(y_xim_get_onspot() && InputTheme.line==1 && im.Preedit==1))
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
		if(InputTheme.line!=1 && y<InputTheme.Middle-1+border)
		{
			double t=InputTheme.Middle-1+border;
			h-=t-y;
			y=t;
		}
		if(y+h>InputTheme.RealHeight-border)
		{
			h=InputTheme.RealHeight-border-y;
		}
		if(InputTheme.radius[1])
			ui_draw_round_rect(ctx,x,y,w,h,InputTheme.radius[1],color,color,0);
		else
			ui_fill_rect(ctx,x,y,w,h,color);
	}

	if(/*!im.EnglishMode && */eim && eim->CandPageCount>1 && InputTheme.page.show)
	{
		if(InputTheme.page.text[0]==1 && InputTheme.page.up[0])
		{
			double pos_x,pos_y;
			pos_x=im.PagePosX;
			pos_y=im.PagePosY;
			UI_IMAGE up=eim->CurCandPage>0?InputTheme.page.up[0]:InputTheme.page.up[1];
			ui_stretch_image(ctx,up,pos_x,pos_y,InputTheme.page.size,InputTheme.page.size);
			UI_IMAGE down=eim->CurCandPage<eim->CandPageCount-1?InputTheme.page.down[0]:InputTheme.page.down[1];
			ui_stretch_image(ctx,down,pos_x+im.PageLen[0]+im.PageLen[1],pos_y,InputTheme.page.size,InputTheme.page.size);
		}
	}


	ui_draw_text_begin(ctx);

	if(eim && eim->StringGet[0] && !(y_xim_get_onspot() && InputTheme.line==1 && im.Preedit==1))
	{
		ui_draw_text(ctx,InputTheme.layout,im.CodePos[0],InputTheme.CodeY,
			im.StringGet,InputTheme.text[6]);
	}
	if(im.CodeInput[0] && !(y_xim_get_onspot() && InputTheme.line==1 && im.Preedit==1))
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
		if(InputTheme.page.text[0]>1)
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
		else if(InputTheme.page.text[0]==1 && InputTheme.page.up[0])
		{
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

		if(InputTheme.no==0 || InputTheme.no==2)
		{
			color=InputTheme.text[0];
			if(i==eim->SelectIndex && InputTheme.no==2)
				color=InputTheme.text[1];
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
	ui_pets_draw(ctx,1);
}

int ui_shadow_init(UI_SHADOW *s,const char *c)
{
	int len;
	char color[32];
	s->len=0;
	s->color.color=0;
	if(!c)
		return -1;
	int ret=l_sscanf(c,"%d %31s %f,%f %f,%f",&len,color,&s->x1,&s->y1,&s->x2,&s->y2);
	if(ret!=1 && ret!=2 && ret!=6)
		return -1;
	if(len<3 || len>63)
		return -2;
	s->len=len;
	s->color=ui_color_parse(ret>1?color:"#00000040");
	if(ret>2)
	{
		if(s->x1<0.0f || s->x1>1.0f || s->y1<0.0f || s->y1>1.0f)
			return -3;
		if(s->x2<0.0f || s->x2>1.0f || s->y2<0.0f || s->y2>1.0f)
			return -3;
	}
	else
	{
		s->x1=0.0f;
		s->y1=1.0f;
		s->x2=0.5f;
		s->y2=0.0f;
	}
	return 0;
}

static uint32_t ui_anchor_parse(const char *s)
{
	uint32_t r=0;
	int c;
	for(int i=0;(c=s[i])!='\0';i++)
	{
		if(c>='A' && c<='Z')
		{
			r|=1<<(c-'A');
		}
	}
	return r;
}

static UI_PET *ui_pet_new(const char *c)
{
	if(!c)
		return NULL;
	double scale=InputTheme.scale!=1?ui_scale:1;
	char which[8],image[64],anchor[8];
	int w,h,off_x=0,off_y=0,z_index=0;
	int ret=l_sscanf(c,"%7s %63s %7s %d,%d %d,%d %d",which,image,anchor,&w,&h,&off_x,&off_y,&z_index);
	if(ret<7)
	{
		// fprintf(stderr,"pet config %s bad\n",c);
		return NULL;
	}
	UI_PET *p=l_new(UI_PET);
	strcpy(p->which,which);
	p->image=ui_image_load_scale(image,scale*ui_res_scale,w,h,IMAGE_SKIN);
	if(!p->image)
	{
		// fprintf(stderr,"pet load %s fail\n",image);
		l_free(p);
		return NULL;
	}
	p->anchor=ui_anchor_parse(anchor);
	p->w=(int)round(w*scale);
	p->h=(int)round(h*scale);
	p->off_x=(int)round(off_x*scale);
	p->off_y=(int)round(off_y*scale);
	p->z_index=z_index;

	return p;
}

UI_PET *ui_pets_new(LPtrArray *arr)
{
	if(!arr)
		return NULL;
	int len=l_ptr_array_length(arr);
	UI_PET *r=NULL;
	for(int i=0;i<len;i++)
	{
		const char *c=l_ptr_array_nth(arr,i);
		UI_PET *p=ui_pet_new(c);
		if(p)
			r=l_slist_prepend(r,p);
	}
	return r;

}

void ui_pet_free(UI_PET *p)
{
	if(!p)
		return;
	ui_image_free(p->image);
	l_free(p);
}

static int ui_pet_get_rect(const char *which,UI_RECT *rc)
{
	if(!strcmp(which,"input"))
	{
		*rc=(UI_RECT){0,0,InputTheme.RealWidth,InputTheme.RealHeight};
		return 0;
	}
	if(!strcmp(which,"code"))
	{
		*rc=(UI_RECT){
			im.CodePos[1],
			InputTheme.CodeY,
			im.CodePos[3]-im.CodePos[1],
			im.cursor_h};
		return 0;
	}
	if(!strcmp(which,"caret"))
	{
		*rc=(UI_RECT){
			im.CodePos[2],
			InputTheme.CodeY,
			(int)ceil(InputTheme.line_width),
			im.cursor_h};
		return 0;
	}
	EXTRA_IM *eim=CURRENT_EIM();
	if(!eim)
		return -1;
	if(!strcmp(which,"select") && eim->SelectIndex<eim->CandWordCount)
	{
		double *posx=im.CandPosX+3*eim->SelectIndex;
		double *posy=im.CandPosY+3*eim->SelectIndex;

		*rc=(UI_RECT){
			(int)posx[1],
			(int)posy[1],
			im.CandWidth[eim->SelectIndex],
			im.CandHeight[eim->SelectIndex],
		};
		return 0;
	}
	return -1;
}

static void ui_pet_anchor_calc(UI_RECT *rc,uint32_t anchor,int *x,int *y)
{
	uint32_t L=anchor&(1<<('L'-'A'));
	uint32_t T=anchor&(1<<('T'-'A'));
	uint32_t R=anchor&(1<<('R'-'A'));
	uint32_t B=anchor&(1<<('B'-'A'));
	if(L && !R)
		*x=rc->x;
	else if(R && !L)
		*x=rc->x+rc->w-1;
	else
		*x=(rc->x+rc->w/2);
	
	if(T && !B)
		*y=rc->y;
	else if(B && !T)
		*y=rc->y+rc->h-1;
	else
		*y=(rc->y+rc->h/2);
}

int ui_pet_get_pos(UI_PET *p,int *x,int *y)
{
	UI_RECT rc;
	if(0!=ui_pet_get_rect(p->which,&rc))
		return -1;

	int anchor_x,anchor_y;
	ui_pet_anchor_calc(&rc,p->anchor,&anchor_x,&anchor_y);

	*x=anchor_x+p->off_x;
	*y=anchor_y+p->off_y;
	return 0;
}

static void ui_pets_draw(DRAW_CONTEXT1 *ctx,int z_index)
{
	UI_PET *p=InputTheme.pets;
	while(p!=NULL)
	{
		if(p->z_index==z_index)
		{
			int x,y;
			int ret=ui_pet_get_pos(p,&x,&y);
			if(ret==0)
			{
				ui_stretch_image(ctx,p->image,x,y,p->w,p->h);
			}
		}
		p=p->next;
	}
}
