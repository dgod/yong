#include <math.h>
#include "common.h"

#include "ui.h"
#include "translate.h"
#include "color-schema.h"
#include "app-indicator.h"
#include "wui.h"
#include "ybus.h"
#include "ybus-ibus.h"

const W_UI *wui;

static int ui_button_show(int id,int show);

#include "ui-common.c"
#include "ui-timer.c"
#include "ui-poll.c"

static bool is_wayland=false;
static bool is_gnome=false;
static bool MainWin_over=false;
static bool MainWin_visible=false;

static int IconSelected;
static void *StatusIcon;

static w_win_t ImageWin;
static UI_IMAGE ImageWin_bg;

static w_win_t InputWin_wayland;
static w_win_t InputWin_layer;

typedef struct ui_menu{
	LKeyFile *config;
	w_win_t root;
	GMenu *root_g;
	GSimpleActionGroup *actions;
	int mb;
	int base;
	int count;
	char *cmd[64];
}ui_menu_t;

static W_INPUT_HOOK input_hook;
static bool wayland_tip_center;

static ui_menu_t *ui_build_menu2(LKeyFile *kf);
int ybus_wayland_ui_init(W_INPUT_HOOK *input_hook);

static bool is_process_running(const char *exe)
{
	LDir *dir=l_dir_open("/proc");
	if(!dir)
		return true;
	while(1)
	{
		const char *name=l_dir_read_name(dir);
		if(!name)
			break;
		if(!isdigit(name[0]))
			continue;
		char cmdline[1024];
		sprintf(cmdline,"/proc/%s/cmdline",name);
		FILE *fp=fopen(cmdline,"r");
		if(!fp)
			continue;
		size_t size=fread(cmdline,1,sizeof(cmdline)-1,fp);
		fclose(fp);
		if(size<=0)
			continue;
		cmdline[size]=0;
		if(strstr(cmdline,exe))
		{
			l_dir_close(dir);
			return true;
		}
	}
	l_dir_close(dir);
	return false;
}

static int ui_wait_other(void)
{
	const char *wait=y_im_get_config_data("main","wait");
	if(wait)
	{
		char exe[128];
		int timeout0=2000,timeout1=100;
		sscanf(wait,"%127s %d %d",exe,&timeout0,&timeout1);
		for(int i=0;i<timeout0;i+=100)
		{
			if(is_process_running(exe))
			{
				if(timeout1>0)
					l_thrd_sleep_ms(timeout1);
				break;
			}
			l_thrd_sleep_ms(100);
		}
	}
	int delay=y_im_get_config_int("main","delay");
	if(delay>0)
	{
		l_thrd_sleep_ms(delay*1000);
	}
	return 0;
}

static void calc_ui_scale(void)
{
	int dpi=wui->win_get_dpi(NULL);
	if(dpi>=96)
		ui_scale=dpi/96.0;
	const char *temp=y_im_get_config_data("main","scale");
	if(temp && temp[0])
	{
		ui_scale=strtod(temp,NULL);
	}
	ui_surface_scale=wui->win_get_scale(NULL);
	ui_res_scale=ui_scale*ui_surface_scale;
	// printf("screen size changed %f %f\n",ui_scale,ui_res_scale);
}

static int main_click_cb(w_win_t win,const W_EVENT *e,void *data)
{
	if(e->mouse.button==W_BUTTON_MIDDLE)
	{
		if(e->type==W_MOUSE_UP)
		{
			YongReloadAllTip();
		}
		return 0;
	}
	UI_EVENT ue={
		.event=e->type==W_MOUSE_DOWN?UI_EVENT_DOWN:UI_EVENT_UP,
		.x=e->mouse.x-MainTheme.shadow.len,
		.y=e->mouse.y-MainTheme.shadow.len,
		.which=e->mouse.button==W_BUTTON_LEFT?UI_BUTTON_LEFT:UI_BUTTON_RIGHT,
	};
	if(ue.x<0 || ue.x>=MainWin_W || ue.y<0 || ue.y>=MainWin_W)
		return 0;
	if(!MainWin_Drag)
	{
		int ret=ui_button_event(MainWin,&ue,NULL);
		if(ret)
			return 1;
	}
	if(e->mouse.button==W_BUTTON_RIGHT)
	{
		if(e->type==W_MOUSE_UP)
		{
			CONNECT_ID *id=y_xim_get_connect();
			if(!id || (id && !id->focus))
				YongShowMain(0);
		}
		return 1;
	}
	if(e->type==W_MOUSE_DOWN)
	{
		wui->win_set_capture(MainWin,true);
		wui->win_set_cursor(MainWin,W_CURSOR_MOVE);
		MainWin_Drag=TRUE;
		MainWin_Drag_X=e->mouse.x;
		MainWin_Drag_Y=e->mouse.y;
	}
	else if(MainWin_Drag)
	{
		MainWin_Drag=FALSE;
		wui->win_set_capture(MainWin,false);
		wui->win_set_cursor(MainWin,W_CURSOR_DEFAULT);
		int w,h;
		wui->get_workarea(MainWin,NULL,NULL,&w,&h);
		if(MainWin_X<0) MainWin_X=0;
		else if(MainWin_X>w-MainWin_W) MainWin_X=w-MainWin_W;
		if(MainWin_Y<0) MainWin_Y=0;
		else if(MainWin_Y>h-MainWin_H) MainWin_Y=h-MainWin_H;
		wui->win_move(MainWin,MainWin_X,MainWin_Y);
		if(MainWin_pos_custom)
		{
			char temp[64];
			sprintf(temp,"%d,%d",MainWin_X,MainWin_Y);
			y_im_set_config_string("main","pos",temp);
			y_im_save_config();
		}
	}

	return 1;
}

static int main_enter_leave_notify(w_win_t win,const W_EVENT *e,void *data)
{
	if(e->type==W_MOUSE_ENTER)
	{
		MainWin_over=true;
	}
	else
	{
		if(MainWin_Drag && wui->wayland_get_surface(win))
		{
			W_EVENT f={
				.type=W_MOUSE_UP,
				.mouse={
					.button=W_BUTTON_LEFT,
					.x=e->mouse.x,
					.y=e->mouse.y,
				}
			};
			main_click_cb(win,&f,data);
		}
		MainWin_over=false;

		UI_EVENT ue;
		ue.event=UI_EVENT_LEAVE;
		ui_button_event(MainWin,&ue,NULL);
	}
	int tran=MainWin_tran;
	if(MainWin_auto_tran && !MainWin_over)
		tran=255-(255-tran)*2/3;
	return wui->win_tran(MainWin,tran);
}

static int main_motion_cb(w_win_t win,const W_EVENT *e,void *data)
{
	if(MainWin_Drag)
	{
		int x,y;
		wui->win_get_pos(win,&x,&y);
		MainWin_X=x+e->mouse.x-MainWin_Drag_X;
		MainWin_Y=y+e->mouse.y-MainWin_Drag_Y;
		wui->win_move(win,MainWin_X,MainWin_Y);
		return 0;
	}
	UI_EVENT ue={
		.event=UI_EVENT_MOVE,
		.x=e->mouse.x-MainTheme.shadow.len,
		.y=e->mouse.y-MainTheme.shadow.len,
	};
	if(ue.x<0 || ue.x>=MainWin_W || ue.y<0 || ue.y>=MainWin_W)
		return 0;
	ui_button_event(MainWin,&ue,NULL);
	return 0;
}

static guint screen_changed_timer=0;
static gboolean on_screen_size_changed_next(gpointer unused)
{
	screen_changed_timer=0;
	calc_ui_scale();
	y_ui_reload_all();
	return FALSE;
}

static int on_screen_size_changed(w_win_t win,const W_EVENT *e,void *data)
{
	if(screen_changed_timer)
		g_source_remove(screen_changed_timer);
	screen_changed_timer=g_timeout_add(100,on_screen_size_changed_next,NULL);
	return 0;
}

static int on_main_draw(w_win_t win,const W_EVENT *e,void *data)
{
	DRAW_CONTEXT1 ctx;
	ui_draw_begin(&ctx,win,e->draw.cr);
	if(MainTheme.shadow.len)
	{
		ui_draw_shadow(&ctx,MainTheme.radius,&MainTheme.shadow);
		ui_draw_translate(&ctx,MainTheme.shadow.len,MainTheme.shadow.len);
	}
	ui_draw_main_win(&ctx);
	ui_draw_end(&ctx);
	return 1;
}

static int input_motion_cb(w_win_t win,const W_EVENT *e,void *data)
{
	if(!InputWin_Drag)
		return 0;
	int x,y;
	wui->win_get_pos(win,&x,&y);
	InputWin_X=x+e->mouse.x-InputWin_Drag_X+InputTheme.shadow.len;
	InputWin_Y=y+e->mouse.y-InputWin_Drag_Y+InputTheme.shadow.len;
	wui->win_move(win,InputWin_X-InputTheme.shadow.len,InputWin_Y-InputTheme.shadow.len);
	return 1;
}

static int input_click_cb(w_win_t win,const W_EVENT *e,void *data)
{
	if(e->mouse.button!=W_BUTTON_LEFT && e->mouse.button!=W_BUTTON_RIGHT)
		return 0;
	if(e->type==W_MOUSE_DOWN && e->mouse.button==W_BUTTON_LEFT)
	{
		if(InputWin_Drag) return 0;
		int x=e->mouse.x-InputTheme.shadow.len;
		int y=e->mouse.y-InputTheme.shadow.len;
		if(x >= InputTheme.CandX && y>= InputTheme.CandY)
		{
			EXTRA_IM *eim=CURRENT_EIM();
			if(eim)
			{
				int count=eim->CandWordCount;
				double x=e->mouse.x;
				if(count && x>=im.CandPosX[0] && x<im.CandPosX[3*count])
				{
					return 1;
				}
			}
		}
		wui->win_set_capture(InputWin,true);
		wui->win_set_cursor(InputWin,W_CURSOR_MOVE);
		InputWin_Drag=TRUE;
		InputWin_Drag_X=e->mouse.x;
		InputWin_Drag_Y=e->mouse.y;
		return 1;
	}
	else if(e->type==W_MOUSE_UP)
	{
		if(!InputWin_Drag)
		{
			EXTRA_IM *eim=CURRENT_EIM();
			if(!eim)
				return 1;
			double x=e->mouse.x-InputTheme.shadow.len;
			double y=e->mouse.y-InputTheme.shadow.len;
			if(x < InputTheme.CandX || y< InputTheme.CandY)
				return 1;
			int count=eim->CandWordCount;
			if(!count) return 1;
			for(int i=0;i<count;i++)
			{
				double *pos=im.CandPosX+i*3;
				if(InputTheme.line==2)
				{
					pos=im.CandPosY+i*3;
					x=y;
				}
				if(x >= pos[0] && ((i<count-1 && x<pos[3]-InputTheme.space)
					|| (i==count-1 && x<pos[3])))
				{
					if(e->mouse.button==W_BUTTON_LEFT)
					{
						char *s=eim->GetCandWord(i);
						if(s)
						{
							y_xim_send_string(s2t_conv(s));
							YongResetIM();
						}
						else
						{
							if(eim->SelectIndex>=0)
								YongUpdateInputDesc(eim);
							y_im_str_encode(eim->StringGet,im.StringGet,0);
							y_ui_input_draw();
						}
					}
					else
					{
						const char *s=eim->CandTable[i];
						y_xim_send_string(s2t_conv(s));
					}
					break;					
				}
			}

		}
		else
		{
			InputWin_Drag=FALSE;
			wui->win_set_capture(InputWin,false);
			wui->win_set_cursor(InputWin,W_CURSOR_DEFAULT);
			int w,h;
			wui->get_workarea(InputWin,NULL,NULL,&w,&h);
			if(InputWin_X<0) InputWin_X=0;
			else if(InputWin_X>w-InputTheme.RealWidth) InputWin_X=w-InputTheme.RealWidth;
			if(InputWin_Y<0) InputWin_Y=0;
			else if(InputWin_Y>h-InputTheme.RealHeight) InputWin_Y=h-InputTheme.RealHeight;
			wui->win_move(win,InputWin_X,InputWin_Y);
		}
		return 1;
	}
	return 0;
}
static int input_enter_leave_notify(w_win_t win,const W_EVENT *e,void *data)
{
	if(e->type==W_MOUSE_LEAVE)
	{
		if(InputWin_Drag && wui->wayland_get_surface(win))
		{
			W_EVENT f={
				.type=W_MOUSE_UP,
				.mouse={
					.button=W_BUTTON_LEFT,
					.x=e->mouse.x,
					.y=e->mouse.y,
				}
			};
			main_click_cb(win,&f,data);
		}
	}
	return 0;
}

static int on_input_draw(w_win_t win,const W_EVENT *e,void *data)
{
	DRAW_CONTEXT1 ctx;
	ui_draw_begin(&ctx,win,e->draw.cr);
	if(InputTheme.shadow.len)
	{
		ui_draw_shadow(&ctx,InputTheme.radius[0],&InputTheme.shadow);
		ui_draw_translate(&ctx,InputTheme.shadow.len,InputTheme.shadow.len);
	}
	ui_draw_input_win(&ctx);
	ui_draw_end(&ctx);
	return TRUE;
}

#if 0
static void redirect_stderr(void)
{
	int fd = open("/tmp/error.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open error");
        return;
    }

    if (dup2(fd, STDERR_FILENO) == -1) {
        perror("dup2 error");
        close(fd);
        return;
    }

    close(fd);
}
#endif

static int input_hook_get_pos(int *x,int *y)
{
	// gtk3-x11下进行光标跟随
	if(!is_wayland)
	{
		if(is_gnome && !ybus_wm_ready())
			return 1;
		*x=InputWin_X;
		*y=InputWin_Y;
		return 0;
	}
	// 配置要求研制成功居中
	if(wayland_tip_center)
	{
		return 1;
	}
	// 让compositor进行光标跟随
	if(InputWin==InputWin_wayland)
	{
		return 2;
	}
	else
	{
		// 手动进行窗口的光标跟随
		*x=InputWin_X;
		*y=InputWin_Y;
		return 0;
	}
}

static int ui_init(void)
{
	// redirect_stderr();
	ui_wait_other();
	wui->init("net.dgod.yong");
	ybus_wayland_ui_init(&input_hook);
	input_hook.get_pos=input_hook_get_pos;
	W_OPTIONS options={
		.translate=y_translate_get_utf8,
		.input_hook=&input_hook,
	};
	const char *workarea=y_im_get_config_data("main","workarea");
	if(workarea)
	{
		int arr[4],ret;
		ret=l_sscanf(workarea,"%d %d %d %d",arr+0,arr+1,arr+2,arr+3);
		if(ret!=4)
		{
			fprintf(stderr,"bad workarea config\n");
		}
		else
		{
			options.workarea=arr;
		}
	}
	wui->set_options(&options);
	wayland_tip_center=y_im_get_config_int("main","wayland_tip_center")?true:false;
	wui->win_connect(NULL,W_CONFIG,on_screen_size_changed,NULL);
	MainWin=wui->win_create("main","layer",NULL);
	wui->win_connect(MainWin,W_MOUSE_ENTER,main_enter_leave_notify,NULL);
	wui->win_connect(MainWin,W_MOUSE_LEAVE,main_enter_leave_notify,NULL);
	wui->win_connect(MainWin,W_MOUSE_DOWN,main_click_cb,NULL);
	wui->win_connect(MainWin,W_MOUSE_UP,main_click_cb,NULL);
	wui->win_connect(MainWin,W_MOUSE_MOVE,main_motion_cb,NULL);
	wui->win_connect(MainWin,W_DRAW,on_main_draw,NULL);

	InputWin=wui->win_create("input","input",NULL);
	wui->win_connect(InputWin,W_MOUSE_DOWN,input_click_cb,NULL);
	wui->win_connect(InputWin,W_MOUSE_UP,input_click_cb,NULL);
	wui->win_connect(InputWin,W_MOUSE_MOVE,input_motion_cb,NULL);
	wui->win_connect(InputWin,W_MOUSE_LEAVE,input_enter_leave_notify,NULL);
	wui->win_connect(InputWin,W_DRAW,on_input_draw,NULL);

	if(wui->wayland_get_surface(InputWin))
	{
		InputWin_wayland=InputWin;
		InputWin_layer=wui->win_create("input","layer",NULL);
		wui->win_connect(InputWin_layer,W_MOUSE_DOWN,input_click_cb,NULL);
		wui->win_connect(InputWin_layer,W_MOUSE_UP,input_click_cb,NULL);
		wui->win_connect(InputWin_layer,W_MOUSE_MOVE,input_motion_cb,NULL);
		wui->win_connect(InputWin_layer,W_MOUSE_LEAVE,input_enter_leave_notify,NULL);
		wui->win_connect(InputWin_layer,W_DRAW,on_input_draw,NULL);
	}
	color_schema_init(YongReloadAll);
	calc_ui_scale();
	return 0;
}

void YongLogWrite(const char *fmt,...)
{
	va_list ap;
	va_start(ap,fmt);
	vfprintf(stderr,fmt,ap);
	va_end(ap);
}

static int ui_loop(void)
{
	return wui->loop();
}

static void ui_clean(void)
{
	/* clean input window */
	for(int i=0;i<3;i++)
	{
		if(InputTheme.bg[i])
		{
			g_object_unref(InputTheme.bg[i]);
			InputTheme.bg[i]=NULL;
		}
		if(InputTheme.rgn[i])
		{
			//gdk_region_destroy(InputTheme.rgn[i]);
			ui_region_destroy(InputTheme.rgn[i]);
			InputTheme.rgn[i]=NULL;
		}
	}

	if(InputTheme.layout)
	{
		ui_font_free(InputTheme.layout);
		InputTheme.layout=NULL;
	}
	if(InputTheme.page.layout)
	{
		ui_font_free(InputTheme.page.layout);
		InputTheme.page.layout=NULL;
	}
	
	wui->win_destroy(InputWin);
	wui->win_destroy(MainWin);

	pango_cairo_font_map_set_default (NULL);
}

static void ui_quit(void)
{
	wui->quit();
}

void ui_win_show(w_win_t w,int show)
{
	if(show)
		wui->win_show(w);
	else
		wui->win_hide(w);
}

static void ui_main_win_hide_timer(void *unused)
{
	ui_win_show(MainWin,0);
}

void *ui_main_win(void)
{
	if(!MainWin)
		return NULL;
	return MainWin;
}

int ui_main_show(int show)
{
	if(show && MainWin_X==0 && MainWin_Y<=0) 			// right bottom
	{
		gint w,h;
		int wa_x,wa_y,wa_w,wa_h;
		w=MainWin_W+2*MainTheme.shadow.len;h=MainWin_H+2*MainTheme.shadow.len;
		wui->get_workarea(MainWin,&wa_x,&wa_y,&wa_w,&wa_h);
		MainWin_X=wa_x+wa_w-w;MainWin_Y=wa_y+wa_h-h;
		wui->win_move(MainWin,MainWin_X,MainWin_Y);
	}
	else if(show && MainWin_X==1 && MainWin_Y==-1)		// top center
	{
		gint w;
		int wa_x,wa_w;
		w=MainWin_W+2*MainTheme.shadow.len;
		wui->get_workarea(MainWin,&wa_x,NULL,&wa_w,NULL);
		MainWin_X=wa_x+(wa_w-w)/2;
		MainWin_Y=0;
		wui->win_move(MainWin,MainWin_X,MainWin_Y);
	}
	else if(show && MainWin_X==2 && MainWin_Y==-1)		// left bottom
	{
		gint wa_x,wa_y,wa_w,wa_h;
		gint h=MainWin_H+2*MainTheme.shadow.len;
		wui->get_workarea(MainWin,&wa_x,&wa_y,&wa_w,&wa_h);
		MainWin_X=wa_x;MainWin_Y=wa_y+wa_h-h;
		wui->win_move(MainWin,wa_x,wa_y+wa_h-h);
	}
	else if(show)
	{
		wui->win_move(MainWin,MainWin_X,MainWin_Y);
	}
	if(show==2)
		show=MainWin_visible?0:1;
	if(show>0)
	{
		ui_timer_del(ui_main_win_hide_timer,NULL);
		MainWin_visible=true;
		ui_win_show(MainWin,1);
		wui->win_redraw(MainWin);
	}
	else if(show==0)
	{
		MainWin_visible=false;
		ui_timer_add(50,ui_main_win_hide_timer,NULL);
	}
	return 0;
}

static int ui_main_update(UI_MAIN *param)
{
	if(MainTheme.bg)
	{
		ui_image_free(MainTheme.bg);
		MainTheme.bg=NULL;
	}
	if(!param->bg)
	{
		wui->win_hide(MainWin);
		return 0;
	}
	MainTheme.scale=param->scale;
	MainTheme.line_width=param->line_width;
	MainTheme.move_style=param->move_style;
	MainTheme.radius=param->radius;
	ui_shadow_init(&MainTheme.shadow,param->shadow);
	if(param->bg[0]=='#')
	{
		MainTheme.bg_color=ui_color_parse(param->bg);
		MainTheme.border=ui_color_parse(param->border);
		MainWin_W=param->rc.w;MainWin_H=param->rc.h;
		if(param->scale!=1 && ui_scale!=1)
		{
			MainWin_W=(int)round(MainWin_W*ui_scale);
			MainWin_H=(int)round(MainWin_H*ui_scale);
			MainTheme.radius=(int)round(param->radius*ui_scale);
			MainTheme.shadow.len=(int)round(MainTheme.shadow.len*ui_scale);
		}
	}
	else
	{
		if(param->scale!=1 && param->force_scale)
		{
			MainTheme.bg=ui_image_load_scale(param->bg,ui_res_scale,param->rc.w,param->rc.h,IMAGE_SKIN);
			MainWin_W=param->rc.w;MainWin_H=param->rc.h;
			if(param->scale!=1 && ui_scale!=1)
			{
				MainWin_W=(int)round(MainWin_W*ui_scale);
				MainWin_H=(int)round(MainWin_H*ui_scale);
			}
		}
		else
		{
			MainTheme.scale=1;
			MainTheme.bg=ui_image_load(param->bg,IMAGE_SKIN);
			ui_image_size(MainTheme.bg,&MainWin_W,&MainWin_H);
		}
	}
	int real_width=MainWin_W+2*MainTheme.shadow.len;
	int real_height=MainWin_H+2*MainTheme.shadow.len;
	wui->win_resize(MainWin,real_width,real_height);
	MainTheme.move=param->move;
	MainWin_X=param->rc.x;MainWin_Y=param->rc.y;
	MainWin_pos_custom=MainWin_Y!=-1;
	MainWin_tran=param->tran;
	MainWin_auto_tran=param->auto_tran;
	int tran=MainWin_tran;
	if(MainWin_auto_tran && !MainWin_over)
		tran=255-(255-tran)*2/3;
	wui->win_tran(MainWin,tran);
	wui->win_effect(MainWin,param->effect);
	ui_main_show(-1);
	wui->win_redraw(MainWin);
	return 0;
}

static void *ui_input_win(void)
{
	if(!InputWin)
		return NULL;
	return InputWin;
}

static UI_IMAGE ui_input_bg_adjust(UI_IMAGE bg,int cand_max,int bottom)
{
	int cand;
	int w,h,w0,h0;
	
	if(InputTheme.Top==InputTheme.Bottom && InputTheme.Top==0)
		return bg;
	if(!bottom) bottom=InputTheme.Bottom;
	if(InputTheme.line==2)
		cand=y_im_get_config_int("IM","cand");
	else
		cand=1;
	
	ui_image_size(bg,&w0,&h0);

	ui_text_size(NULL,InputTheme.layout," ",&w,&h);
	w=w0;
	if(InputTheme.line==0)
	{
		int middle=(InputTheme.CodeY+h0-InputTheme.WorkBottom)/2;
		int pad=InputTheme.CandY-middle;
		double scale=(h+pad+pad+h)*1.0/(h0-InputTheme.CodeY-InputTheme.WorkBottom);
		pad=(int)(scale*pad);
		middle=InputTheme.CodeY+h+pad;
		h=InputTheme.CodeY+h+pad+pad+h+InputTheme.WorkBottom;
		if(h<InputTheme.Top+InputTheme.Bottom)
			return bg;
		InputTheme.CandY=middle+pad;
	}
	else
	{
		h=cand*h+(cand-1)*InputTheme.space+bottom+InputTheme.CandY;
	}
	if(h<InputTheme.Top+InputTheme.Bottom)
		h=InputTheme.Top+InputTheme.Bottom;
		
	if(InputTheme.mHeight>h)
		InputTheme.mHeight=h;


	int extra=h-h0;
	UI_IMAGE res=cairo_image_surface_create(cairo_image_surface_get_format(bg),w,h);
	UI_DC dc=cairo_create(res);
	ui_image_draw_full(dc,bg,0,0,w,InputTheme.Top,0,0,w,InputTheme.Top);
	ui_image_draw_full(dc,bg,0,InputTheme.Top,
			w,h0-InputTheme.Bottom-InputTheme.Top+extra,
			0,InputTheme.Top,w,h0-InputTheme.Bottom-InputTheme.Top);
	ui_image_draw_full(dc,bg,0,h0-InputTheme.Bottom+extra,w,InputTheme.Bottom,
			0,h0-InputTheme.Bottom,w,InputTheme.Bottom);
	cairo_destroy(dc);

	ui_image_free(bg);
	return res;
}

static int get_text_width(const char *s,UI_FONT layout,int *height)
{
	if(!layout)
		layout=InputTheme.layout;
	return ui_text_size(NULL,layout,s,NULL,height);
}

static int get_input_text_width(void)
{
	UI_FONT font=InputTheme.layout;
	return ui_text_size(NULL,font,"A",NULL,NULL);
}

static int get_input_text_height(UI_FONT font)
{
	if(!font) font=InputTheme.layout;
	char temp[8]="\xe6\xb5\x8b";
	int cy;
	get_text_width(temp,font,&cy);
	return cy;
}

static int ui_input_update(UI_INPUT *param)
{
	const char *tmp;
	UI_IMAGE bg;
	int bg_w,bg_h;

	if(InputTheme.layout)
	{
		ui_font_free(InputTheme.layout);
		InputTheme.layout=NULL;
	}
	if(InputTheme.page.layout)
	{
		ui_font_free(InputTheme.page.layout);
		InputTheme.page.layout=NULL;
	}
	for(int i=0;i<3;i++)
	{
		if(InputTheme.bg[i])
		{
			ui_image_free(InputTheme.bg[i]);
			InputTheme.bg[i]=NULL;
		}
		if(InputTheme.rgn[i])
		{
			ui_region_destroy(InputTheme.rgn[i]);
			InputTheme.rgn[i]=NULL;
		}
	}
	for(int i=0;i<2;i++)
	{
		if(InputTheme.page.up[i])
		{
			ui_image_free(InputTheme.page.up[i]);
			InputTheme.page.up[i]=NULL;
		}
		if(InputTheme.page.down[i])
		{
			ui_image_free(InputTheme.page.down[i]);
			InputTheme.page.down[i]=NULL;
		}
	}
	if(InputTheme.pets)
	{
		l_slist_free(InputTheme.pets,(LFreeFunc)ui_pet_free);
		InputTheme.pets=NULL;
	}

	InputTheme.line=param->line;
	InputTheme.caret=param->caret;
	InputTheme.page.show=param->page.show;
	InputTheme.page.text[0]=param->page.text[0];
	InputTheme.page.text[1]=param->page.text[1];
	InputTheme.page.color=param->page.color;
	InputTheme.page.scale=param->page.scale;
	InputTheme.noshow=param->noshow;
	InputTheme.root=param->root;
	InputTheme.space=param->space;
	InputTheme.no=param->no;
	InputTheme.strip=param->strip;
	InputTheme.x=param->x;
	InputTheme.y=param->y;

	InputTheme.mWidth=param->mw;
	InputTheme.mHeight=param->mh;
	
	InputTheme.CodeX=param->code.x;
	InputTheme.CodeY=param->code.y;
	InputTheme.CandX=param->cand.x;
	InputTheme.CandY=param->cand.y;
	InputTheme.OffX=param->off.x;
	InputTheme.OffY=param->off.y;
	
	InputTheme.scale=param->scale;
	InputTheme.radius[0]=param->radius[0];
	InputTheme.radius[1]=param->radius[1];

	ui_shadow_init(&InputTheme.shadow,param->shadow);
	InputTheme.pets=ui_pets_new(param->pets);

	if(InputTheme.scale==1 && ui_scale!=1)
	{
		double temp=ui_scale;
		ui_scale=1;
		InputTheme.layout=ui_font_parse(InputWin,param->font,ui_scale,y_ui_get_scale(1));
		if(param->page.show && param->page.scale!=0 && param->page.scale!=1)
			InputTheme.page.layout=ui_font_parse(InputWin,param->font,ui_scale*param->page.scale,y_ui_get_scale(1));
		ui_scale=temp;
	}
	else
	{
		InputTheme.layout=ui_font_parse(InputWin,param->font,ui_scale,y_ui_get_scale(1));
		if(param->page.show && param->page.scale!=0 && param->page.scale!=1)
			InputTheme.page.layout=ui_font_parse(InputWin,param->font,ui_scale*param->page.scale,y_ui_get_scale(1));
	}
	
	InputTheme.line_width=param->line_width;
	memcpy(InputTheme.pad,param->pad,sizeof(param->pad));

	tmp=param->bg[0];
	assert(tmp!=NULL);
	if(tmp[0]=='#')
	{
		InputTheme.bg_color=ui_color_parse(tmp);
		tmp=param->border;
		InputTheme.border=ui_color_parse(tmp);
		InputTheme.Width=param->w;
		InputTheme.Height=param->h;
		InputTheme.RealHeight=InputTheme.Height;
		InputTheme.RealWidth=InputTheme.Width;
		InputTheme.Left=3;
		InputTheme.Right=3;
		InputTheme.WorkLeft=InputTheme.Left;
		InputTheme.WorkRight=InputTheme.Right;
		InputTheme.WorkBottom=param->work_bottom;
		if(param->sep)
			InputTheme.sep=ui_color_parse(param->sep);
		else
			InputTheme.sep=InputTheme.border;
		
		if(InputTheme.scale!=1 && ui_scale>1)
		{
			InputTheme.space=(int)round(ui_scale*InputTheme.space);
			InputTheme.CodeX=(int)round(ui_scale*InputTheme.CodeX);
			InputTheme.CodeY=(int)round(ui_scale*InputTheme.CodeY);
			InputTheme.CandX=(int)round(ui_scale*InputTheme.CandX);
			InputTheme.mWidth=(int)round(ui_scale*InputTheme.mWidth);
			InputTheme.mHeight=(int)round(ui_scale*InputTheme.mHeight);
			
			int h2=InputTheme.Height/2;
			int pad=InputTheme.CandY-h2;
			h2+=(InputTheme.CodeY-param->code.y)*2;
			InputTheme.RealHeight=InputTheme.Height=2*h2;
			InputTheme.CandY=h2+(int)round(pad*ui_scale);
			InputTheme.WorkBottom=(int)round(ui_scale*InputTheme.WorkBottom);

			InputTheme.pad[0]*=ui_scale;
			InputTheme.pad[1]*=ui_scale;
			InputTheme.pad[2]*=ui_scale;
			InputTheme.pad[3]*=ui_scale;

			InputTheme.radius[0]=(int)round(ui_scale*InputTheme.radius[0]);
			InputTheme.radius[1]=(int)round(ui_scale*InputTheme.radius[1]);
			
			InputTheme.shadow.len=(int)round(ui_scale*InputTheme.shadow.len);
		}
		if(InputTheme.WorkBottom==0)
		{
			InputTheme.WorkBottom=InputTheme.CodeY;
		}
	}
	else
	{
		if(param->sep)
			InputTheme.sep=ui_color_parse(param->sep);
		else
			InputTheme.sep=(UI_COLOR){0};
		InputTheme.Left=param->left;;
		InputTheme.Right=param->right;
		InputTheme.Top=param->top;
		InputTheme.Bottom=param->bottom;
		if(param->scale!=1 && param->force_scale)
		{
			bg=ui_image_load_scale(tmp,ui_res_scale,param->w,param->h,IMAGE_SKIN);
			ui_image_size(bg,&bg_w,&bg_h);
			if(bg_w!=param->w)
			{
				// adjust all size here
				double scale=bg_w*1.0L/param->w/ui_surface_scale;
				InputTheme.line_width=(int)round(scale*InputTheme.line_width);
				InputTheme.Width=(int)round(scale*InputTheme.Width);
				InputTheme.Height=(int)round(scale*InputTheme.Height);
				InputTheme.mWidth=(int)round(scale*InputTheme.mWidth);
				InputTheme.mHeight=(int)round(scale*InputTheme.mHeight);
				InputTheme.Left=(int)round(scale*InputTheme.Left);
				InputTheme.Right=(int)round(scale*InputTheme.Right);
				InputTheme.Top=(int)round(scale*InputTheme.Top);
				InputTheme.Bottom=(int)round(scale*InputTheme.Bottom);

				InputTheme.space=(int)round(scale*InputTheme.space);
				InputTheme.CodeX=(int)round(scale*InputTheme.CodeX);
				InputTheme.CodeY=(int)round(scale*InputTheme.CodeY);
				InputTheme.CandX=(int)round(scale*InputTheme.CandX);
				InputTheme.CandY=(int)round(scale*InputTheme.CandY);

				param->work_left=(int)round(scale*param->work_left);
				param->work_right=(int)round(scale*param->work_right);
				param->work_bottom=(int)round(scale*param->work_bottom);

				if(scale!=1 && !y_im_has_config("input","font"))
				{
					ui_font_free(InputTheme.layout);
					double temp=ui_scale;
					ui_scale=scale;
					InputTheme.layout=ui_font_parse(InputWin,param->font,ui_scale,y_ui_get_scale(1));
					ui_scale=temp;
				}

				InputTheme.pad[0]*=scale;
				InputTheme.pad[1]*=scale;
				InputTheme.pad[2]*=scale;
				InputTheme.pad[3]*=scale;
			}
		}
		else
		{
			bg=ui_image_load_scale(tmp,ui_surface_scale,-1,-1,IMAGE_SKIN);
		}
		if(param->work_bottom>0)
		{
			InputTheme.WorkBottom=param->work_bottom;
		}
		else
		{
			InputTheme.WorkBottom=InputTheme.CodeY;
		}	
		if(param->work_left>0 || param->work_right>0)
		{
			InputTheme.WorkLeft=param->work_left;
			InputTheme.WorkRight=param->work_right;
		}
		else
		{
			InputTheme.WorkLeft=InputTheme.Left;
			InputTheme.WorkRight=InputTheme.Right;
		}

		bg=ui_input_bg_adjust(bg,param->cand_max,param->work_bottom);

		ui_image_size(bg,&bg_w,&bg_h);
		bg_w=(int)round(bg_w/ui_surface_scale);
		bg_h=(int)round(bg_h/ui_surface_scale);
		// printf("bg_w=%d,bg_h=%d\n",bg_w,bg_h);

		InputTheme.RealHeight=bg_h;
	
		InputTheme.Width=bg_w;
		InputTheme.Height=bg_h;

		if(!InputTheme.Left && !InputTheme.Right)
		{
			InputTheme.bg[1]=bg;
		}
		else
		{
			int bg_w,bg_h;
			int ileft=(int)round(InputTheme.Left*ui_res_scale/ui_scale);
			int iright=(int)round(InputTheme.Right*ui_res_scale/ui_scale);
			ui_image_size(bg,&bg_w,&bg_h);
			if(InputTheme.Left)
			{
				InputTheme.bg[0]=ui_image_part(bg,0,0,ileft,bg_h);
				InputTheme.rgn[0]=ui_image_region(InputTheme.bg[0],ui_scale/ui_res_scale);
			}
			InputTheme.bg[1]=ui_image_part(bg,ileft,0,bg_w-ileft-iright,bg_h);
			InputTheme.rgn[1]=ui_image_region(InputTheme.bg[1],ui_scale/ui_res_scale);
			if(InputTheme.rgn[1])
			{
				cairo_region_translate(InputTheme.rgn[1],InputTheme.Left,0);
				cairo_region_get_extents(InputTheme.rgn[1],&InputTheme.clip);
				ui_region_destroy(InputTheme.rgn[1]);
				InputTheme.rgn[1]=0;
			}
			if(InputTheme.Right)
			{
				InputTheme.bg[2]=ui_image_part(bg,bg_w-iright,0,iright,bg_h);
				InputTheme.rgn[2]=ui_image_region(InputTheme.bg[2],ui_scale/ui_res_scale);
			}
			ui_image_free(bg);
		}
	}

	InputTheme.bg_first=ui_color_parse(param->bg[1]?param->bg[1]:"#FFFFFF00");

	InputTheme.text[0]=ui_color_parse(param->text[0]);
	InputTheme.text[1]=ui_color_parse(param->text[1]);
	InputTheme.text[2]=ui_color_parse(param->text[2]);
	InputTheme.text[3]=ui_color_parse(param->text[3]);
	InputTheme.text[4]=ui_color_parse(param->text[4]);
	InputTheme.text[5]=ui_color_parse(param->text[5]);
	InputTheme.text[6]=ui_color_parse(param->text[6]);

	if(InputTheme.page.show)
	{
		InputTheme.page.space=MAX(get_input_text_width(),InputTheme.space);
		if(InputTheme.page.text[0]==1)
		{
			UI_FONT font=InputTheme.page.layout?InputTheme.page.layout:InputTheme.layout;
			int size=get_input_text_height(font);
			InputTheme.page.size=size;
			size=(int)round(size*ui_surface_scale);
			InputTheme.page.up[0]=ui_image_load_page(size,InputTheme.text[4],true);
			InputTheme.page.up[1]=ui_image_load_page(size,InputTheme.page.color,true);
			InputTheme.page.down[0]=ui_image_load_page(size,InputTheme.text[4],false);
			InputTheme.page.down[1]=ui_image_load_page(size,InputTheme.page.color,false);
		}
	}

	InputTheme.RealWidth=2*InputTheme.RealHeight;
	InputTheme.MaxHeight=InputTheme.RealHeight;

	if(InputTheme.line==1 && !InputTheme.CandY)
		InputTheme.CandY=InputTheme.CodeY;
		
	if(!InputTheme.bg[1])
	{
		int cy=get_input_text_height(NULL);
		if(InputTheme.line==0 || InputTheme.line==2)
		{
			int pad=InputTheme.CandY-InputTheme.Height/2;
			if(InputTheme.CodeY<0 && cy>InputTheme.Height-2*InputTheme.CandY)
			{
				InputTheme.Height=InputTheme.CandY*2+cy;
				InputTheme.RealHeight=InputTheme.Height;
			}
			else if(InputTheme.CodeY+cy>InputTheme.Height/2-pad)
			{
				InputTheme.CandY=InputTheme.CodeY+cy+2*pad;
				InputTheme.Height=InputTheme.CandY+cy+InputTheme.CodeY;
				InputTheme.RealHeight=InputTheme.Height;
			}
			InputTheme.Middle=InputTheme.Height/2;
		}
		else if(InputTheme.line==1)
		{
			if(2*InputTheme.CodeY+cy>InputTheme.Height)
			{
				InputTheme.Height=2*InputTheme.CodeY+cy;
				InputTheme.RealHeight=InputTheme.Height;
			}
		}
	}
	else
	{
		if(InputTheme.line==0 || InputTheme.line==2)
		{
			int cy=get_input_text_height(NULL);
			int pad=(InputTheme.CandY-InputTheme.CodeY-cy)/2;
			InputTheme.Middle=InputTheme.CandY-pad;
		}
	}
	wui->win_tran(InputWin,param->tran);
	wui->win_effect(InputWin,param->effect);
	if(InputTheme.noshow==2)
		y_ui_input_draw();
	
	return 0;
}

static int YongCodeWidth(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	int ret;
	
	if(InputTheme.CodeY<0)
	{
		im.CodePos[0]=im.CodePos[1]=im.CodePos[2]=InputTheme.CodeX;
		return 0;
	}

	im.CodePos[0]=InputTheme.CodeX;
	if(eim && eim->StringGet[0])
	{
		ret=get_text_width(im.StringGet,InputTheme.layout,NULL);
		im.CodePos[1]=im.CodePos[0]+ret;
	}
	else
	{
		im.CodePos[1]=InputTheme.CodeX;
	}

	ret=get_text_width(im.CodeInput,InputTheme.layout,&im.cursor_h);
	im.CodePos[3]=im.CodePos[1]+ret;
	if(!eim || eim->CaretPos==-1 || !im.CodeInput[eim->CaretPos])
	{
		im.CodePos[2]=im.CodePos[1]+ret;
	}
	else
	{
		int CaretPos=im.CaretPos>=0?im.CaretPos:eim->CaretPos;
		char tmp=im.CodeInput[CaretPos];
		im.CodeInput[CaretPos]=0;
		im.CodePos[2]=get_text_width(im.CodeInput,InputTheme.layout,NULL)+im.CodePos[1];
		im.CodeInput[CaretPos]=tmp;
	}
	// return (int)im.CodePos[3]+InputTheme.CodeX-InputTheme.WorkLeft;
	return (int)im.CodePos[3];
}

static int YongPageWidth(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	int ret=0;
	if(/*!im.EnglishMode && */eim && eim->CandPageCount>1)
	{
		int h;
		UI_FONT font=InputTheme.page.layout?InputTheme.page.layout:InputTheme.layout;
		if(InputTheme.page.text[0]==0)
		{
			sprintf(im.Page,"%d/%d",eim->CurCandPage+1,eim->CandPageCount);
			im.PageLen[0]=ui_text_size(NULL,font,im.Page,NULL,&h);
			im.PageLen[1]=0;
			im.PageLen[2]=0;
			ret=im.PageLen[0];
		}
		else if(InputTheme.page.text[0]==1 && InputTheme.page.up[0])
		{
			double scale=InputTheme.scale!=1?ui_scale:1;
			im.PageLen[0]=InputTheme.page.size;
			im.PageLen[1]=round(4*scale*InputTheme.page.scale);
			im.PageLen[2]=InputTheme.page.size;
		}
		else
		{
			double scale=InputTheme.scale!=1?ui_scale:1;
			int pos;
			pos=l_unichar_to_utf8(InputTheme.page.text[0],(uint8_t*)im.Page);
			im.Page[pos]=0;
			im.PageLen[0]=ui_text_size(NULL,font,im.Page,NULL,&h);
			im.PageLen[1]=round(4*scale*InputTheme.page.scale);
			pos=l_unichar_to_utf8(InputTheme.page.text[1],(uint8_t*)im.Page);
			im.Page[pos]=0;
			im.PageLen[2]=ui_text_size(NULL,font,im.Page,NULL,&h);
			pos=l_unichar_to_utf8(InputTheme.page.text[0],(uint8_t*)im.Page);
			pos+=l_unichar_to_utf8(InputTheme.page.text[1],(uint8_t*)im.Page+pos);
			im.Page[pos]=0;	
		}
		if(InputTheme.page.text[0]==1 && InputTheme.page.up[0])
		{
			int h;
			ui_image_size(InputTheme.page.up[0],NULL,&h);
			im.PagePosY=InputTheme.CodeY+(im.cursor_h-h)/2.0;
		}
		if(font!=InputTheme.layout)
		{
			// printf("%d %d %d\n",(int)im.PageLen[0],(int)im.PageLen[1],(int)im.PageLen[2]);
			im.PagePosY=InputTheme.CodeY+(im.cursor_h-im.cursor_h*InputTheme.page.scale)/2;
		}
		else
		{
			im.PagePosY=InputTheme.CodeY;
		}
		ret=im.PageLen[0]+im.PageLen[1]+im.PageLen[2];
	}
	return ret;
}

static int YongCandWidth(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	int i,count;
	double cur=0;
	int cur_y;
	double *width,*height;

	if(!eim) return 0;
	count=eim->CandWordCount;

	width=im.CandPosX+count*3;
	height=im.CandPosY+count*3;

	cur=InputTheme.CandX;
	cur_y=InputTheme.CandY;
	*width=cur;*height=cur_y;
	for(i=0;i<count;i++)
	{
		double *pos;
		int h,h1,h2,h3=0;

		pos=im.CandPosX+i*3;

		pos[0]=cur;
		if(InputTheme.no==0 || InputTheme.no==2)
		{
			cur+=get_text_width(YongGetSelectNumber(i),InputTheme.layout,&h1);
		}
		else
		{
			h1=0;
		}

		pos[1]=cur;
		cur+=get_text_width(im.CandTable[i],NULL,&h2);
		h=MAX(h1,h2);		

		pos[2]=cur;
		if(im.Hint)
		{
			//char *t=eim->CodeTips[i];
			char *t=im.CodeTips[i];
			if(t && *t)
			{
				cur+=get_text_width(t,NULL,&h3);
				h=MAX(h,h3);
			}
		}
		*width=MAX(*width,cur);
		im.CandWidth[i]=cur-pos[0];
		im.CandHeight[i]=h;
		if(i!=count-1)
			cur+=InputTheme.space;

		pos=im.CandPosY+i*3;
		pos[0]=cur_y+(h-h1+1)/2;
		pos[1]=cur_y+(h-h2+1)/2;
		pos[2]=cur_y+(h-h3+1)/2;
		if(InputTheme.line==2)
		{
			if(i==count-1)
			{
				//*height+=h+InputTheme.CodeY;
				*height+=h;
			}
			else
			{
				*height+=h+InputTheme.space;
			}
			cur_y+=h+InputTheme.space;

			cur=InputTheme.CandX;
		}
	}
	return (int)*width;
}
int YongDrawInput(void)
{
	int TempWidth,DeltaWidth;
	int TempHeight,DeltaHeight;
	int CodeWidth,CandWidth=0,PageWidth=0;
	EXTRA_IM *eim=CURRENT_EIM();
	int count=0;

	if(!InputWin) return 0;

	if(!im.CodeInputEngine[0] && (!eim || !eim->StringGet[0]) && InputTheme.noshow!=2)
	{
		YongShowInput(0);
		return 0;
	}
	if(InputTheme.CodeY<0 && (!eim || eim->CandWordCount<=0))
	{
		YongShowInput(0);
		return 0;
	}
	if(y_xim_get_onspot() && InputTheme.line==1 && im.Preedit==1)
		CodeWidth=0;
	else
		CodeWidth=YongCodeWidth();
	if(InputTheme.page.show)
		PageWidth=YongPageWidth();
	if(eim) count=eim->CandWordCount;
	if(count)
	{
		int i;
		for(i=0;i<count;i++)
		{
			int len=eim->CodeLen;
			if(eim->WorkMode!=EIM_WM_NORMAL)
			{	
				y_im_key_desc_translate(eim->CodeTips[i],NULL,0,eim->CandTable[i],
					im.CodeTips[i],MAX_TIPS_LEN+1);
			}
			else
			{
				y_im_key_desc_translate(eim->CodeInput,eim->CodeTips[i],len,eim->CandTable[i],
					im.CodeTips[i],MAX_TIPS_LEN+1);
			}
			if(eim->WorkMode==EIM_WM_QUERY)
			{
				char *s=eim->CandTable[i];
				y_im_key_desc_translate(s,NULL,0,eim->CodeInput,im.CandTable[i],MAX_TIPS_LEN+1);
			}
			else
			{
				const char *s=eim->CandTable[i];
				y_im_disp_cand(s,im.CandTable[i],(InputTheme.strip>>(16*(i==eim->SelectIndex)+0))&0xff,
					(InputTheme.strip>>(16*(i==eim->SelectIndex)+8))&0xff,
					eim->CodeInput,eim->CodeTips[i]);
			}
		}
		CandWidth=YongCandWidth();
	}
	if(InputTheme.line==0)
	{
		if(PageWidth)
			im.PagePosX=MAX(CandWidth-PageWidth,CodeWidth+InputTheme.page.space);
		CodeWidth+=InputTheme.CodeX-InputTheme.WorkLeft;
		if(PageWidth)
			CodeWidth+=InputTheme.page.space+PageWidth;
		CandWidth+=InputTheme.CandX-InputTheme.WorkLeft;
		TempWidth=MAX(CodeWidth,CandWidth);
		TempHeight=InputTheme.RealHeight;
	}
	else if(InputTheme.line==1)
	{
		int pad=InputTheme.CodeX-InputTheme.WorkLeft;
		TempWidth=CodeWidth+CandWidth+pad;
		if(PageWidth)
		{
			im.PagePosX=TempWidth+InputTheme.page.space-pad;
			TempWidth+=InputTheme.page.space+PageWidth;			
		}
		if(eim)
		{
			int i,count;
			count=eim->CandWordCount;
			for(i=0;i<=count;i++)
			{
				double *pos=im.CandPosX+i*3;
				pos[0]+=CodeWidth;
				pos[1]+=CodeWidth;
				pos[2]+=CodeWidth;
			}
		}
		TempHeight=InputTheme.RealHeight;
	}
	else
	{
		double *pos=im.CandPosY+count*3;
		int pad=InputTheme.CodeX-InputTheme.WorkLeft;
		CodeWidth+=pad;
		if(PageWidth)
			CodeWidth+=InputTheme.page.space+PageWidth;
		CandWidth+=InputTheme.CandX-InputTheme.WorkLeft;
		TempWidth=MAX(CodeWidth,CandWidth);
		if(InputTheme.CodeY>=0)
			TempHeight=(int)pos[0]+InputTheme.CodeY;
		else
			TempHeight=(int)pos[0]+InputTheme.CandY;
		if(TempHeight<InputTheme.Height)
			TempHeight=InputTheme.Height;
		if(InputTheme.bg[1])
			TempHeight=InputTheme.RealHeight;
	}
	TempWidth+=InputTheme.WorkRight;
	if(InputTheme.line!=2 && !InputTheme.mWidth && TempWidth<InputTheme.RealHeight*2)
		TempWidth=InputTheme.RealHeight*2;
	if(TempWidth<InputTheme.mWidth)
		TempWidth=InputTheme.mWidth;
	if(InputTheme.line==2 && TempHeight<InputTheme.mHeight)
		TempHeight=InputTheme.mHeight;

	// 只调整宽度，高度调整的话，底部的内边距会比较不自然
	int WidthWithShadow=TempWidth+2*InputTheme.shadow.len;
	wui->win_preferred_size(InputWin,&WidthWithShadow,NULL);
	TempWidth=WidthWithShadow-2*InputTheme.shadow.len;

	DeltaWidth=TempWidth-InputTheme.RealWidth;
	DeltaHeight=TempHeight-InputTheme.RealHeight;
	if(DeltaWidth || DeltaHeight)
	{
		InputTheme.RealWidth=TempWidth;
		InputTheme.RealHeight=TempHeight;

		int real_width=InputTheme.RealWidth+2*InputTheme.shadow.len;
		int real_height=InputTheme.RealHeight+2*InputTheme.shadow.len;
		wui->win_resize(InputWin,real_width,real_height);
	}
	// if(PageWidth && InputTheme.line==2)
	{
		im.PagePosX=InputTheme.RealWidth-InputTheme.WorkRight-
			(InputTheme.CodeX-InputTheme.WorkLeft)-PageWidth;
	}
	InputTheme.MaxHeight=MAX(InputTheme.MaxHeight,InputTheme.RealHeight);
	YongMoveInput(POSITION_ORIG,POSITION_ORIG);
	ybus_ibus_input_draw(InputTheme.line);
	YongShowInput(1);

	wui->win_redraw(InputWin);
	/* show at preedit area */
	if(((eim && !eim->CandWordCount) || im.Preedit==1) && (im.CodeInput[0]||im.StringGetEngine[0]))
	{
		if(im.StringGetEngine[0] || (eim && eim->CaretPos>=0 && eim->CaretPos<eim->CodeLen))
		{
			uint8_t temp[MAX_CAND_LEN+1];
			strcpy((char*)temp,im.StringGet);
			if(eim && eim->CaretPos>=0 && eim->CaretPos<eim->CodeLen)
			{
				int CaretPos=im.CaretPos>=0?im.CaretPos:eim->CaretPos;
				l_utf8_strncpy(temp+strlen((char*)temp),(uint8_t*)im.CodeInput,CaretPos);
				strcat((char*)temp,"|");
				strcat((char*)temp,(char*)l_utf8_offset((uint8_t*)im.CodeInput,CaretPos));
			}
			else
			{
				strcat((char*)temp,im.CodeInput);
			}
			y_xim_preedit_draw((char*)temp,-1);
		}
		else
		{
			y_xim_preedit_draw((char*)im.CodeInput,-1);
		}
	}
	else if(im.Preedit==0 && eim && eim->CandWordCount)
	{
		if(eim->SelectIndex>=0)
			y_xim_preedit_draw(im.CandTable[eim->SelectIndex],-1);
	}
	return 0;
}

static int ui_input_redraw(void)
{
	return wui->win_redraw(InputWin);
}

static int ui_input_move(int off,int *x,int *y)
{
	int height=InputTheme.RealHeight;

	if(wui->wayland_get_surface(InputWin))
	{
		// input panel can't not move
		if(InputWin==InputWin_wayland)
			return 0;
		// we don't know where to put if multi outputs
		if(wui->get_n_outputs()>1 && *x!=POSITION_ORIG)
			return 0;
	}

	if(!InputTheme.bg[1])
		height=InputTheme.MaxHeight;

	if(*x==POSITION_ORIG)
	{
		int w,h;
		wui->get_workarea(InputWin,x,y,&w,&h);
		*y=*y+h-height;
		
		if(InputTheme.y!=0)
		{
			if(InputTheme.y==-1 && InputTheme.x==1)
			{
				*y=(h-height)/2;
			}
			else if(InputTheme.y!=-1)
			{
				*x=InputTheme.x;*y=InputTheme.y;
			}
		}
	}

	int scr_w,scr_h;
	wui->get_output_size(MainWin,&scr_w,&scr_h);
	if(*y>=scr_h || *x>=2*scr_w)
	{
		/* xpos may big than scr_w, so use 2*scr_w */
		/* found bad pos is always just the ypos */
		return -1;
	}

	if(off)
	{
		if(*y+height+InputTheme.OffY<=scr_h)
			*y+=InputTheme.OffY;
		else *y-=height+18+30;
		*x+=InputTheme.OffX;
	}
	if(*x<0) *x=0;
	else if(*x+InputTheme.RealWidth>scr_w)
		*x=scr_w-InputTheme.RealWidth;

	if(*y<0) *y=0;
	else if(*y+height>scr_h)
		*y=scr_h-height-18-30;

	if(*x==InputWin_X && *y==InputWin_Y)
		return 0;

	int real_x=*x-InputTheme.shadow.len;
	int real_y=*y-InputTheme.shadow.len;

	wui->win_move(InputWin,real_x,real_y);
	InputWin_X=*x;
	InputWin_Y=*y;

	return 0;
}

static int ui_input_show(int show)
{
	if(show)
	{
		if(ybus_ibus_input_draw(InputTheme.line)==0)
			return 0;
		int real_x=InputWin_X-InputTheme.shadow.len;
		int real_y=InputWin_Y-InputTheme.shadow.len;
		wui->win_move(InputWin,real_x,real_y);
		ui_win_show(InputWin,1);
	}
	else
	{
		wui->win_get_pos(InputWin,&InputWin_X,&InputWin_Y);
		InputWin_X+=InputTheme.shadow.len;
		InputWin_Y+=InputTheme.shadow.len;
		CONNECT_ID *id=y_xim_get_connect();
		if(id)
		{
			id->x=InputWin_X;
			id->y=InputWin_Y;
		}
		ybus_ibus_input_hide();
		ui_win_show(InputWin,0);
	}
	return 0;
}

static void ui_show_message(const char *s)
{
	char temp[2048*3];
	y_im_str_encode(s,temp,0);
	wui->win_alert(MainWin,YT("Yong输入法"),temp);
}

static void show_system_info(void)
{
	char temp[4096];
	int pos=0;
	const char *p;
	p=getenv("LANG");if(!p) p="";
	pos+=sprintf(temp+pos,"LANG=%s\n",p);
	p=getenv("LC_CTYPE");if(!p) p="";
	pos+=sprintf(temp+pos,"LC_CTYPE=%s\n",p);
	p=getenv("XMODIFIERS");if(!p) p="";
	pos+=sprintf(temp+pos,"XMODIFIERS=%s\n",p);
	p=getenv("GTK_IM_MODULE");if(!p) p="";
	pos+=sprintf(temp+pos,"GTK_IM_MODULE=%s\n",p);
	p=getenv("QT_IM_MODULE");if(!p) p="";
	pos+=sprintf(temp+pos,"QT_IM_MODULE=%s\n",p);
	sprintf(temp+pos,"SCALE=%.2f\n",ui_scale);
	// pos+=sprintf(temp+pos,"XDG_CONFIG_HOME=%s",getenv("XDG_CONFIG_HOME")?:"");
	ui_show_message(temp);
}

static void on_status_activate(void)
{
	y_xim_enable(-1);
}

static GMenu *im_list_menu2(void)
{
	char name[128];
	GMenu *menu=g_menu_new();
	for(int i=0;i<32;i++)
	{
		char *tmp=y_im_get_im_name(i);
		if(!tmp) break;
		l_gb_to_utf8(tmp,name,sizeof(name));
		l_free(tmp);

		GMenuItem *item=g_menu_item_new(name,"app.im_list");
		g_menu_item_set_action_and_target(item,"app.im_list","i",i);
		g_menu_append_item(menu,item);
		g_object_unref(item);		
	}

	GMenu *group=g_menu_new();
	y_im_str_encode(YT("\xc4\xac\xc8\xcf"),name,0);	// 默认
	g_menu_append(group,name,"app.im_default");
	g_menu_append_section(menu,NULL,G_MENU_MODEL(group));
	g_object_unref(group);
	return menu;
}

static void ui_menu_free(ui_menu_t *m);

static void ui_add_menu2(ui_menu_t *m,GMenu *parent,const char *group)
{
	char *child,*name;
	char temp[128];
	GMenuItem *item;
	
	if(!strcmp(group,"-"))
	{
		GMenu *section=g_menu_new();
		g_menu_append_section(parent,NULL,G_MENU_MODEL(section));
		g_object_unref(section);
		return;
	}
	name=l_key_file_get_string(m->config,group,"name");
	if(name)
	{
		char gb[128];
		l_utf8_to_gb(name,gb,sizeof(gb));
		l_gb_to_utf8(YT(gb),temp,sizeof(temp));
		l_free(name);
	}
	else
	{
		temp[0]=0;
	}
	child=l_key_file_get_string(m->config,group,"child");
	if(child)
	{
		char **list;
		GMenu *me=g_menu_new();
		if(!parent)
			m->root_g=me;
		if(parent)
		{
			item=g_menu_item_new_submenu(temp,G_MENU_MODEL(me));
			g_object_unref(me);
			g_menu_append_item(parent,item);
			g_object_unref(item);
		}
		list=l_strsplit(child,' ');
		l_free(child);
		for(int i=0;list[i]!=NULL;i++)
		{
			if(!strcmp(list[i],"-"))
			{
				GMenu *group=g_menu_new();
				g_menu_append_section(me,NULL,G_MENU_MODEL(group));
				me=group;
				g_object_unref(group);
				continue;
			}
			ui_add_menu2(m,me,list[i]);
		}
		l_strfreev(list);
	}
	else
	{
		char *exec;
		if(m->count>=64)
			return;
		exec=l_key_file_get_string(m->config,group,"exec");
		if(!exec)
			return;
		if(!m->mb && (!strcmp(exec,"$MBO") || !strcmp(exec,"$MBM") || 
						!strcmp(exec,"$MBEDIT")))
		{
			l_free(exec);
			return;
		}
		if(!strcmp(exec,"$OUTPUT"))
		{
			l_free(exec);
			return;
		}
		if(!strcmp(exec,"$MBEDIT") && !y_im_has_config("table","edit"))
		{
			l_free(exec);
			return;
		}
		if(!strncmp(exec,"$GO(yong-config ",16) &&
				!l_file_exists("/usr/bin/yong-config") &&
				!l_file_exists("./yong-config"))
		{
			l_free(exec);
			return;
		}
		if(!strcmp(exec,"$IMLIST"))
		{
			l_free(exec);
			int ybus_ibus_use_ibus_menu(void);
			if(!ybus_ibus_use_ibus_menu())
			{
				GMenu *me=im_list_menu2();
				g_menu_append_submenu(parent,temp,G_MENU_MODEL(me));
				g_object_unref(me);
			}
		}
		else
		{
			if(!strcmp(exec,"$KEYMAP"))
			{
				char keymap[128];
				if(0!=y_im_get_keymap(keymap,128))
				{
					l_free(exec);
					return;
				}
				snprintf(temp,sizeof(temp),"%s",keymap);
			}
			else if(!strcmp(exec,"$HELP(main)"))
			{
				char desc[128];
				if(y_im_help_desc("main",desc,128)!=0)
				{
					l_free(exec);
					return;
				}
				snprintf(temp,sizeof(temp),"%s",desc);
			}
			else if(!strcmp(exec,"$HELP(?)"))
			{
				char item[64];
				char desc[128];
				if(y_im_get_current(item,64) || y_im_help_desc(item,desc,128))
				{
					l_free(exec);
					return;
				}
				snprintf(temp,sizeof(temp),"%s",desc);
			}
			item=g_menu_item_new(temp,NULL);
			g_menu_item_set_action_and_target(item,"app.menu_cmd","i",m->count);
			g_menu_append_item(parent,item);
			g_object_unref(item);
			m->cmd[m->count++]=exec;
		}
	}
}

static void ui_menu_on_cmd2 (GSimpleAction* self,GVariant* parameter,ui_menu_t *m)
{
	gchar *name;
	g_object_get(self,"name",&name,NULL);
	gint32 param_i=0;
	if(parameter)
		param_i=g_variant_get_int32(parameter);
	if(!strcmp(name,"im_list"))
	{
		if(param_i==im.Index)
			return;
		YongSwitchIM(param_i);
	}
	else if(!strcmp(name,"im_default"))
	{
		y_im_set_default(im.Index);
	}
	else
	{
		const char *exec=m->cmd[param_i];
		if(!strcmp(exec,"$EXIT"))
		{
			y_ui_quit();
		}
		else if(!strcmp(exec,"$SYSINFO"))
		{
			show_system_info();
		}
		else
		{
			y_im_handle_menu(exec);
		}
	}
	g_free(name);
}

static ui_menu_t *ui_build_menu2(LKeyFile *kf)
{
	ui_menu_t *m;
	char *engine;
		
	m=l_new0(ui_menu_t);
	m->config=kf;
	m->base=1500;
	engine=y_im_get_current_engine();
	if(engine && !strcmp(engine,"libmb.so"))
		m->mb=1;
	ui_add_menu2(m,m->root_g,"root");
	GSimpleActionGroup *actions = g_simple_action_group_new();
	GSimpleAction *action = g_simple_action_new("menu_cmd", G_VARIANT_TYPE_INT32);
	g_signal_connect(G_OBJECT(action),"activate",
						G_CALLBACK(ui_menu_on_cmd2),m);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
	g_object_unref(action);
	GVariant *state=g_variant_new_int32(im.Index);
	action = g_simple_action_new_stateful("im_list", G_VARIANT_TYPE_INT32,state);
	g_signal_connect(G_OBJECT(action),"activate",
						G_CALLBACK(ui_menu_on_cmd2),m);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
	g_object_unref(action);
	action = g_simple_action_new("im_default", NULL);
	g_signal_connect(G_OBJECT(action),"activate",
						G_CALLBACK(ui_menu_on_cmd2),m);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
	g_object_unref(action);
	m->actions=actions;
	
	g_object_weak_ref(G_OBJECT(m->actions),(GWeakNotify)ui_menu_free,m);

	return m;
}

static void ui_menu_free(ui_menu_t *m)
{
	if(!m)
		return;
	for(int i=0;i<m->count;i++)
		l_free(m->cmd[i]);
	l_free(m);
}

static void ui_popup_menu(UI_EVENT *event)
{
	LKeyFile *kf=y_im_get_menu_config();
	ui_menu_t *m=ui_build_menu2(kf);
	wui->win_popup_menu(MainWin,m->root_g,m->actions,0);
}

static void ui_tray_update(UI_TRAY *param)
{
	if(!param->enable)
	{
		if(StatusIcon)
		{			
			g_object_unref(G_OBJECT(StatusIcon));
			StatusIcon=NULL;
		}
		return;
	}
	char icon1[256],icon2[256];
	int ret=ui_image_path(param->icon[0],icon1,IMAGE_SKIN|IMAGE_SKIN_DEF);
	ret|=ui_image_path(param->icon[1],icon2,IMAGE_SKIN|IMAGE_SKIN_DEF);
	if(ret==0 || strstr(icon1,".zip/"))
	{
		snprintf(icon1,sizeof(icon1),"%s/skin/%s",y_im_get_path("DATA"),"tray1.png");
		snprintf(icon2,sizeof(icon2),"%s/skin/%s",y_im_get_path("DATA"),"tray2.png");
	}
	l_fullpath(icon1,icon1,sizeof(icon1));
	l_fullpath(icon2,icon2,sizeof(icon2));
	if(!StatusIcon)
	{
		StatusIcon=app_indicator_new("net.dgod.yong",icon2,NULL);
		if(!StatusIcon)
			return;
		app_indicator_set_activate_callback(StatusIcon,(void*)on_status_activate,NULL);
		app_indicator_set_secondary_activate_callback(StatusIcon,(void*)on_status_activate,NULL);
		app_indicator_set_attention_icon(StatusIcon,icon1);
	}
	else
	{
		app_indicator_set_icon(StatusIcon,icon2);
		app_indicator_set_attention_icon(StatusIcon,icon1);
	}
	LKeyFile *kf=y_im_get_menu_config();
	ui_menu_t *m=ui_build_menu2(kf);
	app_indicator_set_menu_full(StatusIcon,m->root_g,m->actions);
	app_indicator_set_status(StatusIcon,IconSelected?APP_INDICATOR_STATUS_ACTIVE:APP_INDICATOR_STATUS_NEEDS_ATTENTION);
}

void ui_tray_tooltip(const char *tip)
{
	char tip2[32];
	if(!StatusIcon) return;
	y_im_str_encode(tip,tip2,0);
	app_indicator_set_tooltip(StatusIcon, NULL, tip2, NULL);
}

void ui_tray_status(int which)
{
	if(which!=1 && which!=0)
		return;
	if(IconSelected==which)
		return;
	IconSelected=which;
	if(!StatusIcon)
		return;
	app_indicator_set_status(StatusIcon,which?APP_INDICATOR_STATUS_ACTIVE:APP_INDICATOR_STATUS_NEEDS_ATTENTION);
}

static char *ui_get_select(int (*cb)(const char *))
{
	char *text=wui->clipboard_get_text();
	if(!text)
	{
		if(cb)
			cb(NULL);
		return NULL;
	}
	int len=strlen(text);
	if(len==0 || len>256)
	{
		l_free(text);
		if(cb)
		cb(NULL);
		return NULL;
	}
	char phrase[1024];
	y_im_str_encode_r(text,phrase);
	l_free(text);
	if(cb)
	{
		int ret=cb(phrase);
		if(ret==IMR_DISPLAY)
		{
			EXTRA_IM *eim=im.eim;
			if(eim->SelectIndex>=0)
				YongUpdateInputDesc(eim);
			y_im_str_encode(s2t_conv(eim->StringGet),im.StringGet,0);
			y_ui_input_draw();
		}
		return NULL;
	}
	return l_strdup(phrase);
}

static void ui_set_select(const char *text)
{
	char temp[8192];
	l_gb_to_utf8(text,temp,sizeof(temp));
	wui->clipboard_set_text(temp);
}

static int ui_calc_with_metrics(const char *s)
{
	char expr[256];
	int pos=0,i,c;
	int wa_x=0,wa_y=0,wa_w=0,wa_h=0;
	int scr_w,scr_h;
	
	wui->get_workarea(MainWin,&wa_x,&wa_y,&wa_w,&wa_h);
	wui->get_output_size(MainWin,&scr_w,&scr_h);
		
	for(i=0;(c=s[i])!='\0';i++)
	{
		if(pos>200)
			return -1;
		if(c=='S')
		{
			switch(s[i+1]){
			case 'W':
				pos+=sprintf(expr+pos,"%d",scr_w);
				i++;
				break;
			case 'w':
				pos+=sprintf(expr+pos,"%d",wa_w);
				i++;
				break;
			case 'H':
				pos+=sprintf(expr+pos,"%d",scr_h);
				i++;
				break;
			case 'h':
				pos+=sprintf(expr+pos,"%d",wa_h);
				i++;
				break;
			default:
				return -1;
			}
		}
		else
		{
			expr[pos++]=c;
		}
	}
	expr[pos]=0;
	LVariant v=l_expr_calc(expr);
	if(v.type==L_TYPE_INT)
		return v.v_int;
	else if(v.type==L_TYPE_FLOAT)
		return v.v_float;
	else
		return -1;
}

void ui_cfg_ctrl(char *name,...)
{
	va_list ap;
	va_start(ap,name);
	if(!strcmp(name,"strip"))
	{
		InputTheme.strip=va_arg(ap,int);
	}
	else if(!strcmp(name,"calc"))
	{
		const char *s=va_arg(ap,const char*);
		int *res=va_arg(ap,int*);
		if(res)
			*res=ui_calc_with_metrics(s);
	}
	else if(!strcmp(name,"status_pos"))
	{
		int *x=va_arg(ap,int *);
		int *y=va_arg(ap,int *);
		*x=MainWin_X;
		*y=MainWin_Y;
	}
	else if(!strcmp(name,"focus"))
	{
		if(!y_ui_is_dummy() && InputWin && wui->wayland_get_surface(InputWin))
		{
			const char *target=va_arg(ap,const char *);
			w_win_t cur;
			if(target && !strcmp(target,"wayland"))
				cur=InputWin_wayland;
			else
				cur=InputWin_layer;
			if(cur!=InputWin)
			{
				wui->win_hide(InputWin);
				InputWin=cur;
				wui->win_resize(cur,InputTheme.RealWidth,InputTheme.RealHeight);
			}
		}
	}
	else if(!strcmp(name,"capslock"))
	{
		int ybus_xim_get_capslock(void);
		int *result=va_arg(ap,int *);
		*result=ybus_xim_get_capslock();
	}
	va_end(ap);
}

static int on_image_win_close(w_win_t win,const W_EVENT *e,void *data)
{
	ImageWin=NULL;
	if(ImageWin_bg)
	{
		ui_image_free(ImageWin_bg);
		ImageWin_bg=NULL;
	}
	wui->win_destroy(win);
	return 1;
}


static int on_image_win_draw(w_win_t win,const W_EVENT *e,void *data)
{
	cairo_t *cr=e->draw.cr;
	int tw,th;
	int w,h;
	ui_image_size(ImageWin_bg,&tw,&th);
	wui->win_get_size(win,&w,&h);
	ui_image_draw_full(cr,ImageWin_bg,0,0,w,h,0,0,tw,th);
	return 1;
}

static int on_image_win_scroll(w_win_t win,const W_EVENT *e,void *data)
{
	int tran=wui->win_tran(win,-1);
	if(e->scroll.dy>0)
	{
		tran+=5;
		if(tran>200)
			tran=200;
	}
	else if(e->scroll.dy<0)
	{
		tran-=5;
		if(tran<0)
			tran=0;
	}
	wui->win_tran(win,tran);
	return 0;
}

static int on_image_win_click(w_win_t win,const W_EVENT *e,void *data)
{
	if(e->mouse.button==W_BUTTON_MIDDLE)
	{
		int *win_data=wui->win_get_data(win);
		wui->win_tran(win,*win_data);
	}
	return 1;
}

static void ui_show_image(const char *name,const char *file,int top,int tran)
{
	bool first=!ImageWin;
	if(!ImageWin)
	{
		int *win_data=l_new0(int);
		ImageWin = wui->win_create(name,"layer,decorated",win_data);
		wui->win_connect(ImageWin,W_CLOSE,on_image_win_close,NULL);
		wui->win_connect(ImageWin,W_DRAW,on_image_win_draw,NULL);
		wui->win_connect(ImageWin,W_SCROLL,on_image_win_scroll,NULL);
		wui->win_connect(ImageWin,W_MOUSE_UP,on_image_win_click,NULL);
	}
	wui->win_hide(ImageWin);
	int *win_data=wui->win_get_data(ImageWin);
	*win_data=tran;
	wui->win_set_title(ImageWin,name);
	wui->win_tran(ImageWin,tran);
	if(ImageWin_bg) ui_image_free(ImageWin_bg);
	ImageWin_bg=ui_image_load_scale(file,ui_res_scale,-1,-1,IMAGE_ALL);
	if(ImageWin_bg)
	{
		int width,height;
		double scale=wui->win_get_scale(ImageWin);
		ui_image_size(ImageWin_bg,&width,&height);
		width=(int)round(width/scale);
		height=(int)round(height/scale);
		wui->win_resize(ImageWin,width,height);
	}
	if(first)
		wui->win_center(ImageWin);
	wui->win_show(ImageWin);
}

static void ui_show_tip(const char *fmt,...)
{
	char gb[128];
	char text[128];
	va_list ap;

	if(!fmt || !fmt[0])
		return;
	text[0]=0;
	va_start(ap,fmt);
	vsnprintf(gb,sizeof(gb),YT(fmt),ap);
	va_end(ap);
	y_im_str_encode(gb,text+strlen(text),0);
	wui->toast(text);
}

static void ui_beep(int c)
{
	wui->win_bell(InputWin);
}

static int ui_request(int cmd)
{
	g_idle_add((GSourceFunc)y_im_request,LINT_TO_PTR(cmd));
	return 0;
}

static gboolean send_ctrl_v_at_idle(void)
{
	y_xim_forward_key(CTRL_V,1);
	return G_SOURCE_REMOVE;
}

void YongSendClipboard(const char *s)
{
	if(y_ui_is_dummy())
		return;
	int len=strlen(s);
	char *utf8=l_alloc(len*2+1);
	y_im_str_encode(s,utf8,0);
	wui->clipboard_set_text(utf8);
	g_timeout_add(50,(GSourceFunc)send_ctrl_v_at_idle,NULL);
}

void YongSendFile(const char *fn)
{
	if(y_ui_is_dummy())
		return;
	int len=strlen(fn);
	char *utf8=l_alloc(len*2+1);
	y_im_str_encode(fn,utf8,0);

	if(strstr(fn,".txt"))
	{
		FILE *fp;
		fp=y_im_open_file(fn,"rb");
		if(fp)
		{
			char temp[4096];
			int len;
			len=fread(temp,1,sizeof(temp)-1,fp);
			temp[len]=0;
			YongSendClipboard(temp);
			fclose(fp);
		}
	}
	else
	{
		char temp[256];
		char fullpath[256];
		ui_image_path(fn,temp,IMAGE_ROOT);
		if(!l_fullpath(fullpath,temp,sizeof(fullpath)))
			return;
		char data[512];
		int size=sprintf(data,"copy\n%s",fullpath);
		wui->clipboard_set_mime("x-special/gnome-copied-files",data,size);
		g_timeout_add(50,(GSourceFunc)send_ctrl_v_at_idle,NULL);
	}

}

static gboolean ui_call_wraper(void **p)
{
	void (*cb)(void*)=p[0];
	void *arg=p[1];
	l_free(p);
	cb(arg);
	return G_SOURCE_REMOVE;
}

static int ui_call(void (*cb)(void*),void *arg)
{
	void **p=l_cnew(2,void*);
	p[0]=cb;
	p[1]=arg;
	g_idle_add((GSourceFunc)ui_call_wraper,p);
	return 0;
}

static double ui_get_scale(int which)
{
	if(which==1)
		return ui_surface_scale;
	return ui_scale;
}

static void ui_setup_dummy(Y_UI *p);

static void load_wui_backend(void)
{
	const char *backend_path=".";
	const char *backend_name=NULL;
	const char *gdk_backend=getenv("GDK_BACKEND");
	if(gdk_backend)
	{
		if(!strcmp(gdk_backend,"x11"))
			backend_name="gtk3-x11";
		else if(!strcmp(gdk_backend,"wayland"))
			backend_name="wayland";
	}
	if(!backend_name)
	{
		if(getenv("WAYLAND_DISPLAY"))
		{
			const char *desktop=getenv("XDG_CURRENT_DESKTOP");
			if(!strstr(desktop,"GNOME"))
			{
				backend_name="wayland";
				is_gnome=true;
			}
		}
	}
	if(!backend_name)
	{
		if(getenv("DISPLAY"))
			backend_name="gtk3-x11";
	}
	if(!backend_name)
	{
		fprintf(stderr,"get wui backend fail\n");
		exit(-1);
	}
	char path[256];
	sprintf(path,"%s/libwui-%s.so",backend_path,backend_name);
	void *h=l_dlopen(path);
	if(!h)
	{
		backend_path=getenv("WUI_BACKEND_PATH");
		if(backend_path)
		{
			sprintf(path,"%s/libwui-%s.so",backend_path,backend_name);
			h=l_dlopen(path);
		}
	}
	if(!h)
	{
		fprintf(stderr,"load wui backend %s fail\n",path);
		exit(-1);
	}
	wui=l_dlsym(h,"wui");
	if(!wui)
	{
		fprintf(stderr,"wui backend %s bad\n",path);
		exit(-1);
	}
	is_wayland=!strcmp(backend_name,"wayland");
}

void ui_show_workarea(void)
{
	int y_im_set_exec(void);
	y_im_set_exec();
	load_wui_backend();
	wui->init("net.dgod.yong.workarea");
	MainWin=wui->win_create("main","layer",NULL);
	int wa_x,wa_y,wa_w,wa_h;
	wui->get_workarea(MainWin,&wa_x,&wa_y,&wa_w,&wa_h);
	printf("workarea %d %d %d %d\n",wa_x,wa_y,wa_w,wa_h);
	wui->get_output_size(MainWin,&wa_w,&wa_h);
	printf("output %d %d\n",wa_w,wa_h);
}

void ui_setup_default(Y_UI *p)
{
	L_LOOP_SCHED sched={
		.sleep=ui_timer_add,
		.idle=ui_idle_add,
		.main=ui_call,
		.poll=ui_poll,
	};
	l_loop_sched(&sched);
	l_thrdp_init(4);
	l_co_init();

	if(!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"))
		return ui_setup_dummy(p);

	load_wui_backend();

	p->init=ui_init;
	p->loop=ui_loop;
	p->clean=ui_clean;
	p->quit=ui_quit;

	p->main_update=ui_main_update;
	p->main_win=ui_main_win;
	p->main_show=ui_main_show;

	p->input_win=ui_input_win;
	p->input_update=ui_input_update;
	p->input_draw=YongDrawInput;
	p->input_redraw=ui_input_redraw;
	p->input_show=ui_input_show;
	p->input_move=ui_input_move;

	p->button_update=ui_button_update;
	p->button_show=ui_button_show;
	p->button_label=ui_button_label;

	p->tray_update=ui_tray_update;
	p->tray_status=ui_tray_status;
	p->tray_tooltip=ui_tray_tooltip;

	p->get_select=ui_get_select;
	p->set_select=ui_set_select;

	p->update_menu=l_noop;
	p->skin_path=ui_skin_path;
	p->cfg_ctrl=ui_cfg_ctrl;
	p->show_message=ui_show_message;
	p->show_image=ui_show_image;
	p->show_tip=ui_show_tip;

	p->beep=ui_beep;
	p->request=ui_request;

	p->get_scale=ui_get_scale;
	p->get_dark=color_schema_get_dark;
	p->timer_add=ui_timer_add;
	p->timer_del=ui_timer_del;
	p->idle_add=ui_idle_add;
	p->idle_del=ui_idle_del;
	p->call=ui_call;
}

static GMainLoop *dummy_loop;

static int dummy_ui_init(void)
{
	dummy_loop=g_main_loop_new(NULL,0);
	return 0;
}

static int dummy_ui_loop(void)
{
	g_main_loop_run(dummy_loop);
	return 0;
}

static void dummy_ui_quit(void)
{
	g_main_loop_quit(dummy_loop);
}

static void ui_setup_dummy(Y_UI *p)
{
	p->dummy=true;

	p->init=dummy_ui_init;
	p->loop=dummy_ui_loop;
	p->clean=l_noop;
	p->quit=dummy_ui_quit;

	p->timer_add=ui_timer_add;
	p->timer_del=ui_timer_del;
	p->idle_add=ui_idle_add;
	p->idle_del=ui_idle_del;
}
