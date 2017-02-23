#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>

#include "ltricky.h"

#include <sys/stat.h>

#include "gbk.h"
#include "common.h"
#include "xim.h"
#include "im.h"
#include "s2t.h"
#include "bihua.h"
#include "ui.h"
#include "english.h"
#include "select.h"
#include "translate.h"
#include "keytool.h"

static LKeyFile *ConfigSkin;

bool MainShow;
bool InputShow;
bool RootMode;
int InputNoShow;
bool MainNoShow;
bool CaretUpdate;

IM im;

/* Hotkey list */
static int virt_key_query=KEYM_CTRL|'/';
static int virt_key_add=KEYM_CTRL|YK_INSERT;
static int virt_key_del=KEYM_CTRL|YK_DELETE;

static int key_trigger=CTRL_SPACE;
int key_select[9];
char key_select_n[11];
static char **sym_select;
static int sym_select_count;
static int key_cnen[2]={YK_LCTRL,YK_NONE};
static int key_switch=CTRL_LSHIFT;
static int key_switch_default;
static int key_switch_to[10];
static int key_s2t=CTRL_ALT_F;
static int key_pageup='-';
static int key_pagedown='=';
static int key_word_left='[';
static int key_word_right=']';
static int key_input_show='`';
static int key_main_show;
static int key_temp_english;
static int key_repeat;
static int key_stop;
static int key_keymap;
static int key_keyboard;
static int key_keyboard_s;
static int key_bihua;
static int key_dict=ALT_ENTER;
static int key_crab;
#ifndef CFG_NO_KEYTOOL
static Y_KEY_TOOL *key_tools;
#endif

static uint8_t enter_mode;
static uint8_t num_mode;
static uint8_t space_mode;
static uint8_t auto_show;
static uint8_t kp_mode;
static uint8_t abcd_mode;
static uint8_t cnen_mode;
static uint8_t caps_bd_mode;

static uint8_t alt_bd_disable;

static uint8_t tip_main;

EXTRA_IM *YongCurrentIM(void)
{
	if(key_bihua &&	im.CodeInput[0]==key_bihua && 
			!im.StringGetEngine[0] && 
			y_bihua_good() &&
			im.BihuaMode)
		return y_bihua_eim();
	else if(im.EnglishMode)
		return y_english_eim();
	else if(im.SelectMode)
		return y_select_eim();
	else
		return im.eim;
}

#if !defined(CFG_NO_DICT)
static void *local_dic;
#endif

static void YongResetIM_(void);

static int y_im_virt_key_conv(int key)
{
	if(key==virt_key_query)
		return YK_VIRT_QUERY;
	else if(key==virt_key_add)
		return YK_VIRT_ADD;
	else if(key==virt_key_del)
		return YK_VIRT_DEL;
	else
		return key;
}

static int key_do_select(int key)
{
	int i;
	for(i=0;i<9;i++)
	{
		if(key_select[i]==YK_NONE)
			break;
		if(key==key_select[i])
			return i+1;
	}
	
	i=y_strchr_pos(key_select_n,key);
	if(i>=0)
		return i;
		
	i=key-YK_VIRT_SELECT;
	if(i>=0 && i<10)
	{
		return i;
	}

	return -1;
}

void YongSetLang(int lang)
{
	CONNECT_ID *id;

	/* this test used to reserve the key already in buffer */
	if(im.CodeInputEngine[0] && !(im.CodeInputEngine[0]&0x80) && !(im.CodeInputEngine[1]&0x80))
	{
		if(im.EnglishMode==0)
		{
			EXTRA_IM *eim;
			if(im.eim)
			{
				int res=im.eim->DoInput(key_cnen[0]);
				if(res!=IMR_ENGLISH && res!=IMR_NEXT)
					return;
			}
			eim=y_english_eim();
			im.EnglishMode=1;
			im.CodeLen=strlen(im.CodeInputEngine);
			im.StringGet[0]=0;im.StringGet[1]=0;
			if(im.eim)
			{
				char temp[MAX_CODE_LEN+1];
				strcpy(temp,im.CodeInputEngine);
				//y_im_str_strip(temp);
				im.eim->CandWordCount=0;
				im.eim->CaretPos=-1;
				im.eim->StringGet[0]=0;
				im.eim->Reset();
				strcpy(im.CodeInputEngine,temp);
			}
			eim->CodeLen=strlen(im.CodeInputEngine);
			eim->CaretPos=eim->CodeLen;
			if(cnen_mode==1)
			{
				y_xim_send_string(im.CodeInputEngine);
				goto out;
			}
			eim->DoInput(YK_VIRT_REFRESH);
			y_im_str_encode(eim->CodeInput,im.CodeInput,DONT_ESCAPE);
			y_im_str_encode(eim->StringGet,im.StringGet,0);
			y_ui_input_draw();
		}
		else if(im.eim)
		{
			EXTRA_IM *eim=y_english_eim();
			char temp[MAX_CODE_LEN+1];
			
			im.EnglishMode=0;
			strcpy(temp,im.CodeInputEngine);
			eim->CandWordCount=0;
			eim->CaretPos=-1;
			eim->StringGet[0]=0;
			eim->Reset();
			strcpy(im.CodeInputEngine,temp);
			eim=im.eim;
			eim->CodeLen=strlen(im.CodeInputEngine);
			eim->CaretPos=eim->CodeLen;
			if(!YongKeyInput(YK_VIRT_REFRESH,0))
			{
				YongSetLang(-1);
			}
		}		
		return;
	}

out:
	id=y_xim_get_connect();
	if(!id)
		return;

	id->lang=lang;
	y_xim_put_connect(id);
	YongUpdateMain(id);
	YongResetIM();
}

void YongSetCorner(int corner)
{
	CONNECT_ID *id;
	
	id=y_xim_get_connect();
	if(!id)
		return;
	id->corner=corner;
	y_xim_put_connect(id);
	YongUpdateMain(id);
	YongResetIM();
}

void YongSetBiaodian(int biaodian)
{
	CONNECT_ID *id;
	
	id=y_xim_get_connect();
	if(!id)
		return;
	if(id->lang==LANG_EN)
		return;
	id->biaodian=biaodian;
	y_xim_put_connect(id);
	YongUpdateMain(id);
}

void YongSetTrad(int trad)
{
	CONNECT_ID *id;
	
	id=y_xim_get_connect();
	if(!id)
		return;
	if(id->lang==LANG_EN)
		return;
	id->trad=trad;
	y_xim_put_connect(id);
	YongUpdateMain(id);
}

static void corner_change_cb(void *user_data)
{
	int cn=LPTR_TO_INT(user_data);
	YongSetCorner(cn);
}

static void biaodian_change_cb(void *user_data)
{
	int ln=LPTR_TO_INT(user_data);
	YongSetBiaodian(ln);
}

static void lang_change_cb(void *user_data)
{
	int ln=LPTR_TO_INT(user_data);
	CONNECT_ID *id=y_xim_get_connect();
	if(!id) return;
	YongSetLang(ln);
}

static void switch_im_cb(void * user_data)
{
	YongSwitchIM(-1);
}

static void s2t_change_cb(void *user_data)
{
	CONNECT_ID *id=y_xim_get_connect();
	if(id && id->state)
	{
		YongSetTrad(!id->trad);
	}
}

static void keyboard_cb(void *user_data)
{
#ifndef CFG_NO_KEYBOARD
	y_kbd_show(-1);
#endif
}

#define MAIN_BTN_COUNT		7
static struct{
	const char *name;
	const char *sub[2];
	int id;
	void (*cb)(void *);
}main_btns[MAIN_BTN_COUNT]={
	{"lang",{"lang_cn","lang_en"},UI_BTN_CN,lang_change_cb},
	{"corner",{"corner_half","corner_full"},UI_BTN_QUAN,corner_change_cb},
	{"biaodian",{"biaodian_cn","biaodian_en"},UI_BTN_CN_BIAODIAN,biaodian_change_cb},
	{"s2t",{"s2t_s","s2t_t"},UI_BTN_SIMP,s2t_change_cb},
	{"keyboard",{"keyboard_img",0},UI_BTN_KEYBOARD,keyboard_cb},
	{"name",{"name_img",0},UI_BTN_NAME,switch_im_cb},
	{"menu",{"menu_img",0},UI_BTN_MENU,0},
};

void update_main_window(void)
{
	UI_MAIN param;
	UI_BUTTON btn;
	char *tmp;
	int i,j;
	
	memset(&param,0,sizeof(param));

	param.scale=l_key_file_get_int(ConfigSkin,"main","scale");
	tmp=(char *)l_key_file_get_data(ConfigSkin,"main","line_width");
	if(!tmp)
	{
		param.line_width=1;
	}
	else
	{
		param.line_width=strtod(tmp,NULL);
		param.move_style=1;
	}
	param.bg=l_key_file_get_string(ConfigSkin,"main","bg");
	if(!param.bg) param.bg=l_strdup("#ffffff");
	if(param.bg[0]=='#')
	{
		param.border=l_key_file_get_string(ConfigSkin,"main","border");
		if(!param.border) param.border=l_strdup("#CBCAE6");
	}
	param.tran=l_key_file_get_int(ConfigSkin,"main","tran");
	if(param.tran<0 || param.tran>255) param.tran=0;

	tmp=l_key_file_get_string(ConfigSkin,"main","size");
	if(tmp)
	{
		l_sscanf(tmp,"%d,%d",&param.rc.w,&param.rc.h);
		l_free(tmp);
	}
	tmp=y_im_get_config_string("main","pos");
	if(tmp)
	{
		param.rc.y=-1;
		char **pos=l_strsplit(tmp,',');
		if(pos[0])
			y_ui_cfg_ctrl("calc",pos[0],&param.rc.x);
		if(pos[1])
			y_ui_cfg_ctrl("calc",pos[1],&param.rc.y);
		l_strfreev(pos);
	}
	tmp=l_key_file_get_string(ConfigSkin,"main","move");
	if(tmp)
	{
		l_sscanf(tmp,"%d,%d,%d,%d",&param.move.x,&param.move.y,
			&param.move.w,&param.move.h);
		l_free(tmp);
	}
	param.auto_tran=y_im_get_config_int("main","tran");

	y_ui_main_update(&param);
	l_free(param.bg);
	l_free(param.border);

	for(i=0;i<MAIN_BTN_COUNT;i++)
	{
		char key[64];
		tmp=l_key_file_get_string(ConfigSkin,"main",main_btns[i].name);
		if(!tmp)
		{
			for(j=0;j<2;j++)
			{
				if(!main_btns[i].sub[j]) break;
				y_ui_button_update(main_btns[i].id+j,0);
			}
			continue;
		}
		l_sscanf(tmp,"%d,%d",&btn.x,&btn.y);
		l_free(tmp);
		sprintf(key,"%s_font",main_btns[i].name);
		btn.font=l_key_file_get_string(ConfigSkin,"main",key);
		sprintf(key,"%s_color",main_btns[i].name);
		btn.color=l_key_file_get_string(ConfigSkin,"main",key);
		for(j=0;j<2;j++)
		{
			char *p;
			char *normal,*over,*down;			
			if(!main_btns[i].sub[j]) break;
			tmp=l_key_file_get_string(ConfigSkin,"main",main_btns[i].sub[j]);
			assert(tmp);
			p=tmp;
			normal=strtok(p,",");
			over=strtok(0,",");
			down=strtok(0,",");
			assert(normal!=NULL);
			btn.normal=normal;btn.over=over;btn.down=down;
			btn.click=main_btns[i].cb;btn.arg=LINT_TO_PTR(!j);
			y_ui_button_update(main_btns[i].id+j,&btn);
			l_free(tmp);
		}
		l_free(btn.font);
		l_free(btn.color);
	}

	MainNoShow=y_im_get_config_int("main","noshow");
	if(MainNoShow)
	{
		y_ui_main_show(0);
	}
	else if(MainShow)
	{
		MainShow=0;
		YongShowMain(1);
	}
	tip_main=y_im_get_config_int("main","tip");
}

void update_tray_icon(void)
{
	UI_TRAY param;
	char *tmp;
	char *im_icon=NULL;

	param.enable=y_im_get_config_int("main","tray");
	if(!ConfigSkin) tmp=NULL;
	else tmp=l_key_file_get_string(ConfigSkin,"tray","icon");
	if(!tmp)
	{
		param.icon[0]=param.icon[1]=0;
		param.enable=0;
	}
	else
	{
		im_icon=y_im_get_im_config_string(im.Index,"icon");
		param.icon[0]=strtok(tmp," \n");
		param.icon[1]=strtok(NULL," \n");
		if(im_icon) param.icon[0]=im_icon;
	}
	y_ui_tray_update(&param);
	l_free(tmp);
	l_free(im_icon);
	y_ui_tray_status(MainShow?UI_TRAY_ON:UI_TRAY_OFF);
	tmp=y_im_get_im_name(im.Index);
	if(tmp)
	{
		y_ui_tray_tooltip(tmp);
		l_free(tmp);
	}
}

int get_input_strip(void)
{
	char *tmp;
	tmp=y_im_get_config_string("input","strip");
	if(tmp)
	{
		int pre=0,suf=-1,spre=0,ssuf=-1;
		l_sscanf(tmp,"%d %d;%d %d",&pre,&suf,&spre,&ssuf);
		l_free(tmp);
		if(pre<3 || pre>32) pre=9;
		if(suf>32 || suf<0) suf=1;
		if(spre<3 || spre>32) spre=14;
		if(ssuf>32 || ssuf<0) ssuf=2;
		if(spre<pre) spre=pre;
		return (ssuf<<24)|(spre<<16)|(suf<<8)|pre;
	}
	return 9;
}

void update_input_window(void)
{
	UI_INPUT param;
	char *tmp,*p,*t;
	int i;

	memset(&param,0,sizeof(param));
	param.scale=l_key_file_get_int(ConfigSkin,"input","scale");
	tmp=(char *)l_key_file_get_data(ConfigSkin,"input","line_width");
	if(!tmp)
		param.line_width=1;
	else
		param.line_width=strtod(tmp,NULL);
	param.line=l_key_file_get_int(ConfigSkin,"input","line");
	param.caret=l_key_file_get_int(ConfigSkin,"input","caret");
	param.page=l_key_file_get_int(ConfigSkin,"input","page");
	param.bg=l_key_file_get_string(ConfigSkin,"input","bg");
	if(!param.bg) param.bg=l_strdup("#ffffff");
	param.tran=l_key_file_get_int(ConfigSkin,"input","tran");
	if(param.tran<0 || param.tran>255) param.tran=0;	

	if(param.bg[0]=='#')
	{
		tmp=l_key_file_get_string(ConfigSkin,"input","size");
		if(tmp)
		{
			l_sscanf(tmp,"%d,%d",&param.w,&param.h);
			l_free(tmp);
		}
		else
		{
			param.w=192;param.h=50;
		}
		param.border=l_key_file_get_string(ConfigSkin,"input","border");
		if(!param.border)
			param.border=l_strdup("#CBCAE6");
	}
	tmp=l_key_file_get_string(ConfigSkin,"input","msize");
	if(tmp)
	{
		l_sscanf(tmp,"%d,%d",&param.mw,&param.mh);
		l_free(tmp);
	}
	tmp=l_key_file_get_string(ConfigSkin,"input","height");
	if(tmp)
	{
		l_sscanf(tmp,"%d",&param.h);
		l_free(tmp);
	}
	
	tmp=l_key_file_get_string(ConfigSkin,"input","stretch");
	if(tmp)
	{
		l_sscanf(tmp,"%hd,%hd %hd,%hd",
				&param.left,&param.right,
				&param.top,&param.bottom);
		l_free(tmp);
		if(param.mw==0 || param.mw<param.left+param.right)
			param.mw=param.left+param.right+1;
	}
	
	tmp=l_key_file_get_string(ConfigSkin,"input","work");
	if(tmp)
	{
		l_sscanf(tmp,"%hd,%hd %hd",&param.work_left,&param.work_right,
				&param.work_bottom);
		l_free(tmp);
	}

	t=tmp=y_im_get_config_string("input","color");
	if(!t) t=tmp=l_key_file_get_string(ConfigSkin,"input","color");
	if(!t) t=tmp=l_strdup("");
	p=strtok(t,",");param.text[0]=l_strdup(p?p:"#0042C8");
	p=strtok(0,",");param.text[1]=l_strdup(p?p:"#161343");
	p=strtok(0,",");param.text[2]=l_strdup(p?p:"#ff0084");
	p=strtok(0,",");param.text[3]=l_strdup(p?p:"#669f42");
	p=strtok(0,",");param.text[4]=l_strdup(p?p:"#008000");
	if(p) p=strtok(0,",");
	param.text[5]=l_strdup(p?p:param.text[0]);
	if(p) p=strtok(0,",");
	param.text[6]=l_strdup(p?p:param.text[5]);
	l_free(tmp);

	tmp=y_im_get_config_string("input","font");
	if(tmp && !tmp[0])
	{
		l_free(tmp);
		tmp=0;
	}
	if(!tmp) tmp=l_key_file_get_string(ConfigSkin,"input","font");
	if(!tmp) tmp=l_strdup("Monospace 12");
	param.font=tmp;

	tmp=l_key_file_get_string(ConfigSkin,"input","code");
	if(tmp)
	{
		l_sscanf(tmp,"%d,%d",&param.code.x,&param.code.y);
		l_free(tmp);
	}
	else
	{
		param.code.x=10;param.code.y=3;
	}
	tmp=l_key_file_get_string(ConfigSkin,"input","cand");
	if(tmp)
	{
		l_sscanf(tmp,"%d,%d",&param.cand.x,&param.cand.y);
		l_free(tmp);
	}
	else
	{
		param.cand.x=10;param.cand.y=28;
	}

	param.hint=y_im_get_config_int("input","hint");
	param.no=l_key_file_get_int(ConfigSkin,"input","no");
	param.sep=l_key_file_get_string(ConfigSkin,"input","sep");
	param.space=l_key_file_get_int(ConfigSkin,"input","space");
	if(!param.space) param.space=10;
	param.root=y_im_get_config_int("input","root");
	param.noshow=y_im_get_config_int("input","noshow");
	param.strip=get_input_strip();
	tmp=y_im_get_config_string("input","offset");
	if(!tmp)
		tmp=l_key_file_get_string(ConfigSkin,"input","offset");
	if(tmp)
	{
		l_sscanf(tmp,"%d,%d",&param.off.x,&param.off.y);
		l_free(tmp);
	}
	RootMode=param.root?TRUE:FALSE;
	InputNoShow=param.noshow;
	tmp=y_im_get_config_string("input","pos");
	if(tmp)
	{
		int ret=l_sscanf(tmp,"%d,%d",&param.x,&param.y);
		if(ret!=2) param.y=-1;
		l_free(tmp);
	}
	param.cand_max=l_key_file_get_int(ConfigSkin,"input","cand_max");

	y_ui_input_update(&param);
	l_free(param.bg);
	l_free(param.border);
	l_free(param.font);
	l_free(param.sep);
	for(i=0;i<L_ARRAY_SIZE(param.text);i++)
		l_free(param.text[i]);
		
	y_ui_cfg_ctrl("onspot",y_im_get_config_int("IM","onspot"));
}

static void update_config_translate(void)
{
	char *config=y_im_get_config_string("main","translate");
	if(!config) return;
	y_translate_init(config);
	l_free(config);
}

static void update_config_skin(void)
{
	char path[256];
	char *tmp;
	char *end;
	
	l_key_file_free(ConfigSkin);
	ConfigSkin=0;
	
	tmp=y_im_get_im_config_string(im.Index,"skin");
	if(tmp && !tmp[0])
	{
		l_free(tmp);
		tmp=NULL;
	}
	if(!tmp)
	{
		tmp=y_im_get_config_string("IM","skin");
		if(tmp && !tmp[0])
		{
			l_free(tmp);
			tmp=NULL;
		}
	}
	if(!tmp) tmp=l_strdup("skin");
	end=tmp+strlen(tmp)-1;
	while(end>=tmp && *end==' ')
		*end=0;

	if(l_str_has_suffix(tmp,".ini"))
	{
		char *p;
		strcpy(path,tmp);
		p=strrchr(tmp,'/');
		if(p) *p=0;
		ConfigSkin=l_key_file_open(path,1,y_im_get_path("HOME"),NULL);
	}
	else
	{
		char *p;
		int ret,line;
		ret=l_sscanf(tmp,"%s %d",path,&line);
		if(ret==2)
		{
			sprintf(path+strlen(path),"/skin%d.ini",line);
			p=strrchr(tmp,' ');
			if(p) *p=0;
			ConfigSkin=l_key_file_open(path,0,y_im_get_path("HOME"),
					y_im_get_path("DATA"),NULL);
		}
		if(!ConfigSkin)
		{
			l_sscanf(tmp,"%s",path);
			strcat(path,"/skin.ini");
			ConfigSkin=l_key_file_open(path,1,y_im_get_path("HOME"),
					y_im_get_path("DATA"),NULL);
		}
	}

	y_ui_skin_path(tmp);
	l_free(tmp);
}

const void *YongGetSelectNumber(int n)
{
#ifdef _WIN32
	static WCHAR temp[4];
#else
	static char temp[4];
#endif
	if(sym_select && sym_select_count)
	{
		if(n>=sym_select_count)
		{
			temp[0]=0;
			return temp;
		}
#ifdef _WIN32
		l_utf8_to_utf16(sym_select[n],temp,sizeof(temp));
#else
		snprintf(temp,sizeof(temp),"%s",sym_select[n]);
#endif
		return temp;
	}
	if(L_UNLIKELY(key_select_n[n]==' ' || !key_select_n[n]))
	{
		temp[0]=0;
	}
	else
	{
		temp[0]=key_select_n[n];
		temp[1]='.';
		temp[2]=0;
	}
	return temp;
}

void update_key_config(void)
{
	int key;
	int i;
	char *tmp;

	key=y_im_get_key("trigger",-1,CTRL_SPACE);
	//if(YongTriggerKey(key)!=-1)
	if(y_xim_trigger_key(key)!=-1)
		key_trigger=key;

	virt_key_add=y_im_get_key("add",-1,CTRL_INSERT);
	virt_key_del=y_im_get_key("del",-1,CTRL_DELETE);
	virt_key_query=y_im_get_key("query",-1,KEYM_CTRL|'/');
	
	key_dict=y_im_get_key("dict",-1,ALT_ENTER);

	key_select[0]=y_im_get_key("select",0,YK_LSHIFT);
	key_select[1]=y_im_get_key("select",1,YK_RSHIFT);
	for(i=2;i<9;i++)
	{
		key_select[i]=y_im_get_key("select",i,YK_NONE);
		if(key_select[i]==YK_NONE) break;
	}
	
	memset(key_select_n,0,sizeof(key_select_n));
	tmp=y_im_get_config_string("key","select_n");
	if(tmp)
	{
		snprintf(key_select_n,sizeof(key_select_n),"%s",tmp);
		l_free(tmp);
	}
	else
	{
		if(num_mode==0)
			strcpy(key_select_n,"1234567890");
	}
	
	key_cnen[0]=y_im_get_key("CNen",0,YK_LCTRL);
	key_cnen[1]=y_im_get_key("CNen",1,YK_NONE);
	key_temp_english=y_im_get_key("tEN",-1,YK_NONE);
	if(key_temp_english>='A' && key_temp_english<='Z')
		key_temp_english=key_temp_english-'A'+'a';

	key_switch=y_im_get_key("switch",-1,CTRL_LSHIFT);
	key_s2t=y_im_get_key("s2t",-1,CTRL_ALT_F);

	key_pageup=y_im_get_key("page",0,'-');
	key_pagedown=y_im_get_key("page",1,'=');

	key_word_left=y_im_get_key("w2c",0,'[');
	key_word_right=y_im_get_key("w2c",1,']');

	key_input_show=y_im_get_key("ishow",-1,'`');
	key_main_show=y_im_get_key("mshow",-1,CTRL_ALT_M);

	key_repeat=y_im_get_key("repeat",-1,YK_NONE);
	if(key_repeat!=YK_NONE && key_repeat<0x80)
		key_repeat=tolower(key_repeat);
	key_stop=y_im_get_key("stop",-1,YK_NONE);
	key_keymap=y_im_get_key("keymap",-1,YK_NONE);
	key_keyboard=y_im_get_key("keyboard",0,CTRL_ALT_K);
	key_keyboard_s=y_im_get_key("keyboard",1,CTRL_SHIFT_K);
	key_bihua=y_im_get_key("bihua",-1,'`');
	if(key_bihua >= 0x80) key_bihua=YK_NONE;
	key_crab=y_im_get_key("crab",-1,CTRL_SHIFT_ALT_H);

	key_switch_default=y_im_get_key("switch_default",-1,0);
	for(i=0;i<10;i++)
	{
		char temp[32];
		sprintf(temp,"switch_%c",i+'0');
		key_switch_to[i]=y_im_get_key(temp,-1,0);
	}
	for(i=0;i<10;i++)
	{
		char *s;
		s=y_im_get_im_config_string(i,"switch");
		if(!s)
			continue;
		key_switch_to[i]=y_im_str_to_key(s);
		l_free(s);
	}

#ifndef CFG_NO_KEYTOOL
	y_key_tools_free(key_tools);
	key_tools=y_key_tools_load();
#endif

}

void update_im(void)
{
	char *eim;
	char which[32];
	int tmp;
	EXTRA_IM *bim;

	bim=y_bihua_eim();

	/* use eim as temp here*/

	eim=y_im_get_config_string("IM","num");
	if(eim && !strcmp(eim,"push"))
		num_mode=1;
	else
		num_mode=0;
	l_free(eim);

	sprintf(which,"%d",im.Index);
	eim=y_im_get_config_string("IM",which);
	if(!eim)
	{
		im.Index=0;
		eim=y_im_get_config_string("IM","0");
	}
	if(!eim) return;
	LoadExtraIM(&im,eim);
	l_free(eim);

	im.Preedit=y_im_get_config_int("IM","preedit");
	tmp=y_im_get_config_int("IM","s2t_biaodian");
	s2t_biaodian(tmp);
	tmp=y_im_get_config_int("IM","s2t_m");
	s2t_multiple(tmp);
	im.CandWord=y_im_get_config_int("IM","cand");
	if(im.CandWord<2) im.CandWord=2;
	else if(im.CandWord>10) im.CandWord=10;
	if(ConfigSkin)
	{
		const char *s=l_key_file_get_data(ConfigSkin,"input","stretch");
		if(!s || !strchr(s,' '))
		{
			tmp=l_key_file_get_int(ConfigSkin,"input","cand_max");
			if(tmp>=2 && tmp<=10)
				if(im.CandWord>tmp) im.CandWord=tmp;
		}
	}
	if(im.eim) im.eim->CandWordMax=im.CandWord;

	if(im.eim)
	{
		eim=y_im_get_im_name(im.Index);
		if(eim)
		{
			y_ui_button_label(UI_BTN_NAME,s2t_conv(eim));
			l_free(eim);
		}
	}
	/* something should different for engines should update */
	im.Hint=y_im_get_config_int("input","hint");	
	y_ui_cfg_ctrl("strip",get_input_strip());
	
	im.AssocLen=y_im_get_im_config_int(im.Index,"assoc_len");
	im.AssocLoop=y_im_get_im_config_int(im.Index,"assoc_loop");
	InitExtraIM(&im,bim,im.eim?im.eim->Bihua:NULL);
	
	/* diff engine may have different keys */
	update_key_config();	
	y_im_key_desc_update();
#ifndef CFG_NO_REPLACE
	y_replace_init(y_im_get_config_data("IM","crab"));
#endif
	
	eim=y_im_get_config_string("IM","space");
	if(eim && !strcmp(eim,"full"))
		space_mode=1;
	else
		space_mode=0;
	l_free(eim);
	
	eim=y_im_get_config_string("IM","enter");
	if(eim && !strcmp(eim,"clear"))
		enter_mode=1;
	else if(eim && !strcmp(eim,"commit"))
		enter_mode=2;
	else
		enter_mode=0;
	l_free(eim);
	
	auto_show=y_im_get_config_int("input","auto_show");
	kp_mode=y_im_get_config_int("IM","keypad");
	abcd_mode=y_im_get_config_int("IM","ABCD");
	cnen_mode=y_im_get_config_int("IM","CNen_commit");
	caps_bd_mode=y_im_get_config_int("IM","caps_bd");
	
	l_strfreev(sym_select);
	sym_select=NULL;
	sym_select_count=0;
	eim=y_im_get_config_string("input","select");
	if(eim && eim[0])
	{
		sym_select=l_strsplit(eim,' ');
		sym_select_count=l_strv_length(sym_select);
	}
	l_free(eim);
	
	alt_bd_disable=y_im_get_config_int("IM","alt_bd_disable");
	y_im_load_punc();
	
	/* reload english mode im */
	y_english_destroy();
	y_english_init();
	InitExtraIM(&im,y_english_eim(),0);
	InitExtraIM(&im,y_select_eim(),0);
	
	y_im_history_update();
	
#ifndef CFG_NO_LAYOUT
	/* load layout */
	if(im.layout)
	{
		y_layout_free(im.layout);
		im.layout=NULL;
	}
	eim=y_im_get_config_string("IM","layout");
	if(eim)
	{
		im.layout=y_layout_load(eim);
		l_free(eim);
	}
#endif
}

int YongSwitchIM(int id)
{
	int prev;
	YongResetIM();
	if(id<0)
	{
		char temp[32];
		id=im.Index+1;
		sprintf(temp,"%d",id);
		if(!y_im_has_config("IM",temp))
			id=0;
	}
	if(im.Index==id)
		return -1;
	prev=y_im_has_im_config(im.Index,"skin");
	im.Index=id;
	y_im_update_main_config();
	update_im();
	if(prev || y_im_has_im_config(id,"skin"))
	{
		update_config_skin();
		update_main_window();
		update_input_window();
	}
	update_tray_icon();
	YongUpdateMain(0);
	return 0;
}

void YongMoveInput(int x,int y)
{
	CONNECT_ID *id;
	int last_pos=0;

	id=y_xim_get_connect();
	if(!id) return;
	
	CaretUpdate=true;
	
	if(RootMode)
	{
		if(id->x!=POSITION_ORIG)
		{
			return;
		}
		x=y=POSITION_ORIG;
	}
	else if(x!=POSITION_ORIG && x==id->x && y==id->y)
	{
		return;
	}

	if(x==POSITION_ORIG || RootMode)
	{
		last_pos=1;
		if(id->x!=POSITION_ORIG)
		{
			x=id->x;
			y=id->y;
		}
	}
	y_ui_input_move(!last_pos,&x,&y);
	
	id->x=x;id->y=y;
}

void DOUT(const char *fmt,...);
static void HideInputLater(void *unused)
{
	y_ui_input_show(0);
	InputShow=0;
}

void YongShowInput(int show)
{
	EXTRA_IM *eim=CURRENT_EIM();
	CONNECT_ID *id=y_xim_get_connect();
	
	if(!InputShow && show && id && !id->state)
		return;

	if(show && !InputShow && (im.CodeInputEngine[0] || (eim && eim->StringGet[0])/* ||
		(InputNoShow==2 && MainShow && 
		id && id->lang!=LANG_EN && id->corner!=CORNER_FULL)*/))
	{
		YongMoveInput(POSITION_ORIG,POSITION_ORIG);
		if(InputNoShow!=1 || im.CodeInputEngine[0]=='`' || im.EnglishMode)
		{
			y_ui_input_show(1);
			InputShow=TRUE;
		}
	}
	else if(!show && InputShow && (InputNoShow!=2 || !MainShow ||
		!id || id->lang==LANG_EN || id->corner==CORNER_FULL))
	{
		y_ui_input_show(0);
		InputShow=FALSE;
	}
	else if(!show && InputShow && InputNoShow==2)
	{
		y_ui_timer_add(3000,HideInputLater,NULL);
	}
	if(InputNoShow==1 && auto_show && eim)
	{
		if(!InputShow && eim->CurCandPage==0 &&
			eim->CodeLen>=auto_show && eim->CandWordCount>1)
		{
			y_ui_input_show(1);
			InputShow=TRUE;
		}
		else if(InputShow && eim->CodeLen<auto_show)
		{
			y_ui_input_show(0);
			InputShow=FALSE;
		}
	}
	if(show && !InputShow && im.EnglishMode)
	{
		y_ui_input_show(1);
		InputShow=TRUE;
	}
	if(!show && InputShow)
	{
		y_ui_input_draw();
	}
}

static void ShowLangTipLater(void *tip)
{
	CONNECT_ID *id=y_xim_get_connect();
	if(!id)
		return;
	if(!CaretUpdate)
		id->x=id->y=POSITION_ORIG;
	y_ui_show_tip(tip);	
}

void YongShowMain(int show)
{
	CONNECT_ID *id=y_xim_get_connect();
	if(id) YongUpdateMain(id);
	if(show && !MainShow)
	{
		MainShow=TRUE;
		YongUpdateMain(NULL);
		if(!MainNoShow)
		{
			y_ui_main_show(1);
		}
		else
		{
			if(id && tip_main)
			{
				CaretUpdate=false;
				y_ui_timer_add(100,ShowLangTipLater,id->lang==0?YT("中文"):YT("英文"));
			}
		}
	}
	else if(!show && MainShow)
	{
		y_ui_main_show(0);
		MainShow=FALSE;
	}
	if(InputNoShow==2)
		YongShowInput(show);
	y_ui_tray_status(MainShow?UI_TRAY_ON:UI_TRAY_OFF);
}

static void YongFlushResult(void)
{
	EXTRA_IM *eim=CURRENT_EIM();
	if(im.InAssoc)
	{
		YongResetIM();
		return;
	}
	if(im.EnglishMode && eim->CodeLen && eim->CodeInput[0])
	{
		if((uint8_t)im.CodeInputEngine[0]==key_temp_english)
		{
			if(im.CodeInputEngine[1])
				y_xim_send_string2(im.CodeInputEngine+1,SEND_FLUSH|SEND_RAW);
			else
				y_xim_send_string2(im.CodeInputEngine,SEND_FLUSH|SEND_RAW);
		}
		else
		{
			y_xim_send_string2(im.CodeInputEngine,SEND_FLUSH|SEND_RAW);
		}
	}
	else
	{
		if(eim && eim->CandWordCount)
		{
			y_xim_send_string(s2t_conv(eim->GetCandWord(eim->SelectIndex)));
		}
	}
	YongResetIM();
}

#if !defined(CFG_NO_DICT)
static int YongCallDict(void)
{
	char url[128];
	char temp[128];
	char *site;
	EXTRA_IM *eim=YongCurrentIM();
	
	if(!eim) return 0;
	if(!eim->CodeInput[0] && !eim->CandWordCount)
		return 0;

#if !defined(CFG_NO_DICT)
	if(local_dic)
	{
		char cand[64];
		char *s;
		if(im.EnglishMode)
		{
			s=eim->CodeInput;
			if(s[0] && s[0]==key_temp_english) s++;
		}
		else
		{
			y_im_get_real_cand(eim->CandTable[eim->SelectIndex],cand,sizeof(cand));
			s=cand;
		}
		if(0==y_dict_query_and_show(local_dic,s))
		{
			YongResetIM();
			return 1;
		}
	}
#endif
	if(im.EnglishMode)
		strcpy(temp,(im.CodeInputEngine[0]==key_temp_english)?(eim->CodeInput+1):eim->CodeInput);
	else
	{
		char cand[64];
		y_im_get_real_cand(eim->CandTable[eim->SelectIndex],cand,sizeof(cand));
		y_im_url_encode(cand,temp);
	}
	site=y_im_get_config_string("IM",im.EnglishMode?"dict_en":"dict_cn");
	if(site)
	{
		sprintf(url,site,temp);
		l_free(site);
	}
	else
	{
		site=im.EnglishMode?"http://www.iciba.com/%s/":
			"http://hanyu.iciba.com/hy/%s/";
		sprintf(url,site,temp);
	}
	y_xim_explore_url(url);
	YongResetIM();
	return 1;
}
#endif

int YongHotKey(int key)
{
	CONNECT_ID *id;
	int i;

	id=y_xim_get_connect();
	if(!id)
		return 0;
	if(!key)
		return 0;

	if(key==key_trigger)
	{
		if(id->state)
		{
			y_xim_enable(0);
		}
		else
		{
			/* we should reset before enable */
			YongResetIM();
			y_xim_enable(1);
			id->state=1;
			YongShowMain(1);
		}
#ifdef _WIN32
		if(MainNoShow && tip_main)
		{
			if(id->state) y_ui_show_tip(YT("打开输入法"));
			else y_ui_show_tip(YT("关闭输入法"));
		}
#endif
		return 1;
	}
	else if(key==key_cnen[0] || key==key_cnen[1])
	{
		if(id->state)
		{
			int prev=id->lang;
			YongSetLang(!id->lang);
			if(prev!=id->lang && tip_main)
				y_ui_show_tip(YT("切换到：%s"),(id->lang==LANG_CN?YT("中文"):YT("英文")));
			return 1;
		}
	}
	else if(key==SHIFT_SPACE)
	{
		if(id->state/* && id->lang==LANG_CN*/)
		{
			YongSetCorner(!id->corner);
			if(tip_main)
				y_ui_show_tip(YT("切换到：%s"),(id->corner==CORNER_FULL?YT("全角"):YT("半角")));
			return 1;
		}
	}
	else if(key==(KEYM_CTRL|'.'))
	{
		if(id->state && id->lang==LANG_CN)
		{
			YongSetBiaodian(!id->biaodian);
			if(tip_main)
				y_ui_show_tip(YT("切换到：%s"),(id->biaodian==LANG_CN?YT("中文标点"):YT("英文标点")));
			return 1;
		}
	}
	else if(key==key_switch)
	{
		if(id->state)
		{
			char *name;
			YongSwitchIM(-1);
			name=y_im_get_im_name(im.Index);
			if(name!=NULL)
			{
				if(tip_main)
					y_ui_show_tip(YT("切换到：%s"),name);
				l_free(name);
			}
			return 1;
		}
	}
	else if(key==key_s2t)
	{
		if(id->state)
		{
			const char *temp;
			YongSetTrad(!id->trad);
			temp=(id->trad==0?YT("简体"):YT("繁体"));
			if(tip_main)
				y_ui_show_tip(YT("切换到：%s"),temp);
			return 1;
		}
	}
	else if(key==key_main_show && MainNoShow)
	{
		if(MainShow)
			y_ui_main_show(1);
		return 1;
	}
	else if(key==key_keymap)
	{
		y_im_show_keymap();
	}
#ifndef CFG_NO_KEYBOARD
	else if(key==key_keyboard)
	{
		y_kbd_show(-1);
		return 1;
	}
	else if(key==key_keyboard_s)
	{
		y_kbd_popup_menu();
		return 1;
	}
#endif
#ifndef CFG_NO_DICT
	else if(key==key_dict)
	{
		return YongCallDict();
	}
#endif
	else if(key==key_switch_default)
	{
		int d=y_im_get_config_int("IM","default");
		YongSwitchIM(d);
		return 1;
	}
#ifndef CFG_NO_REPLACE
	else if(key==key_crab)
	{
		y_replace_enable(-1);
		return 1;
	}
#endif
	else if(key&KEYM_MASK)
	{
		for(i=0;i<10;i++)
		{
			if(key==key_switch_to[i])
			{
				YongSwitchIM(i);
				return 1;
			}
		}
#ifndef CFG_NO_KEYTOOL
		if(id->state && y_key_tools_run(key_tools,key))
		{
			return 1;
		}
#endif
	}
	return 0;
}

#define YONG_DO_LEGEND() \
	if(im.AssocLen && (!im.InAssoc || im.AssocLoop)) \
	{ \
		if(!strcmp(eim->StringGet,"$LAST")) \
			strcpy(eim->StringGet,y_xim_get_last()); \
		ret=eim->GetCandWords(PAGE_LEGEND); \
		if(ret==IMR_DISPLAY) \
		{ \
			im.InAssoc=1; \
			goto IMR_TEST; \
		} \
		else \
		{ \
			im.InAssoc=0; \
		} \
	}

static int YongAppendPunc(CONNECT_ID *id,char *res,int key)
{
	const char *biaodian=YongGetPunc(key,id->biaodian,0);
	int last=y_im_last_key(0);
	last&=~KEYM_KEYPAD;
	if(last<='9' && last>='0' && !res[0] && key=='.')
		return 0;
	if(biaodian)
	{
		strcat(res,s2t_conv(biaodian));
		return 1;
	}
	if(id->corner==CORNER_FULL || (space_mode && key==YK_SPACE))
	{
		const char *biaodian;
		if(key&KEYM_SHIFT)
			key=YK_CODE(key);
		biaodian=YongFullChar(key);
		if(biaodian)
			strcat(res,s2t_conv(biaodian));
		return biaodian?1:0;
	}
	return 0;
}

int YongKeyInput(int key,int mod)
{
	CONNECT_ID *id;
	int ret=IMR_NEXT;

	id=y_xim_get_connect();
	if(!id || !id->state) return 0;
	if(id->corner==CORNER_FULL && id->lang==LANG_EN)
	{
		const char *ret;
		if(kp_mode==0 && (key&KEYM_KEYPAD))
			key&=~KEYM_KEYPAD;
		if(key&KEYM_SHIFT)
			key=YK_CODE(key);
		ret=YongFullChar(key);
		if(ret) y_xim_send_string(ret);
		return ret?1:0;
	}
	
	if(id->lang==LANG_EN)
		return 0;
	if((kp_mode==0 || im.EnglishMode) && (key&KEYM_MASK))
	{
		key&=~KEYM_KEYPAD;
	}
	if(key!='.' && key!=(KEYM_ALT|'.'))
	{
		// now only m.x mode used last, so clean it here
		// should change it future
		y_im_last_key(0);
	}
	if(key>='A' && key<='Z' && !im.EnglishMode)
	{
		YongFlushResult();
		return 0;
	}

	if(key==YK_ESC && (im.CodeInputEngine[0] || im.StringGetEngine[0]))
	{
		YongResetIM();
		return 1;
	}
	if((key&KEYM_MASK)==KEYM_SHIFT)
	{
		int temp=YK_CODE(key);
		/* test if it is < 0x80, for win2000 crash */
		if(temp<0x80 && isgraph(temp))
			key=temp;
		if(abcd_mode && temp>='A' && temp<='Z' && im.eim && im.eim->CodeLen==0 && !im.EnglishMode)
		{
			YongFlushResult();
			return 0;
		}
		if(key>='A' && key<='Z' && (mod&KEYM_CAPS))
		{
			key+='a'-'A';
		}
	}
	if(im.EnglishMode==0 && 
		((key>='A' && key<='Z' && !(im.eim && (im.eim->Flag&IM_FLAG_CAPITAL)))
			|| (key_temp_english && im.CodeInputEngine[0]==key_temp_english && im.CodeInputEngine[1]==0 && !(key&KEYM_MASK) && isgraph(key))
			/* || (key==key_temp_english && !im.CodeInputEngine[0])*/)
			)
	{
ENGLISH_MODE:
		if(im.CodeInputEngine[0]==key_temp_english)
		{
			EXTRA_IM *eim=y_english_eim();
			YongResetIM_();
			eim->DoInput(key_temp_english);
		}
		else
		{
			YongFlushResult();
		}
		im.EnglishMode=1;
	}
	if(im.eim)
	{
		EXTRA_IM *eim=im.eim;
		char *url;
		key=y_im_virt_key_conv(key);
		if(key==key_input_show)
		{
			if(!InputShow && InputNoShow && (eim->CandWordCount || eim->CodeInput[0]))
			{
				int tmp=InputNoShow;
				InputNoShow=0;
				YongShowInput(1);
				InputNoShow=tmp;
				return 1;
			}
			else if(InputShow && eim->CandWordCount)
			{
				YongShowInput(0);
				YongShowInput(1);
				return 1;
			}
		}
		if(key_bihua && ((key==key_bihua &&!im.CodeInputEngine[0]) ||im.BihuaMode) &&
				y_bihua_good())
		{
			if(key!=YK_VIRT_QUERY)
			{
				eim=y_bihua_eim();
				im.BihuaMode=1;
			}
			else
			{
				EXTRA_IM *bh=y_bihua_eim();
				eim->CandWordCount=bh->CandWordCount;
				eim->SelectIndex=bh->SelectIndex;
			}
		}
		if(!im.EnglishMode && key==key_repeat && !eim->CodeLen && !eim->CandWordCount)
		{
			y_xim_send_string(0);
			return 1;
		}
		if(im.EnglishMode)
		{
			eim=y_english_eim();
		}
		else if(im.SelectMode)
		{
			eim=y_select_eim();
		}
		url=y_im_find_url2(eim->CodeInput,key);

		if(!im.StopInput)
		{
			if(key==YK_VIRT_QUERY && eim->CandWordCount>0 && !strcmp(eim->CandTable[eim->SelectIndex],"$LAST"))
			{
				strcpy(eim->CandTable[eim->SelectIndex],y_xim_get_last());
			}
			ret=eim->DoInput(key);
			if(im.ChinglishMode && ret==IMR_DISPLAY &&
					key==YK_BACKSPACE && eim->CaretPos==eim->CodeLen)
			{
				YongSetLang(-1);
				if(!im.EnglishMode)
					return 1;
			}
		}
		if(url && (ret==IMR_NEXT || ret==IMR_COMMIT_DISPLAY || ret==IMR_BLOCK))
		{
			strcpy(eim->CodeInput,url);
			YongSetLang(LANG_EN);
			return 1;
		}
		if(eim->CodeInput[0]) im.InAssoc=0;
IMR_TEST:
		{
			char *p=eim->CodeInput;
			//p=y_im_key_desc_translate(p,eim->CandTable[eim->SelectIndex]);
			//y_im_str_encode(p,im.CodeInput,DONT_ESCAPE);
			y_im_key_desc_translate(p,0,eim->CandTable[eim->SelectIndex],(char*)im.CodeInput,MAX_CODE_LEN+1);
			y_im_str_encode(s2t_conv(eim->StringGet),im.StringGet,0);
		}
		
		if(eim==y_bihua_eim() && im.eim && eim->CandWordCount)
		{
			im.eim->CandWordCount=eim->CandWordCount;
			im.eim->SelectIndex=eim->SelectIndex;
			im.eim->DoInput(YK_VIRT_TIP);
		}
		switch(ret)
		{
			case IMR_BLOCK:
				return 1;
			case IMR_PASS:
				return 0;
			case IMR_CLEAN_PASS:
				YongResetIM();
				return 0;
			case IMR_CLEAN:
				YongResetIM();
				return 1;
			case IMR_NEXT:
				if(key==key_temp_english && !eim->CodeInput[0] && im.EnglishMode==0)
				{
					goto ENGLISH_MODE;
				}
				break;
			case IMR_DISPLAY:
				y_ui_input_draw();
				return 1;
			case IMR_COMMIT:
				if(s2t_select(eim->StringGet))
				{
					y_ui_input_draw();
					return 1;
				}
				y_xim_send_string(s2t_conv(eim->StringGet));
				YONG_DO_LEGEND();
				YongResetIM();
				return 1;
			case IMR_COMMIT_DISPLAY:
				y_xim_send_string(s2t_conv(eim->StringGet));
				eim->StringGet[0]=0;
				im.StringGet[0]=0;
				y_ui_input_draw();
				im.InAssoc=0;
				return 1;
			case IMR_CHINGLISH:
				im.ChinglishMode=1;
			case IMR_ENGLISH:
				YongSetLang(LANG_EN);
				return 1;
			case IMR_PUNC:
			{
				const char *t=s2t_conv(eim->StringGet);
				if(t!=eim->StringGet)
					strcpy(eim->StringGet,t);
				if(key==' ') /* fix preedit of english at firefox */
				{
					strcat(eim->StringGet," ");
				}
				else
				{
					YongAppendPunc(id,eim->StringGet,key);
				}
				y_xim_send_string(eim->StringGet);
				YongResetIM();
				return 1;
			}
			default:
				break;
		}
		if(ret==IMR_NEXT && (eim->CandWordCount || eim->CodeInput[0]))
		{
			int pos;
			if(key==key_pagedown)
			{
				ret=eim->GetCandWords(PAGE_NEXT);
				goto IMR_TEST;
			}
			else if(key==key_pageup)
			{
				ret=eim->GetCandWords(PAGE_PREV);
				goto IMR_TEST;
			}
			else if(key==key_stop && !im.StopInput)
			{
				if(eim->CandWordCount==1)
				{
					char *p=eim->GetCandWord(eim->SelectIndex);
					if(p)
					{
						y_xim_send_string(s2t_conv(p));
						YONG_DO_LEGEND();
						YongResetIM();
						return 1;
					}
				}
				im.StopInput=1;
				return 1;
			}
			else if((pos=key_do_select(key))>=0)
			{
				if(eim->CandWordCount>pos)
				{
					char *p=eim->GetCandWord(pos);
					if(p)
					{
						if(s2t_select(p))
						{
							y_ui_input_draw();
							return 1;
						}
						y_xim_send_string(s2t_conv(p));
						YONG_DO_LEGEND();
						YongResetIM();
					}
					else
					{
						ret=IMR_DISPLAY;
						goto IMR_TEST;
					}
					return 1;
				}
			}
			else if(key==YK_SPACE || (enter_mode==2 && key==YK_ENTER && !im.EnglishMode))
			{
				char *p=eim->GetCandWord(eim->SelectIndex);
				if(p)
				{
					if(s2t_select(p))
					{
						y_ui_input_draw();
						return 1;
					}
					y_xim_send_string(s2t_conv(p));
					YONG_DO_LEGEND();
					YongResetIM();
				}
				else
				{
					// 在空码时按下选择键，原来是保留原样显示，但还是清空编码等状态比较好
					if(eim->StringGet[0] || eim->CandWordCount)
						ret=IMR_DISPLAY;
					else
						ret=IMR_CLEAN;
					goto IMR_TEST;
				}
				return 1;
			}
			else if(key==YK_ENTER)
			{
				if(im.EnglishMode)
				{
					YongFlushResult();
				}
				else if(enter_mode==0)
				{
					y_im_str_strip(eim->CodeInput);
					y_xim_send_string2(eim->CodeInput,SEND_FLUSH|SEND_RAW);
				}
				else
				{
					y_ui_input_draw();
				}
				if(im.InAssoc)
				{
					YongResetIM();
					return 0;
				}
				YongResetIM();
				return 1;
			}
			else if((key>='0' && key<='9')||(key>=YK_KP_0 && key<=YK_KP_9))
			{
				char *p;
				int i;
				int num_push=0;
				
				i=y_strchr_pos(key_select_n,key&~KEYM_KEYPAD);
				if((key & KEYM_KEYPAD) && kp_mode==2)
					num_push=1;
				if(num_mode==0 && !num_push) p=eim->GetCandWord(i);
				else if(im.InAssoc) p="";
				else p=eim->GetCandWord(eim->SelectIndex);
				if(p)
				{
					if(num_mode==0 && !num_push)
					{
						if(s2t_select(p))
						{
							y_ui_input_draw();
							return 1;
						}
						y_xim_send_string(s2t_conv(p));
						YONG_DO_LEGEND();
						YongResetIM();
						return 1;	
					}
					else if(id->corner==CORNER_FULL)
					{
						YongAppendPunc(id,eim->StringGet,key&~KEYM_MASK);
						y_xim_send_string(s2t_conv(p));
						YongResetIM();
						return 1;
					}
					else
					{
						sprintf(eim->StringGet,"%s%c",p,key&~KEYM_MASK);
						y_xim_send_string(s2t_conv(eim->StringGet));
						YongResetIM();
						y_im_last_key(key);
						return 1;	
					}									
				}
				else
				{
					ret=IMR_DISPLAY;
					goto IMR_TEST;
				}
			}
			else if(key==key_word_left)
			{
				const char *t,*t2;
				t=s2t_conv(&eim->CandTable[eim->SelectIndex][0]);
				t2=strstr(t,"，");
				if(t2)
				{
					int i;
					for(i=0;t<t2;i++)
						eim->StringGet[i]=*t++;
					eim->StringGet[i]=0;
				}
				else if(t[0]&0x80)
				{
					eim->StringGet[0]=t[0];
					eim->StringGet[1]=t[1];
					eim->StringGet[2]=0;
				}
				else
				{
					return 1;
				}
				y_xim_send_string(eim->StringGet);
				YongResetIM();
				return 1;
			}
			else if(key==key_word_right)
			{
				const char *t,*t2;
				t=s2t_conv(&eim->CandTable[eim->SelectIndex][0]);
				t2=strstr(t,"，");
				if(t2)
				{
					int i;
					t2+=2;
					for(i=0;*t2;i++)
						eim->StringGet[i]=*t2++;
					eim->StringGet[i]=0;
					y_xim_send_string(eim->StringGet);
					YongResetIM();
					return 1;
				}
				t+=strlen(t)-2;
				if(t[0]&0x80)
				{
					eim->StringGet[0]=t[0];
					eim->StringGet[1]=t[1];
					eim->StringGet[2]=0;
					y_xim_send_string(eim->StringGet);
					YongResetIM();
				}
				return 1;
			}
			else if(key==YK_UP)
			{
				if(eim->SelectIndex>0)
				{
					eim->SelectIndex=eim->SelectIndex-1;
					y_ui_input_draw();
				}
				else if(eim->CurCandPage>0)
				{
					eim->GetCandWords(PAGE_PREV);
					eim->SelectIndex=eim->CandWordCount-1;
					y_ui_input_draw();
				}
				return 1;
			}
			else if(key==YK_DOWN)
			{
				if(eim->SelectIndex<eim->CandWordCount-1)
				{
					eim->SelectIndex=eim->SelectIndex+1;
					y_ui_input_draw();
				}
				else if(eim->CurCandPage<eim->CandPageCount-1)
				{
					eim->GetCandWords(PAGE_NEXT);
					eim->SelectIndex=0;
					y_ui_input_draw();
				}
			
				return 1;
			}
			else if(im.SelectMode)
			{
				y_xim_send_string2(eim->GetCandWord(eim->SelectIndex),false);
				YongResetIM();
				ret=YongKeyInput(key,0);
				y_xim_send_string2("",true);
				return ret;
			}
			else if((key&KEYM_MASK)==KEYM_ALT/* && !alt_bd_disable*/)
			{
				const char *s;
				key=YK_CODE(key);
				if(!alt_bd_disable)
				{
					s=YongGetPunc(key,!id->biaodian,0);
					if(s && eim->CodeLen<MAX_CODE_LEN-1)
					{
						eim->CodeInput[eim->CodeLen++]=key;
						eim->CodeInput[eim->CodeLen]=0;
						YongSetLang(LANG_EN);
						return 1;
					}
				}
				if(im.EnglishMode && key>='1' && key<'1'+eim->CandWordCount)
				{
					char *p=eim->GetCandWord(key-'1');
					if(p!=NULL)
					{
						y_xim_send_string(p);
						YongResetIM();
					}
					else
					{
						ret=IMR_DISPLAY;
						goto IMR_TEST;
					}
					return 1;
				}
			}
		}
		else if(ret==IMR_NEXT && !eim->CodeInput[0])
		{
			if(key==' ')
				y_im_history_write(" ");
			else if(key=='\t')
				y_im_history_write("\t");
			else if(key=='\r')
				y_im_history_write("\n");
			if((key&KEYM_MASK)==KEYM_ALT && eim->CandWordCount==0 && !alt_bd_disable)
			{
				const char *s;
				key=YK_CODE(key);
				if(key=='.')
				{
					int last=y_im_last_key(0)&~KEYM_KEYPAD;
					if(last>='0' && last<='9' && id->biaodian==LANG_CN)
					{
						s=YongGetPunc(key,id->biaodian,0);
						if(s)
						{
							y_xim_send_string(s);
							return 0;
						}
					}
				}
				s=YongGetPunc(key,!id->biaodian,0);
				if(s)
				{
					y_xim_send_string(s);
					return 1;
				}
				return 0;
			}
			else if(key>='A' && key<='Z')
			{
				if(abcd_mode) return 0;
				goto ENGLISH_MODE;
			}
		}
	}
	if(ret==IMR_NEXT)
	{
		const char *biaodian;
		int last;
		
		if(im.Bing && key=='+')
			return 0;

		if(caps_bd_mode==LANG_EN && (mod&KEYM_CAPS))
			biaodian=YongGetPunc(key,LANG_EN,0);
		else
			biaodian=YongGetPunc(key,id->biaodian,0);
		last=y_im_last_key(0);
		last&=~KEYM_KEYPAD;
		if(last<='9' && last>='0' && key=='.')
			return 0;
		
		if(biaodian)
		{
			EXTRA_IM *eim=CURRENT_EIM();
			im.StringGet[0]=0;
			if(eim && (eim->CandWordCount || eim->StringGet[0]) && !im.InAssoc)
			{
				char *p=eim->GetCandWord(-1);
				if(p)
				{
					y_xim_send_string2(s2t_conv(p),0);
				}
			}
			y_xim_send_string2(s2t_conv(biaodian),SEND_BIAODIAN|SEND_FLUSH);
			YongResetIM();
			return 1;
		}
		if(id->corner==CORNER_FULL || (space_mode && key==YK_SPACE))
		{
			const char *ret;
			if(key&KEYM_SHIFT)
				key=YK_CODE(key);
			ret=YongFullChar(key);
			if(ret)
			{
				y_xim_send_string(ret);
			}
			return ret?1:0;
		}
	}
	y_im_last_key(key);
	if(!(key&~0x7f) && isgraph(key))
	{
		EXTRA_IM *eim=CURRENT_EIM();
		if(eim && eim->CandWordCount>0)
		{
			im.StringGet[0]=0;
			char *p=eim->GetCandWord(-1);
			if(p)
			{
				char temp[2]={key,0};
				y_xim_send_string2(s2t_conv(p),0);
				y_xim_send_string2(temp,SEND_BIAODIAN|SEND_FLUSH);
				YongResetIM();
				return 1;
			}
		}
	}
	return 0;
}

static void YongResetIM_(void)
{
	EXTRA_IM *eim;
	im.StringGetEngine[0]=0;
	eim=CURRENT_EIM();
	im.BihuaMode=0;
	im.EnglishMode=0;
	im.ChinglishMode=0;
	im.StopInput=0;
	im.CodeLen=0;
	im.CodeInput[0]=im.CodeInput[1]=0;
	im.StringGet[0]=im.StringGet[1]=0;
	im.InAssoc=0;
	if(eim)
	{
		eim->WorkMode=EIM_WM_NORMAL;
		eim->CaretPos=-1;
		eim->Reset();
	}
	if(eim!=im.eim && (eim=im.eim)!=NULL)
	{
		eim->WorkMode=EIM_WM_NORMAL;
		eim->CaretPos=-1;
		eim->Reset();
	}
}

void YongResetIM(void)
{
	YongResetIM_();
	YongShowInput(0);
	y_xim_preedit_clear();
}

void YongUpdateMain(CONNECT_ID *id)
{
	char *name;
	
	name=y_im_get_im_name(im.Index);
	if(name)
	{
		y_ui_tray_tooltip(name);
		y_ui_button_label(UI_BTN_NAME,s2t_conv(name));
		l_free(name);
	}
	
	if(!id) id=y_xim_get_connect();
	if(!id) return;
	if(id->state==0) return;

	y_ui_button_show(UI_BTN_CN+id->lang,1);
	y_ui_button_show(UI_BTN_CN+!id->lang,0);

	y_ui_button_show(UI_BTN_QUAN+id->corner,1);
	y_ui_button_show(UI_BTN_QUAN+!id->corner,0);

	y_ui_button_show(UI_BTN_CN_BIAODIAN+id->biaodian,1);
	y_ui_button_show(UI_BTN_CN_BIAODIAN+!id->biaodian,0);
	
	y_ui_button_show(UI_BTN_SIMP+id->trad,1);
	y_ui_button_show(UI_BTN_SIMP+!id->trad,0);

	if(InputNoShow==2)
		YongShowInput(1);
}

void YongReloadAll(void)
{
	YongResetIM();
	y_xim_update_config();
	y_im_update_main_config();
	update_config_skin();
	update_main_window();
	update_input_window();
	update_tray_icon();
	y_im_load_urls();
	y_im_load_book();
	update_im();
	y_ui_update_menu();
	YongUpdateMain(0);
	/* redo it for something new from config file */
	YongResetIM();
}

void YongDestroyIM(void)
{
	if(im.eim)
	{
		im.eim->Destroy();
		im.eim=NULL;
		y_im_module_close(im.handle);
		im.handle=0;
	}
	y_im_history_free();
#ifndef CFG_NO_KEYTOOL
	y_key_tools_free(key_tools);
	key_tools=NULL;
#endif
}

#ifdef __WIN32
#ifndef ATTACH_PARENT_PROCESS
#define ATTACH_PARENT_PROCESS   ((DWORD)-1)
#endif
/*
static void attach_console(void)
{
	if (fileno (stdout) != -1 &&
		_get_osfhandle (fileno (stdout)) != -1)
	{
	}
	else
	{
		typedef BOOL (* WINAPI AttachConsole_t) (DWORD);

		AttachConsole_t p_AttachConsole =
			(AttachConsole_t) GetProcAddress (GetModuleHandle (_T("kernel32.dll")),
							  "AttachConsole");

		if (p_AttachConsole != NULL && p_AttachConsole (ATTACH_PARENT_PROCESS))
		{
			freopen ("CONOUT$", "w", stdout);
			dup2 (fileno (stdout), 1);
			freopen ("CONOUT$", "w", stderr);
			dup2 (fileno (stderr), 2);
		}
	}
}*/
#endif

#ifndef CFG_BUILD_LIB

static void signal_handler(int sig)
{
	//printf("signal %d\n",sig);
	switch(sig)
	{
#ifdef SIGHUP
		case SIGHUP:
#endif
		case SIGINT:
#ifdef SIGPIPE
		case SIGPIPE:
#endif
		case SIGTERM:
			YongDestroyIM();
			break;
	}
	_exit(0);
}

int main(int arc,char *arg[])
{
	int mb_tool=0;
	int i;
	char *xim=0;
	
#ifdef _WIN32
	//attach_console();
#endif
	
	VERBOSE("deal command line\n");

	for(i=0;i<arc;i++)
	{
		if(!strcmp(arg[i],"-d"))
		{
#ifndef _WIN32
			int id=fork();
			if(id==-1)
				return EXIT_FAILURE;
			else if(id>0)
				return EXIT_SUCCESS;
			else break;
#endif
		}
		else if(!strcmp(arg[i],"-s"))
		{
			y_im_setup_config();
			return EXIT_SUCCESS;
		}
		else if(!strcmp(arg[i],"--ybus"))
		{
			xim="ybus";
		}
#ifdef CFG_XIM_IBUS
		else if(!strcmp(arg[i],"--ibus"))
		{
			xim="ibus";
		}
		else if(!strcmp(arg[i],"--xml"))
		{
			extern void xim_ibus_output_xml(void);
			y_ui_init(xim);
			y_im_config_path();
			y_im_update_main_config();
			xim_ibus_output_xml();
			exit(0);
		}
		else if(!strcmp(arg[i],"--ibus-menu"))
		{
			extern void xim_ibus_menu_enable(int enable);
			xim_ibus_menu_enable(1);
		}
#endif
		else if(!strcmp(arg[i],"--tool=libmb.so"))
		{
			mb_tool=1;
			arc-=i+1;
			arg+=i+1;
		}
#ifdef _WIN32
		else if(!strcmp(arg[i],"-exec") && i+2==arc)
		{
			//MessageBoxA(0,arg[i+1],0,0);
			int y_im_is_url(const char *s);
			int show=SW_SHOWNORMAL;
			char cmdline[256];
			char *p=cmdline;
			snprintf(cmdline,256,"%s",arg[i+1]);
			y_im_expand_env(p);
			if(y_im_is_url(p))
			{
				ShellExecuteA(NULL,"open",p,NULL,NULL,show);
			}
			else if(l_file_exists(p))
			{
				int len=strlen(p);
				if(len>4 && !strcmp(p+len-4,".bat"))
					show=SW_HIDE;
				ShellExecuteA(NULL,"open",p,NULL,NULL,show);
			}			
			else
			{
				char *cmd,*param;
				if(p[0]=='"')
				{
					param=strchr(p+1,'"');
					if(!param) return 0;
					cmd=l_strndup(p+1,param-p-1);
					param++;
					if(*param==' ') param++;
					else param=0;
				}
				else
				{
					param=strchr(p+1,' ');
					if(!param)
					{
						cmd=p;
					}
					else
					{
						cmd=l_strndup(p,param-p);
						param++;
					}
				}
				
				SHELLEXECUTEINFOA info;
				memset(&info,0,sizeof(info));
				info.cbSize=sizeof(info);
				info.lpFile=cmd;
				info.lpParameters=param;
				info.nShow=SW_SHOWNORMAL;
				info.hwnd=y_ui_main_win();
				info.lpVerb="open";
#ifdef _WIN64
				info.lpDirectory="..";
#endif
				if(strstr(cmd,"yong-config") && strstr(param,"--update"))
				{
					static const char *sys="C:\\Program Files";
					char path[MAX_PATH];
					OSVERSIONINFO ovi={.dwOSVersionInfoSize=sizeof(ovi)};
					GetVersionEx(&ovi);
					GetCurrentDirectoryA(sizeof(path),path);
					int uac=!strncasecmp(sys,path,strlen(sys));
					if(ovi.dwMajorVersion>=6 && uac!=0)
					{
						info.lpVerb="runas";
					}
					info.lpDirectory=NULL;
				}
				ShellExecuteExA(&info);
				//ShellExecuteA(NULL,"open",cmd,param,NULL,show);
			}			
			return 0;
		}
#endif
#if 0
		else if(arc==3 && i==1 &&
				arg[i][0]=='-' && arg[i][1]=='M' && arg[i][2]=='a' && arg[i][3]=='c')
		{
			extern int y_im_mac_force;
			y_im_mac_force=atoi(arg[2]);
		}
#endif
	}
	
	VERBOSE("ui init\n");

	//clock_t start=clock();
#ifdef CFG_XIM_FBTERM
	if(getenv("FBTERM_IM_SOCKET"))
		xim="fbterm";
#endif
	y_ui_init(xim);
	
	VERBOSE("config path\n");

	y_im_config_path();

	if(mb_tool)
	{
		int (*tool_main)(int arc,char **arg);
		void *handle;
		int ret;
		handle=y_im_module_open("libmb.so");
		if(!handle)
		{
			return EXIT_FAILURE;
		}
		tool_main=(void*)y_im_module_symbol(handle,"tool_main");
		if(!tool_main)
		{
			y_im_module_close(handle);
			return EXIT_FAILURE;
		}
		EXTRA_IM *eim=y_im_module_symbol(handle,"EIM");
		eim->OpenFile=(void*)y_im_open_file;
		ret=tool_main(arc,arg);
		y_im_module_close(handle);
		return ret;
	}
	
	VERBOSE("update main config\n");

	y_im_update_main_config();
	update_config_translate();
	update_config_skin();
	update_main_window();
	update_input_window();
	y_im_load_urls();
	y_im_load_book();
	y_im_history_init();
	
	if(!xim || strcmp(xim,"fbterm"))
	{
		VERBOSE("load dict\n");
		local_dic=y_dict_open("dict.txt");
		y_dict_query_and_show(local_dic,0);
	}

	VERBOSE("load soft keyboard\n");
	y_kbd_init("keyboard.ini");
	im.Index=y_im_get_config_int("IM","default");
	
	/* xim init should before update_im, but may have other problem */
	VERBOSE("init xim\n");
	if(y_xim_init(xim))
		return -1;

	VERBOSE("update im\n");
	update_im();
	VERBOSE("update tray\n");
	update_tray_icon();
	VERBOSE("update menu\n");
	y_ui_update_menu();

	atexit(YongDestroyIM);
	
#ifdef SIGHUP
	signal(SIGHUP,signal_handler);
#endif
	signal(SIGINT,signal_handler);
	signal(SIGTERM,signal_handler);
#ifdef SIGPIPE
	signal(SIGPIPE,signal_handler);
#endif

	if(y_im_get_config_int("IM","enable"))
		y_xim_enable(1);

	//fprintf(stderr,"load in %.2f seconds\n",((double)(clock()-start))/(double)CLOCKS_PER_SEC);
	VERBOSE("init done\n");

	y_ui_loop();
	return 0;
}

#else

int y_main_init(int index)
{
	y_ui_init(NULL);
	y_im_config_path();
	y_im_update_main_config();
	tip_main=y_im_get_config_int("main","tip");
	InputNoShow=y_im_get_config_int("input","noshow");
	y_im_load_urls();
	y_im_load_book();
	y_xim_init(NULL);
	if(index>=0)
		im.Index=index;
	else
		im.Index=y_im_get_config_int("IM","default");
	update_im();
	return 0;
}

void y_main_clean(void)
{
	YongDestroyIM();
	y_ui_clean();
#ifndef CFG_NO_REPLACE
	y_replace_free();
#endif
	y_im_free_urls();
	y_im_free_book();
	y_im_free_config();
}

#endif
