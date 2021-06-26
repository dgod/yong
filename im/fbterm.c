#include "yong.h"
#include "xim.h"
#include "im.h"
#include "common.h"
#include "immessage.h"
#include "keycode.h"
#include "ui.h"
#include "llib.h"

#define FBTERM_DEBUG

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>

#include <cairo.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <fcntl.h>

//#define memcpy(a,b,c) memmove(a,b,c)

enum{
	Black = 0,
	DarkRed,
	DarkGreen,
	DarkYellow,
	DarkBlue,
	DarkMagenta,
	DarkCyan,
	Gray,
	DarkGray,
	Red,
	Green,
	Yellow,
	Blue,
	Magenta,
	Cyan,
	White
};

#define MSG(a) ((Message *)(a))

static int imfd;
static GIOChannel *imch;
static GMainLoop *loop;

static CONNECT_ID id;
static CONNECT_ID *CurrentID=&id;
static int def_lang;

static Info info;

static unsigned MainWin=1;
static gint MainWin_X,MainWin_Y,MainWin_W,MainWin_H;

static unsigned InputWin=0;
static gint InputWin_X,InputWin_Y;

static struct{
	int CodeX,CodeY;
	int CandX,CandY;
	int OffX,OffY;
	int Height,Width;
	int mWidth,mHeight;
	int RealHeight,RealWidth;
	int Left,Right;
	int WorkLeft,WorkRight;
	int MaxHeight;
	
	unsigned char bg_color;
	unsigned char text[5];

	int space;
	int root;
	int noshow;
	int line;
	int page;
	int caret;
	int no;
	int strip;
	int x,y;
}InputTheme;

static int im_pipe[2];

#ifdef FBTERM_DEBUG
FILE *fp;
void fb_debug(char *fmt,...)
{
	va_list ap;
	if(!fp)
	{
		fp=fopen("log.txt","w");
	}
	if(!fp) return;
	va_start(ap,fmt);
	vfprintf(fp,fmt,ap);
	va_end(ap);
	fflush(fp);
}
#else
#define fb_debug(fmt,arg...)
#endif

#include "keymap.c"

static gboolean im_pipe_cb(GIOChannel *source,GIOCondition condition,gpointer data)
{
	unsigned char c;
	int ret;
	ret=read(im_pipe[0],&c,1);
	if(ret==1)
	{
		y_im_request(c);
	}
	return TRUE;
}

int GetKey(int KeyCode,int KeyState);

static void fbterm_send_string(const char *text)
{
	int ret;
	int len;
	if(imch==NULL)
		return;

	len=strlen(text);
	char buf[G_STRUCT_OFFSET(Message, texts) + len];

	MSG(buf)->type = PutText;
	MSG(buf)->len = sizeof(buf);
	memcpy(MSG(buf)->texts, text, len);

	ret=write(imfd,buf,MSG(buf)->len);
	if(ret!=MSG(buf)->len)
	{
		fb_debug("send string fail\n");
	}
}

static void input_expose(void);
static void main_expose(void);

static int process_key(int keyval,int modifiers,int down)
{
	CONNECT_ID *id=CurrentID;
	int key;
	static int last_press;
	static uint32_t last_press_time;
	
	//fb_debug("process %x %x %d\n",modifiers,keyval,down);
	
	id->focus=1;
	
	key=GetKey(keyval,modifiers);
	if(!key)
		return FALSE;
	if(down)
	{
		if(!last_press)
			last_press_time=y_im_tick();
        last_press=key;
	}
	if(key>=YK_LSHIFT && key<=YK_RALT)
	{
		if(down)
			return FALSE;
		if(key!=last_press || y_im_tick()-last_press_time>300)
		{
			last_press=0;
			return FALSE;
		}
	}
	if(!down)
	{
		if(last_press==key)
			last_press=0;
	}
	switch(key){
	case CTRL_SPACE:
	case SHIFT_SPACE:
	{
		if(down && YongHotKey(key))
			return TRUE;
		break;
	}
	case CTRL_LSHIFT:
	case CTRL_RSHIFT:
	{
		if(down && key==last_press && YongHotKey(key))
			return TRUE;
		break;
	}
	case YK_LCTRL:
	case YK_RCTRL:
	case YK_LSHIFT:
	case YK_RSHIFT:
	{
		y_im_input_key(key);
		return FALSE;
	}
	default:
	{
		if(down && YongHotKey(key))
			return TRUE;
		break;
	}}
	if(id->state && down)
	{
		int mod=key&KEYM_MASK;
		key&=~KEYM_CAPS;
		if(YongKeyInput(key,mod))
		{
			y_im_speed_update(key,0);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean fb_io_func(GIOChannel *src,GIOCondition cond,gpointer user)
{
	unsigned char data[512],*msg;
	int bytes;
	
	bytes=read(imfd,data,sizeof(data));
	if(bytes<=0)
	{
		g_main_loop_quit(loop);
		return FALSE;
	}
	//fb_debug("io read bytes %d %02x%02x %02x%02x\n",
	//		bytes,data[0],data[1],data[2],data[3]);
	msg=data;
	while(bytes>0)
	{
		int len=MSG(msg)->len;
		if(bytes<len) break;
		//fb_debug("msg type:%d len:%d\n",MSG(msg)->type,MSG(msg)->len);
		switch(MSG(msg)->type){
		case Disconnect:
		{
			g_main_loop_quit(loop);
			break;
		}
		case Active:
		{
			//fb_debug("active\n");
			CurrentID=&id;
			CurrentID->focus=1;
			CurrentID->state=1;
			init_keycode_state();
			YongSetLang(def_lang);
			YongShowMain(1);
			YongShowInput(1);
			break;
		}
		case Deactive:
		{
			//fb_debug("deactive\n");
			YongResetIM();
			YongShowInput(0);
			YongShowMain(0);
			if(CurrentID)
			{
				CurrentID->state=0;
				CurrentID->focus=0;
				CurrentID=0;
			}
			break;
		}
		case ShowUI:
		{
			if(CurrentID)
			{
				YongShowInput(CurrentID->state);
				YongShowMain(CurrentID->state);
			}
			break;
		}
		case HideUI:
		{
			Message msg;
			msg.type = AckHideUI;
			msg.len = sizeof(msg);
			write(imfd,(char *)&msg,sizeof(msg));
			break;
		}
		case SendKey:
		{
			unsigned char *buf;
			int len;
			int i;
			int block=0;
			//fb_debug("key\n");
			buf=(unsigned char*)MSG(msg)->keys;
			len=MSG(msg)->len - G_STRUCT_OFFSET(Message, keys);
			//fb_debug("%02x%02x %02x%02x %02x %02x %02x %02x %02x\n",
			//		msg[0],msg[1],msg[2],msg[3],
			//		msg[4],msg[5],msg[6],msg[7],msg[8]);
			for(i=0;i<len;i++)
			{
				char down = !(buf[i] & 0x80);
				short code = buf[i] & 0x7f;
				if (!code)
				{
					if (i + 2 >= len) break;
					code = (buf[++i] & 0x7f) << 7;
					code |= buf[++i] & 0x7f;
					if (!(buf[i] & 0x80) || !(buf[i - 1] & 0x80))
						continue;
				}
				//fb_debug("code %hd down %d\n",code,(down?1:0));
				unsigned short keysym = keycode_to_keysym(code, down);
				guint32 keyval = linux_keysym_to_keyval(keysym, code);
				if(!keyval)
				{
					char *str = keysym_to_term_string(keysym, down);
					fbterm_send_string(str);
					continue;
				}
				block=process_key(keyval,modifier_state,down);
				if(!block)
				{
					char *str = keysym_to_term_string(keysym, down);
					fbterm_send_string(str);
				}
				calculate_modifiers(keyval,down);
			}
			break;
		}
		case CursorPosition:
		{
			if(CurrentID && CurrentID->state)
			{
				YongMoveInput(MSG(msg)->cursor.x, MSG(msg)->cursor.y);
			}
			break;
		}
		case AckWin:
		{
			/* Draw fbterm ui here */
			input_expose();
			main_expose();
			break;
		}
		case FbTermInfo:
		{
			info=MSG(msg)->info;
			//fb_debug("%d %d\n",info.fontWidth,info.fontHeight);
			
			if(InputTheme.line!=1)
				InputTheme.CandY=InputTheme.CodeY+info.fontHeight+3;
			else
				InputTheme.CandY=InputTheme.CodeY;
			InputTheme.space=info.fontWidth/2;
			InputTheme.RealHeight=InputTheme.CandY+info.fontHeight+3;
			break;
		}
		case TermMode:
		{
			update_term_mode(MSG(msg)->term.crWithLf,
					MSG(msg)->term.applicKeypad,
					MSG(msg)->term.cursorEscO);
			break;
		}
		case Ping:
		{
			Message msg;
			msg.type=AckPing;
			msg.len=sizeof(msg);
			write(imfd,(char *)&msg,sizeof(msg));
			break;
		}
		default:
			break;
		}
		bytes-=len;
		msg+=len;
	}
	return TRUE;
}

static void get_im_socket(void)
{
	char *val;
	val = getenv("FBTERM_IM_SOCKET");
	if(val)
	{
		char *tail;
		imfd = strtol(val, &tail, 0);
		if (!*tail)
		{
			fb_debug("find socket %d\n",imfd);
			imch=g_io_channel_unix_new(imfd);
			g_io_channel_set_encoding(imch,NULL,NULL);
			if(0==g_io_add_watch(imch,G_IO_IN,fb_io_func,0))
			{
				fb_debug("io add watch fail\n");
			}
		}
	}
}

static void connect_fbterm(char raw)
{
	GIOStatus ret;
	Message msg;
	get_im_socket();
	if (imfd == 0) return;

	msg.type = Connect;
	msg.len = sizeof(msg);
	msg.raw = (raw ? 1 : 0);
	ret=write(imfd,(char *)&msg,sizeof(msg));
	if(ret!=sizeof(msg))
	{
		fb_debug("write connect fail\n",ret);
	}
}

int xim_fbterm_init(void)
{
	connect_fbterm(1);
	def_lang=y_im_get_config_int("IM","lang");
	id.track=1;
	id.trad=im.TradDef;
	fb_debug("fbterm init done\n");
	return 0;
}

void xim_fbterm_destroy(void)
{
}

CONNECT_ID *xim_fbterm_get_connect(void)
{
	return CurrentID;
}

static void xim_fbterm_put_connect(CONNECT_ID *dummy)
{
	if(dummy->dummy)
	{
		id.corner=dummy->corner;
		id.lang=dummy->lang;
		id.biaodian=dummy->biaodian;
		id.trad=dummy->trad;
	}
}

void xim_fbterm_enable(int enable)
{
}

void xim_fbterm_forward_key(int key,int repeat)
{
	if(imfd == 0)
		return;
}

int xim_fbterm_trigger_key(int key)
{
	return 0;
}

static void xim_fbterm_send_string(const char *s,int flags)
{

	char text[512];
	int len;
	if(imch==NULL)
		return;

	y_im_str_encode(s,text,flags);
	len=strlen(text);
	char buf[G_STRUCT_OFFSET(Message, texts) + len];

	MSG(buf)->type = PutText;
	MSG(buf)->len = sizeof(buf);
	memcpy(MSG(buf)->texts, text, len);
	write(imfd,buf,MSG(buf)->len);
}

static void xim_explore_url(const char *s)
{
	char temp[256];
	y_im_str_encode(s,temp,0);
	if(y_im_is_url(temp))
	{
		char *args[]={"xdg-open",temp,0};
		g_spawn_async(NULL,args,NULL,
			G_SPAWN_SEARCH_PATH|G_SPAWN_STDOUT_TO_DEV_NULL,
			0,0,0,0);
	}
	else
	{
		g_spawn_command_line_async(temp,NULL);
	}
}

/* UI CODE */

static int xim_fbterm_preedit_clear(void)
{
	return 0;
}

static int xim_fbterm_preedit_draw(const char *s,int len)
{
	return 0;
}

static void fill_rect(Rectangle rect, unsigned char color)
{
	Message msg;
	msg.type = FillRect;
	msg.len = sizeof(msg);
	
	rect.x+=InputWin_X;
	rect.y+=InputWin_Y;

	msg.fillRect.rect = rect;
	msg.fillRect.color = color;

	write(imfd, (char *)&msg, sizeof(msg));
}

static void draw_text(unsigned x, unsigned y, unsigned char fc, unsigned char bc, const char *text)
{
	unsigned len=strlen(text);

	if (!text || !len) return;

	char buf[G_STRUCT_OFFSET(Message, drawText.texts) + len];

	MSG(buf)->type = DrawText;
	MSG(buf)->len = sizeof(buf);

	MSG(buf)->drawText.x = InputWin_X+x;
	MSG(buf)->drawText.y = InputWin_Y+y;
	MSG(buf)->drawText.fc = fc;
	MSG(buf)->drawText.bc = bc;
	memcpy(MSG(buf)->drawText.texts, text, len);

	write(imfd, buf, MSG(buf)->len);
}

static void input_expose(void)
{
	int i,count=0;
	unsigned char color;
	EXTRA_IM *eim=CURRENT_EIM();
	Rectangle rect;//={0,0,InputTheme.RealWidth,InputTheme.RealHeight};

	rect.x=0;rect.y=0;rect.w=InputTheme.RealWidth;rect.h=InputTheme.RealHeight;
	
	if(!im.CodeInputEngine[0] && (!eim || !eim->StringGet[0]) && InputTheme.noshow!=2)
		return;
	
	if(eim)
		count=eim->CandWordCount;

	fill_rect(rect,InputTheme.bg_color);
	if(eim && eim->StringGet[0])
	{
		draw_text(im.CodePos[0],InputTheme.CodeY,InputTheme.text[0],InputTheme.bg_color,im.StringGet);
	}
	if(im.CodeInputEngine[0])
	{
		draw_text(im.CodePos[1],InputTheme.CodeY,InputTheme.text[0],InputTheme.bg_color,im.CodeInput);
	}
	if(eim && eim->CandPageCount>1 && InputTheme.page)
	{
		draw_text(im.PagePosX,im.PagePosY,InputTheme.text[4],InputTheme.bg_color,im.Page);
	}
	if(InputTheme.caret)
	{
		//rect=(Rectangle){im.CodePos[2],InputTheme.CodeY,1,im.cursor_h};
		rect.x=im.CodePos[2];rect.y=InputTheme.CodeY;rect.w=1;rect.h=im.cursor_h;
		fill_rect(rect,InputTheme.text[3]);
	}
	for(i=0;i<count;i++)
	{
		double *posx=im.CandPosX+3*i;
		double *posy=im.CandPosY+3*i;

		if(InputTheme.no==0)
		{
			color=InputTheme.text[0];
			draw_text(posx[0],posy[0],color,InputTheme.bg_color,YongGetSelectNumber(i));
		}
		else
		{
			color=InputTheme.text[0];
		}
		if(i==eim->SelectIndex) color=InputTheme.text[1];
		draw_text(posx[1],posy[1],color,InputTheme.bg_color,im.CandTable[i]);

		if(im.Hint && eim->CodeTips[i] && *im.CodeTips[i])
		{
			color=InputTheme.text[2];
			draw_text(posx[2],posy[2],color,InputTheme.bg_color,im.CodeTips[i]);
		}
	}
}

static int ui_fbterm_init(void)
{
	GIOChannel *chn;

	loop=g_main_loop_new(NULL,0);
	
	pipe(im_pipe);
	chn=g_io_channel_unix_new(im_pipe[0]);
	g_io_add_watch(chn,G_IO_IN,im_pipe_cb,NULL);

	return 0;
}

static void ui_fbterm_clean(void)
{
}

static int ui_fbterm_loop(void)
{
	g_main_loop_run(loop);
	return 0;
}

static int ui_fbterm_input_update(UI_INPUT *param)
{
	InputTheme.line=param->line;
	InputTheme.caret=param->caret;
	InputTheme.page=param->page;
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
	
	/* fbterm special */
	InputTheme.CodeX=3;
	InputTheme.CodeY=3;
	InputTheme.CandX=3;
	if(InputTheme.line!=1)
		InputTheme.CandY=InputTheme.CodeY+info.fontHeight+3;
	else
		InputTheme.CandY=InputTheme.CodeY;
	InputTheme.space=info.fontWidth/2;
	InputTheme.bg_color=White;
	InputTheme.text[0]=Black;
	InputTheme.text[1]=Blue;
	InputTheme.text[2]=Red;
	InputTheme.text[3]=Black;
	InputTheme.text[4]=Black;

	InputTheme.RealWidth=2*InputTheme.RealHeight;
	InputTheme.MaxHeight=InputTheme.RealHeight;

	return 0;
}

static int get_text_width(const char *s,int *height)
{
	int width=0;
	while(s && s[0])
	{
		gunichar c=g_utf8_get_char(s);
		if(g_unichar_iswide(c))
			width+=info.fontWidth*2;
		else
			width+=info.fontWidth;
		s=g_utf8_next_char(s);
	}
	if(height) *height=info.fontHeight;
	return width;
}

static int fbterm_code_width(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	int ret;
	im.CodePos[0]=InputTheme.CodeX;
	if(eim && eim->StringGet[0])
	{
		ret=get_text_width(im.StringGet,NULL);
		im.CodePos[1]=im.CodePos[0]+ret;
	}
	else
	{
		im.CodePos[1]=InputTheme.CodeX;
	}
	ret=get_text_width(im.CodeInput,&im.cursor_h);
	im.CodePos[3]=im.CodePos[1]+ret;
	if(!eim || eim->CaretPos==-1 || !im.CodeInputEngine[eim->CaretPos])
	{
		im.CodePos[2]=im.CodePos[1]+ret;
	}
	else
	{
		char tmp,*p;
		p=(char*)im.CodeInput;
		tmp=p[eim->CaretPos];
		p[eim->CaretPos]=0;
		im.CodePos[2]=get_text_width(p,NULL)+im.CodePos[1];
		p[eim->CaretPos]=tmp;
	}
	return (int)im.CodePos[3];
}

static int fbterm_page_width(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	int ret=0;
	if(eim && eim->CandPageCount>1)
	{
		sprintf(im.Page,"%d/%d",eim->CurCandPage+1,eim->CandPageCount);
		im.PageLen=get_text_width(im.Page,NULL);
		im.PagePosY=InputTheme.CodeY;
		ret=im.PageLen;
	}
	return ret;
}

static int fbterm_cand_width(void)
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
		if(InputTheme.no==0)
		{
			cur+=get_text_width(YongGetSelectNumber(i),&h1);
		}
		else
		{
			h1=0;
		}
		pos[1]=cur;
		cur+=get_text_width(im.CandTable[i],&h2);
		h=MAX(h1,h2);		

		pos[2]=cur;
		if(im.Hint)
		{
			char *t=(char*)im.CodeTips[i];
			if(t && *t)
			{
				cur+=get_text_width(t,&h3);
				h=MAX(h,h3);
			}
		}
		*width=MAX(*width,cur);
		if(i!=count-1)
			cur+=InputTheme.space;

		pos=im.CandPosY+i*3;
		pos[0]=cur_y+(h-h1+1)/2;
		pos[1]=cur_y+(h-h2+1)/2;
		pos[2]=cur_y+(h-h3+1)/2;
		if(InputTheme.line==2)
		{
			if(i==count-1)
				*height+=h+InputTheme.CodeY;
			else
				*height+=h+InputTheme.space;
			cur_y+=h+InputTheme.space;

			cur=InputTheme.CandX;
		}
	}
	return (int)*width;
}

static void set_im_window(unsigned id, int x,int y,int w,int h)
{
	if (imfd == -1 || !CurrentID->state || id >= NR_IM_WINS) return;

	Message msg;
	msg.type = SetWin;
	msg.len = sizeof(msg);
	msg.win.winid = id;
	msg.win.rect = (Rectangle){x,y,w,h};

	write(imfd, (char *)&msg, sizeof(msg));
}

static int ui_fbterm_input_draw(void)
{
	int TempWidth,DeltaWidth;
	int TempHeight,DeltaHeight;
	int CodeWidth,CandWidth=0,PageWidth=0;
	EXTRA_IM *eim=CURRENT_EIM();
	int count=0;

	if(!im.CodeInputEngine[0] && (!eim || !eim->StringGet[0]) && InputTheme.noshow!=2)
	{
		YongShowInput(0);
		return 0;
	}

	CodeWidth=fbterm_code_width();
	if(InputTheme.page)
		PageWidth=fbterm_page_width();
	if(eim) count=eim->CandWordCount;

	if(count)
	{
		int i;
		for(i=0;i<count;i++)
		{
			char *p=im.CandTable[i];
			int len;
			if(eim->WorkMode==EIM_WM_ASSIST)
				len=0;
			y_im_key_desc_translate(eim->CodeTips[i],len,p,
					im.CodeTips[i],MAX_TIPS_LEN+1);
			if(eim->WorkMode==EIM_WM_QUERY)
			{
				char *s=eim->CandTable[i];
				y_im_key_desc_translate(s,0,eim->CodeInput,p,MAX_TIPS_LEN+1);
			}
			else
			{
				const char *s=s2t_conv(eim->CandTable[i]);
				y_im_disp_cand(s,p,(InputTheme.strip>>(16*(i==eim->SelectIndex)+0))&0xff,
					(InputTheme.strip>>(16*(i==eim->SelectIndex)+8))&0xff);
			}
		}
		CandWidth=fbterm_cand_width();
	}
	if(InputTheme.line==0)
	{
		if(PageWidth)
			im.PagePosX=MAX(CandWidth-PageWidth,CodeWidth+InputTheme.space);
		CodeWidth+=InputTheme.CodeX-InputTheme.WorkLeft;
		if(PageWidth)
			CodeWidth+=InputTheme.space+PageWidth;
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
			im.PagePosX=TempWidth+InputTheme.space-pad;
			TempWidth+=InputTheme.space+PageWidth;			
		}
		if(eim)
		{
			int i,count;
			count=eim->CandWordCount;
			for(i=0;i<count;i++)
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
			CodeWidth+=InputTheme.space+PageWidth;
		CandWidth+=InputTheme.CandX-InputTheme.WorkLeft;
		TempWidth=MAX(CodeWidth,CandWidth);
		TempHeight=(int)pos[0]+InputTheme.CodeY;
		if(TempHeight<InputTheme.Height)
			TempHeight=InputTheme.Height;
	}
	TempWidth+=InputTheme.WorkRight;
	if(InputTheme.line!=2 && !InputTheme.mWidth && TempWidth<InputTheme.RealHeight*2)
		TempWidth=InputTheme.RealHeight*2;
	if(TempWidth<InputTheme.mWidth)
		TempWidth=InputTheme.mWidth;
	if(InputTheme.line==2 && TempHeight<InputTheme.mHeight)
		TempHeight=InputTheme.mHeight;
	DeltaWidth=TempWidth-InputTheme.RealWidth;
	DeltaHeight=TempHeight-InputTheme.RealHeight;
	if(DeltaWidth || DeltaHeight)
	{
		InputTheme.RealWidth=TempWidth;
		InputTheme.RealHeight=TempHeight;
		set_im_window(InputWin,CurrentID->x,CurrentID->y,InputTheme.RealWidth,InputTheme.RealHeight);
	}
	if(PageWidth && InputTheme.line==2)
	{
			im.PagePosX=InputTheme.RealWidth-InputTheme.WorkRight-
				(InputTheme.CodeX-InputTheme.WorkLeft)-PageWidth;
	}
	InputTheme.MaxHeight=MAX(InputTheme.MaxHeight,InputTheme.RealHeight);
	YongMoveInput(POSITION_ORIG,POSITION_ORIG);
	YongShowInput(1);
	y_ui_input_redraw();
	return 0;
}

static int ui_fbterm_input_redraw(void)
{
	set_im_window(InputWin,CurrentID->x,CurrentID->y,InputTheme.RealWidth,InputTheme.RealHeight);
	return 0;
}

static int ui_fbterm_input_show(int show)
{
	if(show)
		set_im_window(InputWin,CurrentID->x,CurrentID->y,InputTheme.RealWidth,InputTheme.RealHeight);
	else
		set_im_window(InputWin,0,0,0,0);
	return 0;
}

static int ui_fbterm_input_move(int off,int *x,int *y)
{
	int height=InputTheme.MaxHeight;
	if(*x==POSITION_ORIG)
	{
ORIG:
		*y=0;
		*x=info.screenWidth/2;
	}
	if(*y>info.screenHeight || *x>=2*info.screenWidth)
		return -1;
	if(off)
	{
		if(*y+height+InputTheme.OffY<=info.screenHeight)
			*y+=InputTheme.OffY;
		else *y-=height+18+30;
		*x+=InputTheme.OffX;
	}
	
	if(*x<0) *x=0;
	else if(*x+InputTheme.RealWidth>info.screenWidth)
		*x=info.screenWidth-InputTheme.RealWidth;
	if(*y<0) *y=0;
	else if(*y+height>info.screenHeight)
		*y=info.screenHeight-InputTheme.RealHeight-18-30;
	if(*x==0 && *y==0)
		goto ORIG;
	set_im_window(InputWin,*x,*y,InputTheme.RealWidth,InputTheme.RealHeight);
	InputWin_X=*x;
	InputWin_Y=*y;
	return 0;
}

static int ui_fbterm_request(int cmd)
{
	unsigned char tmp=(unsigned char)cmd;
	write(im_pipe[1],&tmp,1);
	return 0;
}

static void fill_rect_main(Rectangle rect, unsigned char color)
{
	Message msg;
	msg.type = FillRect;
	msg.len = sizeof(msg);
	
	rect.x+=MainWin_X;
	rect.y+=MainWin_Y;

	msg.fillRect.rect = rect;
	msg.fillRect.color = color;

	write(imfd, (char *)&msg, sizeof(msg));
}

void draw_text_main(unsigned x, unsigned y, unsigned char fc, unsigned char bc, const char *text)
{
	unsigned len=strlen(text);

	if (!text || !len) return;

	char buf[G_STRUCT_OFFSET(Message, drawText.texts) + len];

	MSG(buf)->type = DrawText;
	MSG(buf)->len = sizeof(buf);

	MSG(buf)->drawText.x = MainWin_X+x;
	MSG(buf)->drawText.y = MainWin_Y+y;
	MSG(buf)->drawText.fc = fc;
	MSG(buf)->drawText.bc = bc;
	memcpy(MSG(buf)->drawText.texts, text, len);

	write(imfd, buf, MSG(buf)->len);
}

typedef struct{
	bool show;
}UI_BTN_REAL;
static UI_BTN_REAL btns[UI_BTN_COUNT];

static int ui_fbterm_main_update(UI_MAIN *param)
{
	return 0;
}

static void main_expose(void)
{
	Rectangle rc;
	int i;
	if(!MainWin_W) return;
	rc.x=0;rc.y=0;
	rc.w=MainWin_W;rc.h=MainWin_H;
	fill_rect_main(rc,White);
	for(i=0;i<UI_BTN_COUNT;i++)
	{
		int x=2,y=2,step=2*info.fontWidth;
		char text[8]={0};
		if(!btns[i].show)
			continue;
		switch(i){
		case UI_BTN_CN:
			y_im_str_encode("ÖÐ",text,0);
			break;
		case UI_BTN_EN:
			y_im_str_encode("Ó¢",text,0);
			break;
		case UI_BTN_QUAN:
			y_im_str_encode("°ë",text,0);
			x+=step;
			break;
		case UI_BTN_BAN:
			y_im_str_encode("È«",text,0);
			x+=step;
			break;
		case UI_BTN_CN_BIAODIAN:
			y_im_str_encode("¡£",text,0);
			x+=step*2;
			break;
		case UI_BTN_EN_BIAODIAN:
			y_im_str_encode(".",text,0);
			x+=step*2;
			break;
		case UI_BTN_SIMP:
			y_im_str_encode("¼ò",text,0);
			x+=step*3;
			break;
		case UI_BTN_TRAD:
			y_im_str_encode("·±",text,0);
			x+=step*3;
			break;
		}
		//fprintf(stderr,"%d %s\n",i,text);
		if(text[0])
			draw_text_main(x,y,Blue,White,text);
	}	
}

static int ui_fbterm_main_show(int show)
{
	if(show)
	{
		MainWin_W=4*info.fontWidth*2+4;
		MainWin_H=info.fontHeight+4;
		MainWin_X=info.screenWidth-MainWin_W;
		MainWin_Y=info.screenHeight-MainWin_H;
		//fprintf(stderr,"expose %d %d\r\n",MainWin_W,MainWin_H);
		set_im_window(MainWin,MainWin_X,MainWin_Y,MainWin_W,MainWin_H);
	}
	else
	{
		MainWin_W=0;
		MainWin_H=0;
		set_im_window(MainWin,0,0,0,0);
	}
	return 0;
}

static int ui_fbterm_button_show(int id,int show)
{
	UI_BTN_REAL *btn=btns+id;
	btn->show=show;
	main_expose();
	return 0;
}

static int ui_fbterm_button_label(int id,const char *text)
{
	return 0;
}

void ui_setup_fbterm(Y_UI *p)
{
	p->init=ui_fbterm_init;
	p->loop=ui_fbterm_loop;
	p->clean=ui_fbterm_clean;
	
	p->main_update=ui_fbterm_main_update;
	p->main_show=ui_fbterm_main_show;
	
	p->input_update=ui_fbterm_input_update;
	p->input_draw=ui_fbterm_input_draw;
	p->input_redraw=ui_fbterm_input_redraw;
	p->input_show=ui_fbterm_input_show;
	p->input_move=ui_fbterm_input_move;
	
	p->button_show=ui_fbterm_button_show;
	p->button_label=ui_fbterm_button_label;
	
	p->request=ui_fbterm_request;
}

int y_xim_init_fbterm(Y_XIM *x)
{
	x->init=xim_fbterm_init;
	x->destroy=xim_fbterm_destroy;
	x->enable=xim_fbterm_enable;
	x->forward_key=xim_fbterm_forward_key;
	x->trigger_key=xim_fbterm_trigger_key;
	x->send_string=xim_fbterm_send_string;
	x->preedit_clear=xim_fbterm_preedit_clear;
	x->preedit_draw=xim_fbterm_preedit_draw;
	x->get_connect=xim_fbterm_get_connect;
	x->put_connect=xim_fbterm_put_connect;
	x->explore_url=xim_explore_url;
	x->name="fbterm";
	return 0;
}
