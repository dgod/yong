#include "common.h"
#include "yong.h"
#include "xim.h"
#include "gbk.h"
#include "im.h"
#include "ui.h"
#include "llib.h"
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>
#include "translate.h"
#include "version.h"

#ifdef CFG_XIM_FBTERM
#include "fbterm.h"
#endif
#include "ybus.h"

//#define memcpy(a,b,c) memmove(a,b,c)

static LKeyFile *MainConfig,*SubConfig,*MenuConfig;

static Y_XIM xim;

int y_xim_init(const char *name)
{
#ifdef __linux__
	if(name && !strcmp(name,"ybus"))
		name=NULL;
#endif
	memset(&xim,0,sizeof(xim));
	if(!name || !name[0])
	{
		int y_xim_init_default(Y_XIM *x);
		y_xim_init_default(&xim);
	}
#ifdef CFG_XIM_FBTERM
	else if(!strcmp(name,"fbterm"))
	{
		int y_xim_init_fbterm(Y_XIM *x);
		y_xim_init_fbterm(&xim);
	}
#endif
	if(!xim.init)
		return -1;
	else
		return xim.init();
}

char *y_xim_get_name(void)
{
	return xim.name?xim.name:"";
}

void y_xim_forward_key(int key,int repeat)
{
	if(xim.forward_key)
		xim.forward_key(key,repeat);
}

void y_xim_update_config(void)
{
	if(xim.update_config)
		xim.update_config();
}

static void send_raw_string_async(const char *text,void *unused)
{
	if(!text || !text[0])
		return;
	if(strlen(text)>MAX_CAND_LEN || strchr(text,'\n'))
		YongSendClipboard(text);
	else
		xim.send_string(text,DONT_ESCAPE);
}

static char **y_im_parse_argv(const char *s,int size);

static void async_spawn_at_idle(char *s)
{
#if defined(CFG_XIM_WEBIM)
	// TODO:
#elif defined(CFG_XIM_ANDROID)
	y_im_expand_space(s);
	xim.explore_url(s);
	l_free(s);
#else
	char **argv=y_im_parse_argv(s+1,-1);
	l_free(s);
	if(!argv)
		return;
	if(!argv[0])
	{
		l_strfreev(argv);
		return;
	}
#ifdef _WIN64
	if(!l_file_exists(argv[0]))
	{
		char temp[256];
		sprintf(temp,"%s/%s",y_im_get_path("DATA"),argv[0]);
		if(l_file_exists(temp))
		{
			if(l_fullpath(temp,temp,sizeof(temp)))
			{
				l_free(argv[0]);
				argv[0]=l_strdup(temp);
			}
		}
	}
#endif
	y_im_async_spawn(argv,send_raw_string_async,NULL);
	l_strfreev(argv);
#endif
	return;
}

void y_xim_explore_url(const char *s)
{
	if(s[0]=='|')
	{
		y_ui_idle_add((void*)async_spawn_at_idle,l_strdup(s));
		return;
	}
	if(xim.explore_url)
		xim.explore_url(s);
}

static char last_output[512];
static char temp_output[512];
static char last_biaodian[12];
static char last_biaodian_only;
static char last_code[64];
static int64_t last_output_time;

void y_xim_set_last(const char *s)
{
	if(!s || !s[0])
	{
		last_output[0]=0;
		last_biaodian[0]=0;
		last_biaodian_only=0;
		return;
	}
	if(!strcmp(s,"$LAST"))
		return;
	strcpy(last_output,s);
	last_biaodian[0]=0;
	last_biaodian_only=0;
}

const char *y_xim_get_last(void)
{
	return last_output;
}

int y_xim_trigger_key(int key)
{
	if(xim.trigger_key)
		return xim.trigger_key(key);
	return -1;
}

static int encrypt_clipboard_cb(const char *s)
{
	char temp[128];
	if(!s)
	{
		y_ui_show_tip(YT("没有数据需要加密"));
		return 0;
	}
	if(0!=y_im_book_encrypt(s,temp))
	{
		y_ui_show_tip(YT("加密数据失败"));
		return 0;
	}
	y_xim_send_string2(temp,SEND_FLUSH);
	return 0;
}

static void y_im_strip_key_useless(char *gb)
{
	char *s;
	int key=0;
	s=strrchr(gb,'$');
	if(s)
	{
		key=y_im_str_to_key(s+1,NULL);
		if(key>0) *s=0;
	}
}

static void escape_last(const char *s,char *out)
{
	char c;
	while((c=*s++)!=0)
	{
		if(c=='$')
		{
			if(s[0]=='$')
			{
				*out++=c;
				*out++=c;
				s++;
			}
			else if(!strncmp(s,"LAST",4))
			{
				s+=4;
				strcpy(out,last_output);
				out+=strlen(last_output);
			}
			else
			{
				*out++=c;
			}
		}
		else
		{
			*out++=c;
		}
	}
	*out=0;
}

static void y_im_input_key_at_idle(int *keys)
{
	if(!keys)
		return;
	for(int i=0;keys[i]!=0;i++)
		y_im_input_key(keys[i]);
	l_free(keys);
}

void y_xim_send_keys(const char *s)
{
	if(!xim.send_keys)
		return;
	int keys[256];
	int count=y_im_parse_keys(s,keys,256);
	if(count<=0)
		y_ui_show_tip(YT("格式不正确"));
	else
		xim.send_keys(keys,count);
}

void y_xim_send_string2(const char *s,int flag)
{
	int key=0;
	if(s)
	{
		if(flag&SEND_RAW)
			goto COPY;
		s+=y_im_str_desc(s,NULL);
		if(strstr(s,"$LAST"))
		{
			escape_last(s,temp_output);
		}
		else if(s[0]=='$')
		{
			if(l_str_has_suffix(s,"$SPACE"))
			{
				int len=strlen(s);
				if(len>6)
					s=l_strndupa(s,len-6);
			}
			if(!strcmp(s,"$LAST"))
			{
				strcat(temp_output,last_output);
			}
			else if(!strncmp(s,"$GO(",4) && l_str_has_suffix(s,")"))
			{
				y_xim_send_string2("",SEND_FLUSH|SEND_GO);
				y_im_go_url(s);
				return;
			}
			else if(!strncmp(s,"$KEY(",5) && l_str_has_suffix(s,")"))
			{
				char temp[256];
				strcpy(temp,s+5);
				temp[strlen(temp)-1]=0;
				y_im_book_key(temp);
				y_ui_show_tip(YT("设置密钥完成"));
				return;
			}
			else if(!strncmp(s,"$DECRYPT(",9) && l_str_has_suffix(s,")"))
			{
				char temp[256];
				int len;
				len=strlen(temp_output);
				if(len+65>sizeof(temp_output))
					return;
				strcpy(temp,s+9);
				temp[strlen(temp)-1]=0;
				if(0!=y_im_book_decrypt(temp,temp_output+len))
				{
					y_ui_show_tip(YT("解密数据失败"));
					return;
				}
			}
			else if(!strncmp(s,"$ENCRYPT(",9) && l_str_has_suffix(s,")"))
			{
				char temp[256];
				int len;
				len=strlen(temp_output);
				if(len+128>sizeof(temp_output))
					return;
				strcpy(temp,s+9);
				temp[strlen(temp)-1]=0;
				if(temp[0]!=0)
				{
					if(0!=y_im_book_encrypt(temp,temp_output+len))
					{
						y_ui_show_tip(YT("加密数据失败"));
						return;
					}
				}
				else
				{
					y_ui_get_select(encrypt_clipboard_cb);
				}
			}
			else if(s[1]=='B' && s[2]=='D' && s[3]=='(' && s[4] && s[5]==')' && !s[6])
			{
				const char *temp;
				int lang=LANG_CN;
				CONNECT_ID *id=y_xim_get_connect();
				if(id) lang=id->biaodian;
				temp=YongGetPunc(s[4],lang,0);
				if(temp)
				{
					strcat(temp_output,temp);
				}
				else if(isgraph(s[4]))
				{
					char temp[2]={s[4],0};
					strcat(temp_output,temp);
				}	
			}
			else if(!strcmp(s,"$RELOAD()"))
			{
				y_ui_idle_add((void*)YongReloadAllTip,NULL);
			}
			else if(!strncmp(s,"$KEYBOARD(",10) && l_str_has_suffix(s,")"))
			{
#if !defined(CFG_XIM_ANDROID) && !defined(__EMSCRIPTEN__)
				int pos=0,sub=0;
				sscanf(s+10,"%d,%d",&pos,&sub);
				y_kbd_select(pos,sub);
#endif	
			}
			else if(y_im_forward_key(s)==0)
			{
				// 当前是个模拟按键，后续应该有标点等文字信息
				// 但操作系统那里按键处理可能慢于文字信息处理
				// 用户得不到正确的输入顺序
				goto out;
			}
			else if(l_str_has_surround(s,"$IMKEY(",")"))
			{
				char *temp=l_strndupa(s+7,strlen(s+7)-1);
				int *keys=y_im_str_to_keys(temp);
				y_ui_idle_add((void*)y_im_input_key_at_idle,keys);
			}
			else if(l_str_has_surround(s,"$SENDKEYS(",")"))
			{
				if(xim.send_keys)
				{
					char *temp=l_strndupa(s+10,strlen(s+10)-1);
					int keys[256];
					int count=y_im_parse_keys(temp,keys,256);
					if(count<=0)
						y_ui_show_tip(YT("格式不正确"));
					else
						xim.send_keys(keys,count);
				}
				else
				{
					y_ui_show_tip(YT("不支持此功能"));
				}
			}
			else
			{
				goto COPY;
			}
		}
		else if(s[0])
		{
			if(!(flag&SEND_RAW))
			{
				y_im_strip_key_useless(temp_output);
			}
COPY:
			if(!(flag&SEND_BIAODIAN))
			{
				strcat(temp_output,s);
				strcpy(last_output,temp_output);
				last_biaodian[0]=0;
				last_biaodian_only=0;
			}
			else
			{
				if(temp_output[0]==0)
					last_biaodian_only=1;
				strcat(temp_output,s);
				snprintf(last_biaodian,sizeof(last_biaodian),"%s",s);
			}
		}
	}
	else
	{
		if(strlen(temp_output)+strlen(last_output)<512)
			strcat(temp_output,last_output);
	}
	if(!(flag&SEND_FLUSH))
	{
		return;
	}

	s=temp_output;
	if(!s[0])
	{
		if(!(flag&SEND_GO))
		{
			last_output[0]=0;
			last_biaodian[0]=0;
		}
		return;
	}

	if(!(flag&SEND_RAW))
	{
		s+=y_im_str_desc(s,NULL);
		if(!y_im_forward_key(s))
			goto out;
		key=y_im_strip_key((char*)s);
		if(!y_im_go_url(s))
			goto out;
		if(!y_im_send_file(s))
			goto out;
#ifndef CFG_XIM_ANDROID
		if(strstr(s,"$/"))
		{
			YongSendClipboard(s);
			goto out;
		}
#endif
	}

	y_im_speed_update(0,s);
	y_im_history_write(s);
	if(xim.send_string)
	{
		if(s[0]=='$')
		{
#if defined(_WIN32) && !defined(_WIN64)
			last_output_time=_time64(NULL);
#else
			last_output_time=time(NULL);
#endif
		}
#ifndef CFG_NO_REPLACE
		if(key>0)
			xim.send_string(s,(flag&SEND_RAW)?DONT_ESCAPE:0);
		else
			y_replace_string(s,xim.send_string,(flag&SEND_RAW)?DONT_ESCAPE:0);
#else
		xim.send_string(s,(flag&SEND_RAW)?DONT_ESCAPE:0);
#endif
	}
	if(key>0)
	{
#ifdef WIN32
		y_xim_forward_key(YK_LEFT,key);
#else
		int i;
		for(i=0;i<key;i++)
			y_xim_forward_key(YK_LEFT,1);
#endif
	}
out:
	temp_output[0]=0;
}

void y_im_set_last_code(const char *s,const char *cand)
{
	size_t len=strlen(s);
	if(len>=sizeof(last_code))
		return;
	if(cand)
	{
		cand+=y_im_str_desc(cand,NULL);
		// 上次选择错误，但编码可能要被复用
		if(!strcmp(cand,"$GO(action:undo)"))
			return;
		if(!strcmp(cand,"$BACKSPACE(LAST)"))
			return;
		// 如果之前是发送repeat_code按键，则可能形成死循环
		if(l_str_has_surround(cand,"$IMKEY(",")"))
			return;
	}
	strcpy(last_code,s);
}

const char *y_im_get_last_code(void)
{
	return last_code;
}

void y_im_repeat_last_code(void *unused)
{
	(void)unused;
	for(int i=0;last_code[i]!=0;i++)
	{
		y_im_input_key(last_code[i]);
	}
}

void y_xim_send_string(const char *s)
{
	y_xim_send_string2(s,SEND_FLUSH);
}

int y_im_get_real_cand(const char *s,char *out,size_t size)
{
	if(!s || !s[0])
	{
		out[0]=0;
		return 0;
	}
	if(!strcmp(s,"$LAST"))
	{
		s=last_output;
		s+=y_im_str_desc(s,NULL);
		snprintf(out,size,"%s",s);
	}
	else
	{
		s+=y_im_str_desc(s,NULL);
		snprintf(out,size,"%s",s);
	}
	if(!out[0])
	{
		return 0;
	}

	if(s[0]=='$' || s[0]=='\0')
		return 0;

	y_im_strip_key(out);
	return strlen(out);
}

static inline int is_mask_key(int k)
{
	return k==YK_LCTRL || k==YK_RCTRL ||
		k==YK_LSHIFT || k==YK_RSHIFT ||
		k==YK_LALT || k==YK_RALT;
}

static time_t _idle_timer_add;
static void _idle_timer_cb(void *unused)
{
	_idle_timer_add=0;
	if(!im.eim || !im.eim->Call)
		return;
	im.eim->Call(EIM_CALL_IDLE);
	y_im_history_flush();
}
static void y_im_busy_mark(void)
{
	if(!im.eim)
		return;
	time_t now=time(NULL);
	if(_idle_timer_add)
	{
		if(now-_idle_timer_add<=10)
			return;
		y_ui_timer_del(_idle_timer_cb,NULL);
	}
	if(im.eim->CodeLen || im.eim->StringGet[0])
	{
		_idle_timer_add=0;
		return;
	}
	_idle_timer_add=now;
	y_ui_timer_add(20000,_idle_timer_cb,NULL);
}

int y_im_input_key(int key)
{
	int ret;
	int bing=key&KEYM_BING;
	int mod=key&KEYM_MASK;
	key&=~KEYM_CAPS;

	ret=YongHotKey(key);
	if(ret)
	{
		if(is_mask_key(YK_CODE(key)))
			return 0;
		return ret;
	}
	ret=YongKeyInput(key,mod);
	if(ret)
	{
		y_im_speed_update(key,0);
		if(bing && im.Bing && !im.EnglishMode)
			YongKeyInput(KEYM_BING|'+',0);
		y_im_busy_mark();
		if(is_mask_key(YK_CODE(key)))
			return 0;
	}
	return ret;
}

int y_xim_input_key(int key)
{
	if(!xim.input_key)
		return y_im_input_key(key);
	return xim.input_key(key);
}

CONNECT_ID *y_xim_get_connect(void)
{
	CONNECT_ID *id=0;
	if(xim.get_connect)
		id=xim.get_connect();
	return id;
}

void y_xim_put_connect(CONNECT_ID *id)
{
	if(xim.put_connect)
		xim.put_connect(id);
}

void y_xim_preedit_clear(void)
{
	if(xim.preedit_clear)
		xim.preedit_clear();
}

int y_im_last_key(int key)
{
	static int last;
	int ret;
	ret=last;
	last=key;
	return ret;
}

void y_xim_preedit_draw(char *s,int len)
{
	if(xim.preedit_draw)
		xim.preedit_draw(s,len);
}

void y_xim_enable(int enable)
{
	if(xim.enable)
		return xim.enable(enable);
}

Y_UI y_ui;
int y_ui_init(const char *name)
{
#ifdef CFG_XIM_FBTERM
	if(name && !strcmp(name,"fbterm"))
	{
		ui_setup_fbterm(&y_ui);
	}
	else
	{
		ui_setup_default(&y_ui);
	}
#else
	ui_setup_default(&y_ui);
#endif	
	return y_ui.init();
}

int y_im_copy_file(const char *src,const char *dst)
{
	int ret;
	FILE *fds,*fdd;
	char temp[1024];

	fds=y_im_open_file(src,"rb");
	if(fds==NULL)
	{
		return -1;
	}
	fdd=y_im_open_file(dst,"wb");
	if(fdd==NULL)
	{
		fclose(fds);
		return -1;
	}
	while(1)
	{
		ret=fread(temp,1,sizeof(temp),fds);
		if(ret<=0) break;
		fwrite(temp,1,ret,fdd);
	}
	fclose(fds);
	fclose(fdd);
	return 0;
}

int y_im_set_exec(void)
{
#ifndef _WIN32
	int ret;
	char *tmp;
	char data[256];
	ret=readlink("/proc/self/exe",data,256);		//linux
	if(ret<0)
		ret=readlink("/proc/curproc/file",data,256);//bsd
	if(ret<0||ret>=256)
	{
		strcpy(data,".");
		printf("yong: get self fail\n");
		return -1;
	}
	data[ret]=0;
	tmp=strrchr(data,'/');
	if(!tmp)
	{
		printf("yong: bad path\n");
		return -1;
	}
	*tmp=0;
	//printf("yong: change to dir %s\n",data);
	if(chdir(data))
	{
		printf("yong: chdir fail\n");
		return -1;
	}
#else
	wchar_t file[256],*tmp;
	int ret;
	ret=GetModuleFileName(NULL,file,256);
	if(ret<0 || ret>=256)
		return -1;
	file[ret]=0;
	tmp=wcsrchr(file,'\\');
	if(!tmp)
		return -1;
	*tmp=0;
	SetCurrentDirectory(file);
#endif
	return 0;
}

int y_im_mkdir(const char *dir)
{
	char temp[256];
	char *p,*pdir;

	pdir=l_strdup(dir);
	p=strrchr(pdir,'/');
	if(!p)
	{
		l_free(pdir);
		return 0;
	}
	*p=0;
	sprintf(temp,"%s/%s",y_im_get_path("HOME"),pdir);
	l_mkdir(temp,0700);
	l_free(pdir);
	return 0;
}

int y_im_config_path(void)
{
#if !defined(CFG_BUILD_LIB)
	y_im_set_exec();
#endif
	y_im_copy_config();
#ifdef _WIN32
	char temp[256];
	l_strcpy(temp,sizeof(temp),y_im_get_path("DATA"));
	l_str_replace(temp,'/','\\');
	l_setenv("_DATA",temp,1);
	l_strcpy(temp,sizeof(temp),y_im_get_path("HOME"));
	l_str_replace(temp,'/','\\');
	l_setenv("_HOME",temp,1);
#else
	l_setenv("_DATA",y_im_get_path("DATA"),1);
	l_setenv("_HOME",y_im_get_path("HOME"),1);
#endif
	return 0;
}

#if defined(CFG_XIM_ANDROID) || defined(CFG_XIM_NODEJS)
static void get_so_path(const char *file,char *out)
{
	FILE *fp;
	char line[1024];
	if(out!=NULL)
		strcpy(out,"/data/data/net.dgod.yong/lib");
	fp=fopen("/proc/self/maps","r");
	if(!fp)
		return;
	while(l_get_line(line,sizeof(line),fp)>=0)
	{
		char *p;
		if((p=strstr(line,file)))
		{
			if(p==line)
				break;
			p[-1]=0;
			p=strchr(line,'/');
			if(p==NULL)
				break;
			//YongLogWrite("%s\n",p);
			if(out!=NULL)
				strcpy(out,p);
			break;
		}
	}
	fclose(fp);
	
}
#endif

const char *y_im_get_path(const char *type)
{
	const char *ret;
#if defined(CFG_XIM_NODEJS)
	if(!strcmp(type,"LIB"))
	{
		static char lib_path[128];
		if(!lib_path[0])
		{
#ifdef OPENHARMONY
			get_so_path("libyong.so",lib_path);
#else
			get_so_path("yong.node",lib_path);
#endif
		}
		ret=lib_path;
	}
	else if(!strcmp(type,"HOME"))
	{
		ret=L_TO_STR(YONG_HOME_PATH);
		if(!l_file_exists(ret))
		{
			l_mkdir(ret,0700);
		}
	}
	else
	{
		ret=L_TO_STR(YONG_DATA_PATH);
	}
#elif defined(CFG_XIM_ANDROID)
	if(!strcmp(type,"LIB"))
	{
		static char lib_path[128];
		//ret="/data/data/net.dgod.yong/lib";
		if(!lib_path[0])
			get_so_path("libyong.so",lib_path);
		ret=lib_path;	
	}
	else if(!strcmp(type,"HOME"))
	{
		static char home_path[128];
		if(!home_path[0])
		{
			char *p=getenv("EXTERNAL_STORAGE");
			if(!p)
			{
				strcpy(home_path,"/sdcard/yong/.yong");
			}
			else
			{
				sprintf(home_path,"%s/yong/.yong",p);
			}
		}
		if(!l_file_exists(home_path))
		{
			int res=l_mkdir(home_path,0700);
		}
		ret=home_path;
	}
	else
	{
		static char data_path[128];
		if(!data_path[0])
		{
			char *p=getenv("EXTERNAL_STORAGE");
			if(!p)
			{
				strcpy(data_path,"/sdcard/yong");
			}
			else
			{
				sprintf(data_path,"%s/yong",p);
			}
		}
		ret=data_path;
	}
#elif defined(CFG_XIM_WEBIM)
	return "yong";
#elif defined(CFG_XIM_METRO)
	char sys[128];
	if(!SHGetSpecialFolderPathA(NULL,sys,CSIDL_PROGRAM_FILESX86,FALSE))
		SHGetSpecialFolderPathA(NULL,sys,CSIDL_PROGRAM_FILES,FALSE);
	if(!strcmp(type,"LIB"))
	{
		static char path[256];
#ifdef _WIN64
		sprintf(path,"%s\\yong\\w64",sys);
#else
		sprintf(path,"%s\\yong",sys);
#endif
		ret=path;
	}
	else if(!strcmp(type,"HOME"))
	{
		static char path[256];
		sprintf(path,"%s\\yong\\.yong",sys);
		ret=path;
	}
	else
	{
		static char path[256];
		sprintf(path,"%s\\yong",sys);
		ret=path;
	}
#elif !defined(_WIN32)
	if(!strcmp(type,"HOME"))
	{
		static char path[256];
		sprintf(path,"%s/.yong",getenv("HOME"));
		ret=path;
		if(!l_file_exists(ret))
			l_mkdir(ret,0700);
	}
	else
	{
		if(!strcmp(type,"LIB"))
			ret=".";
		else
			ret="..";
	}
#else
	if(!strcmp(type,"HOME"))
	{
		static int uac=-1;
		if(uac==-1)
		{
			static const char *sys="C:\\Program Files";
			if(sys)
			{
				char path[MAX_PATH];
				GetCurrentDirectoryA(sizeof(path),path);
				uac=!strncasecmp(sys,path,strlen(sys));
			}
			else
			{
				uac=0;
			}
		}
		if(uac==1)
		{
			static char path[256];
			if(path[0]==0)
			{
				wchar_t *data=_wgetenv(L"AppData");
				assert(data!=NULL);
				l_utf16_to_utf8(data,path,sizeof(path));
				strcat(path,"/yong");
			}
			ret=path;
		}
		else
		{
#ifdef _WIN64
			ret="../.yong";
#else
			ret="./.yong";
#endif
		}
		if(!l_file_exists(ret))
			l_mkdir(ret,0700);
	}
	else
	{
#ifdef _WIN64
		if(!strcmp(type,"LIB"))
			ret=".";
		else
			ret="..";
#else
		ret=".";
#endif
	}
#endif
	return ret;
}

static const struct{
	char *name;
	int key;
}str_key_map[]={
	{"NONE",0},
	{"LCTRL",YK_LCTRL},
	{"RCTRL",YK_RCTRL},
	{"LSHIFT",YK_LSHIFT},
	{"RSHIFT",YK_RSHIFT},
	{"LALT",YK_LALT},
	{"RALT",YK_RALT},
	{"LWIN",YK_LWIN},
	{"RWIN",YK_RWIN},
	{"TAB",YK_TAB},
	{"ESC",YK_ESC},
	{"ENTER",YK_ENTER},
	{"BACKSPACE",YK_BACKSPACE},
	{"SPACE",YK_SPACE},
	{"DEL",YK_DELETE},
	{"HOME",YK_HOME},
	{"LEFT",YK_LEFT},
	{"UP",YK_UP},
	{"DOWN",YK_DOWN},
	{"RIGHT",YK_RIGHT},
	{"PGUP",YK_PGUP},
	{"PGDN",YK_PGDN},
	{"PAGEUP",YK_PGUP},
	{"PAGEDOWN",YK_PGDN},
	{"END",YK_END},
	{"INSERT",YK_INSERT},
	{"CAPSLOCK",YK_CAPSLOCK},
	{"COMMA",','},
	{NULL,0}
};

int y_im_str_to_key(const char *s,int *repeat)
{
	char tmp[16];
	int i;
	const char *p=s;
	int key=0;

	if(!s || !s[0] || s[0]=='_')
		return -1;
	if(repeat)
		*repeat=0;
	if(s[0]=='0' && (s[1]=='x' || s[1]=='X'))
	{
		if(repeat)
			*repeat=1;
		return (int)strtol(s+2,NULL,16);
	}
	while(p[0])
	{
		for(i=0;i<15;i++)
		{
			tmp[i]=*p;
			if(!tmp[i] || tmp[i]==' ')
				break;
			p++;
			if(i>0 && tmp[i]=='_')
				break;
		}
		tmp[i]=0;
		if(i==0)
		{
			return -1;
		}
		else if(i==1)
		{
			if(isgraph(tmp[0]))
			{
				key|=tmp[0];
				if(p[0])
					return -1;
				break;
			}
			else
			{
				return -1;
			}
		}
		else if(!strcmp(tmp,"CTRL"))
		{
			if(key&KEYM_CTRL)
				return -1;
			key|=KEYM_CTRL;
		}
		else if(!strcmp(tmp,"SHIFT"))
		{
			if(key&KEYM_SHIFT)
				return -1;
			key|=KEYM_SHIFT;
		}
		else if(!strcmp(tmp,"ALT"))
		{
			if(key&KEYM_ALT)
				return -1;
			key|=KEYM_ALT;
		}
		else if(!strcmp(tmp,"WIN"))
		{
			if(key&KEYM_SUPER)
				return -1;
			key|=KEYM_SUPER;
		}
		else
		{
			for(i=0;str_key_map[i].name;i++)
			{
				if(!repeat)
				{
					if(!strcmp(str_key_map[i].name,tmp))
					{
						key|=str_key_map[i].key;
						break;
					}
				}
				else
				{
					if(l_str_has_prefix(tmp,str_key_map[i].name))
					{
						int k=strlen(str_key_map[i].name);
						if(tmp[k]==0)
						{
							key|=str_key_map[i].key;
							break;
						}
						int l=strlen(tmp);
						if(tmp[k]=='(' && tmp[l-1]==')')
						{
							if(!memcmp(tmp+k+1,"LAST",l-k-2))
							{
								const char *s=last_output;
								s+=y_im_str_desc(s,NULL);
								if(s[0]=='$')
									s=y_im_str_escape(s,0,last_output_time);
								*repeat=last_biaodian_only?0:gb_strlen((const uint8_t*)s);
								if(last_biaodian[0])
									*repeat+=gb_strlen((const uint8_t*)last_biaodian);
								if(*repeat==0)
									*repeat=1;
							}
							else
							{
								*repeat=atoi(tmp+k+1);
							}
							key|=str_key_map[i].key;
							break;
						}
					}
				}			
			}
			if(!str_key_map[i].name)
			{
				return -1;
			}
			if(p[0]==' ')
				break;
		}
	}
	if(key && repeat && *repeat==0)
		*repeat=1;
	return key;
}

static int key_from_config(const char *s)
{
	char key[64];
	int which=-1;
	sscanf(s,"%63s %d",key,&which);
	return y_im_get_key(key,which,0);
}

int *y_im_str_to_keys(const char *s)
{
	int keys[64];
	int count=0;
	const char *begin=s;
	while(count<L_ARRAY_SIZE(keys)-1)
	{
		int key;
		int repeat;
		const char *end=strchr(begin,',');
		if(!end)
		{
			key=y_im_str_to_key(begin,&repeat);
			if(key<=0)
			{
				key=key_from_config(begin);
				repeat=1;
			}
		}
		else
		{
			size_t size=(size_t)(end-begin);
			if(size>32)
				break;
			char temp[64];
			l_strncpy(temp,begin,size);
			key=y_im_str_to_key(temp,&repeat);
			if(key<=0)
			{
				key=key_from_config(temp);
				repeat=1;
			}
		}
		if(!key)
			break;
		while(count<L_ARRAY_SIZE(keys)-1 && repeat>0)
		{
			keys[count++]=key;
			repeat--;
		}
		if(!end)
			break;
		begin=end+1;
	}
	keys[count++]=0;
	return l_memdup(keys,sizeof(int)*count);
}

int y_im_get_key(const char *name,int pos,int def)
{
	int ret=-1;
	char *tmp;

	tmp=y_im_get_config_string("key",name);
	if(!tmp)
		return def;
	if(!tmp[0])
	{
		l_free(tmp);
		return def;
	}
	if(!strcmp(tmp,"CTRL"))
	{
		l_free(tmp);
		if(pos==-1)
			return KEYM_CTRL;
		tmp=l_strdup("LCTRL RCTRL");
	}
	else if(!strcmp(tmp,"SHIFT"))
	{
		l_free(tmp);
		if(pos==-1)
			return KEYM_SHIFT;
		tmp=l_strdup("LSHIFT RSHIFT");
	}
	if(pos==-1)
	{
		ret=y_im_str_to_key(tmp,NULL);
	}
	else
	{
		char **list;
		int i;
		list=l_strsplit(tmp,' ');
		for(i=0;i<pos && list[i];i++);
		if(i==pos && list[i])
			ret=y_im_str_to_key(list[i],NULL);
		l_strfreev(list);
	}
	if(ret<0) ret=def;
	l_free(tmp);
	return ret;
}

int y_im_key_eq(int k1,int k2)
{
	k1&=~KEYM_CAPS;
	k2&=~KEYM_CAPS;
	if(k1==k2)
		return 1;
	if(k1==KEYM_SHIFT && (k2==YK_LSHIFT || k2==YK_RSHIFT))
		return 1;
	if(k2==KEYM_SHIFT && (k1==YK_LSHIFT || k1==YK_RSHIFT))
		return 1;
	if(k1==KEYM_CTRL && (k2==YK_LCTRL || k2==YK_RCTRL))
		return 1;
	if(k2==KEYM_CTRL && (k1==YK_LCTRL || k1==YK_RCTRL))
		return 1;
	return 0;
}

int y_im_parse_keys(const char *s,int *out,int size)
{
	int i,c;
	int surround=0;
	char esc[16];
	int esc_len=-1;
	int mask=0;
	int count=0;
	char temp[strlen(s)+1];

	y_im_expand_with(s,temp,sizeof(temp),EXPAND_SPACE);
	s=temp;
	for(i=0;(c=s[i])!='\0';i++)
	{
		int key;
		if(count>=size-6)
			return -1;
		switch(c){
			case '+':
				if(esc_len==-1 && surround==0)
				{
					key=KEYM_SHIFT;
					mask|=key;
					out[count++]=key;
				}
				break;
			case '%':
				if(esc_len==-1 && surround==0)
				{
					key=KEYM_ALT;
					mask|=key;
					out[count++]=key;
				}
				break;
			case '^':
				if(esc_len==-1 && surround==0)
				{				
					key=KEYM_CTRL;
					mask|=key;
					out[count++]=key;
				}
				break;
			case '#':
				if(esc_len==-1 && surround==0)
				{				
					key=KEYM_WIN;
					mask|=key;
					out[count++]=key;
				}
				break;
			case '(':
				if(surround)
					return -2;
				surround=1;
				break;
			case ')':
				surround=0;
				goto unmask;
				break;
			case '{':
				esc_len=0;
				break;
			case '}':
				if(esc_len<0)
					return -3;
				if(esc_len==0)
				{
					if(s[i+1]!='}')
					{
						esc_len=-1;
						goto unmask;
					}
					esc[esc_len++]='}';
					break;
				}
				else
				{
					esc[esc_len]=0;
					if(esc_len==1)
					{
						key=esc[0];
						out[count++]=key;
					}
					else
					{
						char str[10];
						int repeat=1;
						int ret;
						if(esc[0]==' '&&esc[1]==' ')
						{
							str[0]=' ';
							str[1]=0;
							ret=2;
							repeat=atoi(esc+2);
						}
						else
						{
							ret=sscanf(esc,"%9s %d",str,&repeat);
						}
						if(ret<1 || repeat<=0)
							return -5;
						if(strlen(str)==1)
						{
							key=str[0];
							if(!mask)
								key|=KEYM_VIRT;
							out[count++]=key;
						}
						else if(!mask && (str[0]&0x80))
						{
							key=l_gb_to_unichar((const uint8_t*)str);
							out[count++]=key|KEYM_VIRT;
						}
						else if(!strcmp(str,"DELAY"))
						{
							if(repeat>5000)
								return -6;
							out[count++]=KEYM_UP|repeat;
							repeat=0;
							esc_len=-1;
							// don't goto unmask
							break;
						}
						else if(!strcmp(str,"CLICK"))
						{
							if(repeat<=0 || repeat>7)
								return -6;
							out[count++]=KEYM_CAPS|(1<<16)|repeat;
							repeat=0;
						}
						else
						{
							int vk;
							struct{
								const char *str;
								int key;
							}keymap[]={
								{"BACKSPACE",YK_BACKSPACE},
								{"BS",YK_BACKSPACE},
								{"BKSP",YK_BACKSPACE},
								{"CAPSLOCK",YK_CAPSLOCK},
								{"DELETE",YK_DELETE},
								{"DEL",YK_DELETE},
								{"DOWN",YK_DOWN},
								{"END",YK_END},
								{"ENTER",YK_ENTER},
								{"ESC",YK_ESC},
								{"HOME",YK_HOME},
								{"INSERT",YK_INSERT},
								{"INS",YK_INSERT},
								{"LEFT",YK_LEFT},
								{"PGDN",YK_PGDN},
								{"PGUP",YK_PGUP},
								{"RIGHT",YK_RIGHT},
								{"TAB",YK_TAB},
								{"SPACE",YK_SPACE},
								{"UP",YK_UP},
								{"F1",YK_F1},
								{"F2",YK_F2},
								{"F3",YK_F3},
								{"F4",YK_F4},
								{"F5",YK_F5},
								{"F6",YK_F6},
								{"F7",YK_F7},
								{"F8",YK_F8},
								{"F9",YK_F9},
								{"F10",YK_F10},
								{"F11",YK_F11},
								{"F12",YK_F12},
							};
							int i;
							for(i=0;i<L_ARRAY_SIZE(keymap);i++)
							{
								if(!strcmp(keymap[i].str,str))
								{
									vk=keymap[i].key;
									break;
								}
							}
							if(i==L_ARRAY_SIZE(keymap))
								return -7;
							out[count++]=vk;
						}
						if(repeat>1)
						{
							if(repeat>32)
								return -8;
							out[count++]=KEYM_BING|(repeat-1);
						}
					}
					esc_len=-1;
					if(!surround)
						goto unmask;
					break;
				}
			default:
				if(esc_len>=0)
				{
					if(esc_len>=sizeof(esc)-1)
						return -4;
					esc[esc_len++]=c;
					break;
				}
				if(c=='~')
				{
					out[count++]=YK_ENTER;
					goto unmask;
				}
				else
				{
					if((c&0x80)!=0)
					{
						c=l_gb_to_unichar((const uint8_t*)s+i);
						i++;
					}
					key=c;
				}
				if(!mask)
					key|=KEYM_VIRT;
				out[count++]=key;
unmask:
				if(mask&KEYM_CTRL)
				{
					out[count++]=KEYM_CTRL|KEYM_UP;
				}
				if(mask&KEYM_ALT)
				{
					out[count++]=KEYM_ALT|KEYM_UP;
				}
				if(mask&KEYM_SHIFT)
				{
					out[count++]=KEYM_SHIFT|KEYM_UP;
				}
				if(mask&KEYM_WIN)
				{
					out[count++]=KEYM_WIN|KEYM_UP;
				}
				mask=0;
				break;
		}
	}
	if(mask)
	{
		i--;
		goto unmask;
	}
	if(esc_len>=0)
		return -9;
	if(surround)
		return -10;
	return count;
}

static char **y_im_parse_argv(const char *s,int size)
{
	if(size<=0)
		size=strlen(s);
	char temp[256];
	int len=0;
	int in_str=0;
	LPtrArray *arr=l_ptr_array_new(8);
	for(int i=0;i<size;i++)
	{
		int c=s[i];
		if(!in_str && c==' ')
		{
			temp[len]=0;
			l_ptr_array_append(arr,l_strdup(temp));
			len=0;
			continue;
		}
		if(len==0 && !in_str && c=='"')
		{
			in_str=1;
			continue;
		}
		if(in_str && c=='"')
		{
			in_str=0;
			continue;
		}
		if(c=='\\')
		{
			if(s[i+1]=='\\')
			{
				i++;
			}
			else if(s[i+1]=='"')
			{
				c='"';
				i++;
			}
		}
		temp[len++]=c;
	}
	if(len>0)
	{
		temp[len]=0;
		l_ptr_array_append(arr,l_strdup(temp));
	}
	l_ptr_array_append(arr,NULL);
	if(l_ptr_array_length(arr)>0)
	{
		char *first=l_ptr_array_nth(arr,0);
		if(l_str_has_suffix(first,".js"))
		{
#ifdef _WIN32
			bool cscript=false;
#endif
			if(!l_file_exists(first))
			{
				char temp[256];
				sprintf(temp,"%s/%s",y_im_get_path("HOME"),first);
				if(!l_file_exists(temp))
				{
					sprintf(temp,"%s/%s",y_im_get_path("DATA"),first);
					if(!l_file_exists(temp))
					{
						l_ptr_array_free(arr,NULL);
						return NULL;
					}
				}
#ifdef _WIN32
				char *script=l_file_get_contents(temp,NULL,NULL);
				if(script)
				{
					if(strstr(script,"WScript."))
						cscript=true;
					l_free(script);
				}
#endif
#ifdef _WIN64
				if(l_str_has_prefix(temp,"../"))
					memmove(temp,temp+3,strlen(temp+3)+1);
#endif
				l_ptr_array_nth(arr,0)=l_strdup(temp);
			}
#ifdef _WIN32
			if(!cscript)
			{
				l_ptr_array_insert(arr,0,l_strdup("node"));
			}
			else
			{
				l_ptr_array_insert(arr,0,l_strdup("//Nologo"));
				l_ptr_array_insert(arr,0,l_strdup("cscript.exe"));
			}
#else
			l_ptr_array_insert(arr,0,l_strdup("node"));
#endif
		}
	}
	char **res=(char**)arr->data;
	for(int i=0;res[i]!=NULL;i++)
	{
		char *p=res[i];
		if(!strchr(p,'$'))
			continue;
		y_im_expand_with(p,p,strlen(p)+1,EXPAND_SPACE);

		if(p[0]!='$')
			continue;
		if(l_str_has_surround(p,"$CONFIG(",")"))
		{
			char *t=l_strndupa(p+8,strlen(p+8)-1);
			char **cfg=l_strsplit(t,',');
			if(!cfg[0] || !cfg[1])
			{
				l_strfreev(cfg);
				l_ptr_array_free(arr,l_free);
				return NULL;
			}
			t=y_im_get_config_string_gb(cfg[0],cfg[1]);
			l_strfreev(cfg);
			if(!t)
			{
				l_ptr_array_free(arr,l_free);
				return NULL;
			}
			l_free(p);
			l_ptr_array_nth(arr,i)=t;
		}
		else if(!strcmp(p,"$CAND"))
		{
			EXTRA_IM *eim=YongCurrentIM();
			if(!eim || !eim->CandWordCount)
			{
				l_ptr_array_free(arr,l_free);
				return NULL;
			}
			l_free(p);
			l_ptr_array_nth(arr,i)=l_strdup(eim->CandTable[eim->SelectIndex]);
		}
		else if(!strcmp(p,"$CODE"))
		{
			EXTRA_IM *eim=YongCurrentIM();
			if(!eim || !eim->CodeLen)
			{
				l_ptr_array_free(arr,l_free);
				return NULL;
			}
			l_free(p);
			l_ptr_array_nth(arr,i)=l_strdup(eim->CodeInput);
		}
		else if(!strcmp(p,"$CLIPBOARD"))
		{
			char *t=y_ui_get_select(NULL);
			if(!t)
			{
				l_ptr_array_free(arr,l_free);
				return NULL;
			}
			l_free(p);
			l_ptr_array_nth(arr,i)=t;
		}
		else if(l_str_has_prefix(p,"$(_HOME)") || l_str_has_prefix(p,"$(_DATA)"))
		{
			char temp[256];
			y_im_expand_with(p,temp,sizeof(temp),EXPAND_ENV);
			l_free(p);
			l_ptr_array_nth(arr,i)=l_strdup(temp);
		}
	}
	arr->data=NULL;
	l_ptr_array_free(arr,NULL);
	return res;
}

static void str_replace(char *s1,int l1,const char *s2)
{
	int l2=strlen(s2);
	int left=strlen(s1+l1);
	memmove(s1+l2,s1+l1,left+1);
	memcpy(s1,s2,l2);
}

int y_im_forward_key(const char *s)
{
	int key,repeat=1;
	if(s[0]!='$')
		return -1;
	key=y_im_str_to_key(s+1,&repeat);
	if(key<=0 || repeat<=0)
		return -1;
	y_xim_forward_key(key,repeat);
	if(key==YK_BACKSPACE)
	{
		last_output[0]=0;
		last_biaodian[0]=0;
		last_biaodian_only=0;
	}
	return 0;
}

void y_im_expand_space(char *s)
{
	y_im_expand_with(s,s,strlen(s)+1,EXPAND_SPACE);
}

void y_im_expand_env(char *s,int size)
{
	y_im_expand_with(s,s,size,EXPAND_ENV);
}

int y_im_expand_with(const char *s,char *to,int size,int which)
{
	int i,c,pos;
	for(i=pos=0;(c=s[i])!='\0' && pos<size-1;i++)
	{
		if(c!='$')
		{
			to[pos++]=c;
			continue;
		}
		if(s[i+1]=='$')
		{
			to[pos++]='$';
			i++;
			continue;
		}
		if((which&EXPAND_SPACE) && s[i+1]=='_')
		{
			to[pos++]=' ';
			i++;
			continue;
		}
		if((which&EXPAND_ENV) && s[i+1]=='(')
		{
			char name[32];
			int j;
			for(j=0;j<31;j++)
			{
				int c=s[i+j+2];
				if(c==0)
				{
					to[pos]=0;
					return -1;
				}
				if(c==')') break;
				name[j]=c;
			}
			
			if(j<31 && j>0)
			{
				char temp[256];
				const char *val;
				name[j]=0;
				val=l_getenv_gb(name,temp,sizeof(temp));
				if(val!=NULL)
				{
					int len=strlen(val);
					if(pos+len>=size-1)
					{
						to[pos]=0;
						return -1;
					}
					int left=i+3+j;
					if(s==to && pos+len>left)
					{
						memmove(to+pos+len,s+left,strlen(s+left)+1);
						memcpy(to+pos,val,len);
						i=pos+len-1;
					}
					else
					{
						memcpy(to+pos,val,len);
						i=left-1;
					}
					pos+=len;
				}
				continue;
			}
		}
		if((which&EXPAND_DESC) && (s[i+1]=='[' || s[i+1]==']'))
		{
			to[pos++]=s[++i];
			continue;
		}
		to[pos++]='$';
	}
	to[pos]=0;
	return pos;
}

int y_im_go_url(const char *s)
{
	char *tmp;

	if(s[0]!='$')
		return -1;

	if(s[1]=='G' && s[2]=='O' && s[3]=='(')
	{
		int len=strlen(s);
		if(s[len-1]==')')
		{
			char go[256];
			static const L_ESCAPE_CONFIG c={
				.lead='$',
				.flags=L_ESCAPE_GB|L_ESCAPE_LAST,
				.count=3,
				.sep=',',
				.env="$()",
				.surround={'(',')'},
				.map={{' ','_'},{',',','},{'$','$'}}
			};
			if(!l_unescape(s+3,go,sizeof(go),&c))
			{
				return -1;
			}
			tmp=go;
			if(!strncmp(tmp,"$DECRYPT(",9) && l_str_has_suffix(tmp,")"))
			{
				char dec[80];
				tmp[strlen(tmp)-1]=0;
				if(0!=y_im_book_decrypt(tmp+9,dec))
					return -1;
				y_xim_explore_url(dec);
				return 0;
			}
			//y_im_expand_with(tmp,tmp,sizeof(go)-(tmp-go),EXPAND_SPACE|EXPAND_ENV);
#ifndef CFG_XIM_ANDROID
			if(!strcmp(tmp,"sync"))
				tmp="yong-config --sync";
			else if(!strcmp(tmp,"update"))
				tmp="yong-config --update";
#endif
			y_xim_explore_url(tmp);
			return 0;
		}
	}
	return -1;
}

int y_im_send_file(const char *s)
{
	char *tmp;

	if(s[0]!='$')
		return -1;

	if(!strncmp(s+1,"FILE(",5))
	{
		int len=strlen(s);
		if(s[len-1]==')')
		{
			char go[128];
			strcpy(go,s+6);
			tmp=strchr(go,')');
			if(!tmp)
				return -1;
			*tmp=0;
			YongSendFile(go);
			return 0;
		}
	}
	return -1;
}

int y_im_str_desc(const char *s,void *out)
{
	if(s[0]!='$' || s[1]!='[')
		return 0;
	const char *end=s+2;
	int surround=1;
	while(1)
	{
		uint32_t hz;
		end=gb_next(end,&hz);
		if(!end)
			return 0;
		if(hz=='$')
		{
			end=gb_next(end,&hz);
			if(!end)
				return 0;
			if(hz=='[' || hz==']')
				continue;
		}
		if(hz=='[')
		{
			surround++;
		}
		if(hz==']')
		{
			surround--;
			if(surround==0)
				break;
		}
	}
	int ret=(int)(end-s-3);
	if(out && ret)
	{
		char temp[ret+1];
		l_strncpy(temp,s+2,ret);
		y_im_expand_with(temp,temp,sizeof(temp),EXPAND_ENV);
		y_im_str_encode(temp,out,0);
	}
	return ret+3;
}

char *y_im_str_escape(const char *s,int commit,int64_t t)
{
	char *ps;
	struct tm *tm;
	static char line[8192];

	/* test if escape needed */
	ps=strchr(s,'$');
	if(!ps)
	{
		/* copy, so we can always change the escaped string without change orig */
		strcpy(line,s);
		return line;
	}
	
	/* do $LAST first, so we can escape the content later */
	if(s[0]=='$' && !strcmp(s+1,"LAST"))
	{
		s=last_output;
		ps=strchr(s,'$');
		if(!ps)
		{
			strcpy(line,s);
			return line;
		}
	}
	
	/* is only a key, or url */
	if(s[0]=='$')
	{
		int key;
		key=y_im_str_to_key(s+1,NULL);
		if(key>0 && !(key&KEYM_MASK))
		{
			strcpy(line,s+1);
			return line;
		}
		while(s[1]=='G' && s[2]=='O' && s[3]=='(')
		{
			int len=strlen(s);
			if(s[len-1]==')')
			{
				static const L_ESCAPE_CONFIG c={
					.lead='$',
					.flags=L_ESCAPE_GB,
					.count=3,
					.sep=',',
					.surround={'(',')'},
					.map={{' ','_'},{',',','},{'$','$'}}
				};
				char go[MAX_CAND_LEN+1];
				l_unescape(s+3,go,sizeof(go),&c);
				snprintf(line,32,"->%s",go);
				return line;
			}
			break;
		}
		if(s[1]=='B' && s[2]=='D' && s[3]=='(' && s[4] && s[5]==')' && !s[6])
		{
			const char *temp;
			int lang=LANG_CN;
			CONNECT_ID *id=y_xim_get_connect();
			if(id) lang=id->biaodian;
			temp=YongGetPunc(s[4],lang,commit?0:1);
			if(!temp)
			{
				if(isgraph(s[4]))
				{
					line[0]=s[4];
					line[1]=0;
					return line;
				}
				return NULL;
			}
			strcpy(line,temp);
			return line;
		}
	}
	strcpy(line,s);
	s=line;
	ps=strchr(s,'$');
	if(!t) t=l_time();
	tm=l_localtime(&t);

	/* escape the time and $ self */
	do{
		ps++;
		/* self */
		if(!strncmp(ps,"$",1))
		{
			str_replace(ps-1,2,"$");
		}
		else if(!strncmp(ps,"_",1))
		{
			str_replace(ps-1,2," ");
		}
		else if(!strncmp(ps,"/",1))
		{
#ifdef _WIN32
			str_replace(ps-1,2,"\r\n");
#else
			str_replace(ps-1,2,"\n");
#endif
		}
		/* english */
		else if(!strncmp(ps,"ENGLISH",7))
		{
			str_replace(ps-1,8,"->EN");
		}
		/* time */
		else if(!strncmp(ps,"YYYY0",5))
		{
			char tmp[64];
			l_int_to_str(tm->tm_year+1900,NULL,L_INT2STR_HZ|L_INT2STR_ZERO0,tmp);
			str_replace(ps-1,6,tmp);
		}
		else if(!strncmp(ps,"YYYY",4))
		{
			char tmp[64];
			l_int_to_str(tm->tm_year+1900,NULL,L_INT2STR_HZ,tmp);
			str_replace(ps-1,5,tmp);
		}
		else if(!strncmp(ps,"yyyy",4))
		{
			char tmp[64];
			l_int_to_str(tm->tm_year+1900,NULL,0,tmp);
			str_replace(ps-1,5,tmp);
		}
		else if(!strncmp(ps,"yy0",2))
		{
			char tmp[64];
			l_int_to_str(tm->tm_year%100,"%02d",0,tmp);
			str_replace(ps-1,4,tmp);
		}
		else if(!strncmp(ps,"MON",3))
		{
			char tmp[64];
			l_int_to_str(tm->tm_mon+1,NULL,L_INT2STR_INDIRECT,tmp);
			str_replace(ps-1,4,tmp);
		}
		else if(!strncmp(ps,"mon0",4))
		{
			char tmp[64];
			l_int_to_str(tm->tm_mon+1,"%02d",0,tmp);
			str_replace(ps-1,5,tmp);
		}
		else if(!strncmp(ps,"mon",3))
		{
			char tmp[64];
			l_int_to_str(tm->tm_mon+1,NULL,0,tmp);
			str_replace(ps-1,4,tmp);
		}		
		else if(!strncmp(ps,"DAY",3))
		{
			char tmp[64];
			l_int_to_str(tm->tm_mday,NULL,L_INT2STR_INDIRECT,tmp);
			str_replace(ps-1,4,tmp);
		}
		else if(!strncmp(ps,"day0",4))
		{
			char tmp[64];
			l_int_to_str(tm->tm_mday,"%02d",0,tmp);
			str_replace(ps-1,5,tmp);
		}
		else if(!strncmp(ps,"day",3))
		{
			char tmp[64];
			l_int_to_str(tm->tm_mday,NULL,0,tmp);
			str_replace(ps-1,4,tmp);
		}
		else if(!strncmp(ps,"HOUR",4))
		{
			char tmp[64];
			l_int_to_str(tm->tm_hour,NULL,L_INT2STR_INDIRECT,tmp);
			str_replace(ps-1,5,tmp);
		}
		else if(!strncmp(ps,"hour0",5))
		{
			char tmp[64];
			l_int_to_str(tm->tm_hour,"%02d",0,tmp);
			str_replace(ps-1,6,tmp);
		}
		else if(!strncmp(ps,"hour",4))
		{
			char tmp[64];
			l_int_to_str(tm->tm_hour,NULL,0,tmp);
			str_replace(ps-1,5,tmp);
		}
		else if(!strncmp(ps,"MIN",3))
		{
			char tmp[64];
			l_int_to_str(tm->tm_min,NULL,L_INT2STR_HZ|L_INT2STR_MINSEC,tmp);
			str_replace(ps-1,4,tmp);
		}
		else if(!strncmp(ps,"min",3))
		{
			char tmp[64];
			l_int_to_str(tm->tm_min,"%02d",0,tmp);
			str_replace(ps-1,4,tmp);
		}
		else if(!strncmp(ps,"SEC",3))
		{
			char tmp[64];
			l_int_to_str(tm->tm_sec,NULL,L_INT2STR_HZ|L_INT2STR_MINSEC,tmp);
			str_replace(ps-1,4,tmp);
		}
		else if(!strncmp(ps,"sec",3))
		{
			char tmp[64];
			l_int_to_str(tm->tm_sec,"%02d",0,tmp);
			str_replace(ps-1,4,tmp);
		}
		else if(!strncmp(ps,"WEEK",4) || !strncmp(ps,"week",4))
		{
			static const char *week_name[]=
				{"日","一","二","三","四","五","六",""};
			const char *tmp=(char*)week_name[tm->tm_wday];
			str_replace(ps-1,5,tmp);
		}
		else if(!strncmp(ps,"RIQI",4))
		{
			char nl[128];
			y_im_nl_day(t,nl);
			str_replace(ps-1,5,nl);
		}
		else if(!strncmp(ps,"LAST",4))
		{
			str_replace(ps-1,5,last_output);
		}
		s=ps;ps=strchr(s,'$');
	}while(ps!=NULL);
	return line;
}

int y_im_strip_key(char *gb)
{
	char *s;
	int key=0;
	s=strrchr(gb,'$');
	if(s && s[1]=='|')
	{
		int i;
		key=0;
		memmove(s,s+2,strlen(s+2)+1);
		for(i=0;s[i];)
		{
			if(s[i] && !s[i+1])
			{
				key++;
				break;
			}
			if(s[i]&0x80)
			{
				key++;
				if(gb_is_gb18030_ext((void*)s+i))
					i+=4;
				else
					i+=2;
			}
			else
			{
				if(s[i]=='$' && (s[i+1]=='_' || s[i+1]=='$' || s[i+1]=='/'))
				{
					key++;
					i+=2;
				}
				else
				{
					key++;
					i++;
				}
			}
		}
	}
	else if(s)
	{
		key=y_im_str_to_key(s+1,NULL);
		if(key>0) *s=0;
		if(key==YK_LEFT)
			key=1;
		else
			key=0;
	}
	return key;
}

void y_im_disp_cand(const char *gb,char *out,int pre,int suf,const char *code,const char *tip)
{
	char temp[MAX_CAND_LEN+1];
	char *s=temp;
	int len;

	if(y_im_cand_desc_translate(gb,code,tip,temp,sizeof(temp)))
		goto SKIP;

	/* do $LAST first, so we can escape the content later */
	if(gb[0]=='$' && !strcmp(gb+1,"LAST"))
		strcpy(temp,last_output);
	else
		strcpy(temp,s2t_conv(gb));
	/* if found desc, only display it */
	int skip=y_im_str_desc(s,NULL);
	if(skip>0)
	{
		int ret=skip-3;
		memmove(s,s+2,ret);
		s[ret]=0;
		y_im_expand_with(s,s,sizeof(temp),EXPAND_DESC);
	}

	if(l_str_has_suffix(s,"$SPACE"))
	{
		int len=strlen(s);
		if(len>6 && s[len-7]!='$')
			s[len-6]=0;
	}

	/* escape the string */
	s=y_im_str_escape(s,0,0);
	/* strip key in it */
	y_im_strip_key(s);
SKIP:
	/* get the length of input */
	len=gb_strlen(s);
	/* get string should escape */
	skip=len-(pre+suf);
	if(skip<=0)
	{
		/* gb is from y_im_str_escape, we should not let it both in and out of this */
		y_im_str_encode(s,out,DONT_ESCAPE);
		return;
	}
	/* copy the string and skip some */
	char *pad_str=gb_offset(s,pre);
	char *suf_str=gb_offset(s,len-suf);
	memmove(pad_str+3,suf_str,strlen(suf_str)+1);
	pad_str[0]=pad_str[1]=pad_str[2]='.';
	y_im_str_encode(s,out,DONT_ESCAPE);
}

int y_im_str_encode(const char *gb,void *out,int flags)
{
	int key=-1;
	char *s;
	if(!gb[0])
	{
		memset(out,0,4);
		return 0;
	}
	if(!(flags&DONT_ESCAPE))
	{
		s=y_im_str_escape(gb,1,0);
		y_im_strip_key(s);
	}
	else
	{
		s=l_alloca(8192);
		strcpy(s,gb);
	}

#if defined(_WIN32) || defined(CFG_XIM_ANDROID)
	l_gb_to_utf16(s,out,8192);
#else
	l_gb_to_utf8(s,out,8192);
#endif

	return key;
}

int y_im_str_len(const void *in)
{
#if defined(_WIN32)
	return wcslen(in);
#elif defined(CFG_XIM_ANDROID)
	int i;
	for(i=0;;i++)
	{
		if(((const uint16_t*)in)[i]==0)
			break;
	}
	return i;
#else
	return strlen(in);
#endif
}

void y_im_str_encode_r(const void *in,char *gb)
{
#ifdef _WIN32
	l_utf16_to_gb(in,gb,4096);
#else
	l_utf8_to_gb(in,gb,4096);
#endif
}

void y_im_url_encode(const char *gb,char *out)
{
	int i;
	char temp[256];

	l_gb_to_utf8(gb,temp,sizeof(temp));
	for(i=0;temp[i];i++)
		sprintf(out+3*i,"%%%02x",(uint8_t)temp[i]);
}

int y_im_is_url(const char *s)
{
	const char *p1,*p2,*p3;
	if(s[0]=='"')
		return 0;
	p1=strchr(s,':');
	p2=strchr(s,' ');
	p3=strchr(s,'/');
	if(p1 && p3 && (!p2 || p2>p1))
		return 1;
	return 0;
}

char *y_im_auto_path(char *fn)
{
	char temp[256];

	assert(fn!=NULL);
	
	if(fn[0]=='/' || (fn[0]=='.' && fn[1]=='/'))
	{
		strcpy(temp,fn);
	}
	else
	{
		sprintf(temp,"%s/%s",y_im_get_path("HOME"),fn);
		if(!l_file_exists(temp))
			sprintf(temp,"%s/%s",y_im_get_path("DATA"),fn);
	}
	return l_strdup(temp);
}

FILE *y_im_open_file(const char *fn,const char *mode)
{
	FILE *fp;
	if(!fn)
		return NULL;

	if(fn[0]=='/' || (fn[0]=='.' && fn[1]=='/'))
	{
		if(strchr(mode,'w'))
			y_im_mkdir(fn);
		fp=l_file_open(fn,mode,NULL);
	}
	else
	{
		if(strchr(mode,'w'))
			y_im_mkdir(fn);
		fp=l_file_open(fn,mode,y_im_get_path("HOME"),
				y_im_get_path("DATA"),NULL);
	}
	return fp;
}

void y_im_remove_file(char *fn)
{
	char temp[256];

	if(!fn)
		return;
	
	if(fn[0]=='/')
	{
		strcpy(temp,fn);
		l_remove(temp);
	}
	else
	{
		sprintf(temp,"%s/%s",y_im_get_path("HOME"),fn);
		l_remove(temp);
	}
}

static char *y_im_urls[64]={
	"www.",
	"ftp.",
	"bbs.",
	"mail.",
	"blog.",
	"http",
	"*@",
};
static int urls_begin=0;

static Y_USER_URL *urls_user=0;

Y_USER_URL *y_im_user_urls(void)
{
	return urls_user;
}

void y_im_free_urls(void)
{
	Y_USER_URL *p,*n;
	int i;
	for(i=7;i<64;i++)
	{
		if(!y_im_urls[i])
			break;
		l_free(y_im_urls[i]);
		y_im_urls[i]=0;
	}
	urls_begin=0;
	for(p=urls_user;p;p=n)
	{
		n=p->next;
		l_free(p->url);
		l_free(p);
	}
	urls_user=0;
}

void y_im_load_urls(void)
{
	FILE *fp;
	int i;
	char temp[256];
	y_im_free_urls();
	fp=l_file_open("urls.txt","rb",y_im_get_path("HOME"),
				y_im_get_path("DATA"),NULL);
	if(!fp) return;	
	for(i=7;i<64;)
	{
		if(l_get_line(temp,256,fp)<0)
			break;
		if(temp[0]==0)
			continue;
		if(!strcmp(temp,"!zero"))
		{
			urls_begin=7;
			continue;
		}
		else if(!strcmp(temp,"!english"))
		{
			while(l_get_line(temp,256,fp)>0)
			{
				Y_USER_URL *item=l_new(Y_USER_URL);
				item->url=l_strdup(temp);
				urls_user=l_slist_append(urls_user,item);
			}
			break;
		}
		y_im_urls[i++]=l_strdup(temp);
	}
	fclose(fp);
}

char *y_im_find_url(const char *pre)
{
	int len;
	int i;
	if(!pre)
		return NULL;
	len=strlen(pre);
	if(len<=0)
		return NULL;
	for(i=urls_begin;i<64 && y_im_urls[i];i++)
	{
		if(!strncmp(pre,y_im_urls[i],len))
			return y_im_urls[i];
	}
	return NULL;
}

char *y_im_find_url2(const char *pre,int next)
{
	int len;
	int i;
	if(!pre)
		return NULL;
	len=strlen(pre);
	if(len<=0)
		return NULL;
	for(i=urls_begin;i<64 && y_im_urls[i];i++)
	{
		char *url=y_im_urls[i];
		if(!strncmp(pre,url,len) && next==url[len])
			return url;
		if(url[0]=='*' && url[1]==next && url[2]==0)
		{
			// 这时候last_code应该没有其他用途
			snprintf(last_code,60,"%s%c",pre,next);
			return last_code;
		}
	}
	return NULL;
}

void y_im_backup_file(const char *path,const char *suffix)
{
	char temp[260];
	sprintf(temp,"%s%s",path,suffix);
	l_remove(temp);
	y_im_copy_file(path,temp);
}

void y_im_copy_config(void)
{
	char path[256];
	LKeyFile *usr;
	LKeyFile *sys;

	sys=l_key_file_open("yong.ini",0,y_im_get_path("DATA"),NULL);
	if(!sys)
	{
		return;
	}
	sprintf(path,"%s/yong.ini",y_im_get_path("HOME"));
	if(l_file_exists(path))
		usr=l_key_file_open("yong.ini",0,y_im_get_path("HOME"),NULL);
	else
		usr=NULL;
	if(usr)
	{
		if(l_key_file_get_int(usr,"DESC","version")==1)
		{
			char *p=l_key_file_get_string(usr,"IM","default");
			if(p)
			{
				l_key_file_set_string(usr,"IM","default","0");
				l_key_file_set_string(usr,"IM","0",p);
				l_key_file_set_string(usr,"DESC","version","2");
				l_free(p);
				l_key_file_save(usr,y_im_get_path("HOME"));
			}
		}
		else
		{
			if(l_key_file_get_int(usr,"DESC","version")<
				l_key_file_get_int(sys,"DESC","version"))
			{
				char path[256];
				sprintf(path,"%s/yong.ini",y_im_get_path("HOME"));
				y_im_backup_file(path,".old");
				l_key_file_set_dirty(sys);
				l_key_file_save(sys,y_im_get_path("HOME"));
			}
		}
		l_key_file_free(usr);
	}
	else
	{
		l_key_file_set_dirty(sys);
		l_key_file_save(sys,y_im_get_path("HOME"));
	}
	l_key_file_free(sys);
}

void y_im_set_default(int index)
{
	int def=y_im_get_config_int("IM","default");
	
	if(def==index)
		return;
		
	l_key_file_set_int(MainConfig,"IM","default",index);
	l_key_file_save(MainConfig,y_im_get_path("HOME"));	
}

void y_im_save_config(void)
{
	if(!MainConfig)
		return;
	l_key_file_save(MainConfig,y_im_get_path("HOME"));	
}

void y_im_about_self(void)
{
	char temp[2048];
	int pos=0;
	
	pos=sprintf(temp,"%s%s\n",YT("作者："),"dgod");
	pos+=sprintf(temp+pos,"%s%s\n",YT("论坛："),"http://yong.dgod.net");
	/*pos+=*/sprintf(temp+pos,"%s%s\n",YT("编译时间："),YONG_VERSION);
	
#if 0
	pos+=sprintf(temp+pos,"%s%d",YT("机器码："),(int)y_im_gen_mac());
#endif

	y_ui_show_message(temp);
}


void *y_im_module_open(char *path)
{
	void *ret=0;
	char temp[256];
	
	if(strchr(path,'/'))
		strcpy(temp,path);
	else
		sprintf(temp,"%s/%s",y_im_get_path("LIB"),path);
#ifdef _WIN32
	WCHAR real[MAX_PATH];
	l_utf8_to_utf16(temp,real,sizeof(real));
	ret=(void*)LoadLibrary(real);
#else
	ret=dlopen(temp,RTLD_LAZY);
	if(!ret)
	{
		printf("dlopen %s error %s\n",temp,dlerror());
	}
#endif
	return ret;
}

void *y_im_module_symbol(void *mod,char *name)
{
	void *ret;
#ifdef _WIN32
	ret=(void*)GetProcAddress(mod,name);
#else
	ret=dlsym(mod,name);
#endif
	return ret;
}

void y_im_module_close(void *mod)
{
#ifdef _WIN32
	FreeLibrary(mod);
#else
	if(0!=dlclose(mod))
	{
		perror("dlclose");
	}
#endif
}

int y_im_run_tool(char *func,void *arg,void **out)
{
	int (*tool)(void *,void **);
	
	if(!im.handle)
	{
		printf("yong: no active module\n");
		return -1;
	}
	
	tool=y_im_module_symbol(im.handle,func);
	if(!tool)
	{
		printf("yong: this module don't have such tool\n");
		return -1;
	}

	return tool(arg,out);
}

char *y_im_get_im_config_string(int index,const char *key)
{
	char temp[8];
	const char *entry;
	sprintf(temp,"%d",index);
	entry=l_key_file_get_data(MainConfig,"IM",temp);
	if(!entry)
		return NULL;
	return y_im_get_config_string(entry,key);
}

const char *y_im_get_im_config_data(int index,const char *key)
{
	char temp[8];
	const char *entry;
	sprintf(temp,"%d",index);
	entry=l_key_file_get_data(MainConfig,"IM",temp);
	if(!entry)
		return NULL;
	return y_im_get_config_data(entry,key);
}

int y_im_has_im_config(int index,const char *key)
{
	char temp[8];
	const char *entry;
	sprintf(temp,"%d",index);
	entry=l_key_file_get_data(MainConfig,"IM",temp);
	if(!entry)
		return 0;
	return l_key_file_get_data(MainConfig,entry,"skin")?1:0;
}

int y_im_get_im_config_int(int index,const char *key)
{
	char temp[8];
	const char *entry;
	sprintf(temp,"%d",index);
	entry=l_key_file_get_data(MainConfig,"IM",temp);
	if(!entry)
		return 0;
	return y_im_get_config_int(entry,key);
}

char *y_im_get_current_engine(void)
{
	static char engine[32];
	char *entry;
	char *p;
	sprintf(engine,"%d",im.Index);
	entry=y_im_get_config_string("IM",engine);
	engine[0]=0;
	if(!entry)
		return engine;
	p=y_im_get_config_string(entry,"engine");
	snprintf(engine,32,"%s",p);
	l_free(entry);
	l_free(p);
	return engine;
}

char *y_im_get_im_name(int index)
{
	char *entry;
	char temp[32];

	sprintf(temp,"%d",index);
	entry=y_im_get_config_string("IM",temp);
	if(entry)
	{
		char *p;
		p=y_im_get_config_string(entry,"name");
		l_free(entry);
		if(p)
		{
			int size=strlen(p)*2+1;
			entry=l_alloc(size);
			l_utf8_to_gb(p,entry,size);
			l_free(p);
		}
		else
		{
			entry=0;
		}
	}
	if(!entry && index==im.Index && im.eim)
		entry=l_strdup(im.eim->Name);
	return entry;
}

void y_im_setup_config(void)
{
#if !defined(CFG_NO_HELPER)
	char config[256];
	char *args[3]={0,config,0};
	char prog[512];

#ifdef _WIN32
	char *setup="yong-config.exe";
#else
	char *setup="yong-config";
#endif

	sprintf(config,"%s/yong.ini",y_im_get_path("HOME"));

	if(!args[0] && l_file_exists(setup))
	{
		args[0]=setup;
	}
#ifndef _WIN32
	if(!args[0] && l_file_exists("/usr/bin/yong-config"))
	{
		args[0]="/usr/bin/yong-config";
	}
#endif
	if(!args[0])
	{
		static char user[256];
		char *tmp=y_im_get_config_string("main","edit");
		if(tmp && tmp[0] && tmp[0]!=' ')
		{
			strcpy(user,tmp);
			args[0]=user;
		}
		l_free(tmp);
	}
#ifndef _WIN32
	if(!args[0]) args[0]="xdg-open";
#else
	if(!args[0]) args[0]="notepad.exe";
#endif
	snprintf(prog,sizeof(prog),"%s %s",args[0],config);

	if(strstr(args[0],"yong-config"))
	{
		char temp[32];
		y_im_get_current(temp,sizeof(temp));
		sprintf(prog+strlen(prog)," --active=%s",temp);
	}

	y_im_run_helper(prog,config,YongReloadAllTip,NULL);
#endif
}

#if !defined(CFG_NO_HELPER)
uint32_t y_im_tick(void)
{
#ifdef _WIN32
	return GetTickCount();
#else
	struct timeval tv;
	gettimeofday(&tv,0);
	return (uint32_t)(tv.tv_sec*1000+tv.tv_usec/1000);
#endif
}

struct im_helper{
#ifdef _WIN32
	HANDLE pid;
	UINT_PTR timer;
#else
	GPid pid;
	guint timer;
	guint child;
#endif
	char *prog;
	char *watch;
	time_t mtime;
	void (*cb)(void);
	void (*exit_cb)(int);
};
static struct im_helper helper_list[4];

static time_t y_im_last_mtime(const char *file)
{
#if 0
#ifdef _WIN32
	struct _stat st;
	wchar_t temp[MAX_PATH];
	if(!file) return 0;
	l_utf8_to_utf16(file,temp,sizeof(temp));
	if(0!=_wstat(temp,&st))
		return 0;
	return st.st_mtime;
#else
	struct stat st;
	if(!file) return 0;
	if(0!=stat(file,&st))
		return 0;
	return st.st_mtime;
#endif
#endif
	return l_file_mtime(file);
}

#ifdef _WIN32
static VOID CALLBACK HelperTimerProc(HWND hwnd,UINT uMsg,UINT_PTR idEvent,DWORD dwTime)
{
	int i;
	for(i=0;i<4;i++)
	{
		struct im_helper *p=helper_list+i;
		DWORD code;
		if(!p->prog) continue;
		if(p->timer!=idEvent) continue;
		if(p->watch)
		{
			time_t mtime;
			mtime=y_im_last_mtime(p->watch);
			if(mtime!=p->mtime)
			{
				p->mtime=mtime;
				if(p->cb)
					p->cb();
			}
		}
		if(!GetExitCodeProcess(p->pid,&code) || code!=STILL_ACTIVE)
		{
			CloseHandle(p->pid);
			l_free(p->prog);
			p->prog=0;
			l_free(p->watch);
			p->watch=0;
			KillTimer(NULL,idEvent);
			if(p->exit_cb)
				p->exit_cb((int)code);
		}
		break;
	}
}

int y_im_run_helper(const char *prog,const char *watch,void (*cb)(void),void (*exit_cb)(int))
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	BOOL ret;
	int i;
	WCHAR wprog[MAX_PATH];
	LPCWSTR lpCurrentDirectory=NULL;
	
#ifdef _WIN64
	int is_setup=0;
	if(strstr(prog,"yong-config"))
		is_setup=1;
	if(!is_setup)
	{
		if(!strchr(prog,':') && !l_file_exists(prog))
		{
			char *temp=alloca(strlen(prog)+8);
			sprintf(temp,"..\\%s",prog);
			prog=temp;
		}
		lpCurrentDirectory=L".";
	}
#endif
	
	for(i=0;i<4;i++)
	{
		char *p=helper_list[i].prog;
		if(!p) continue;
		if(!strcmp(p,prog))
			return -1;
	}
	l_utf8_to_utf16(prog,wprog,sizeof(wprog));
	
	memset(&si,0,sizeof(si));
	si.cb=sizeof(si);
	si.wShowWindow = SW_SHOWNORMAL;
	si.dwFlags = STARTF_USESHOWWINDOW;
	memset(&pi,0,sizeof(pi));
	ret=CreateProcess(NULL,wprog,NULL,NULL,FALSE,0,NULL,lpCurrentDirectory,&si,&pi);
	if(!ret)
	{
		return -2;
	}
	CloseHandle(pi.hThread);

	for(i=0;i<4;i++)
	{
		struct im_helper *p=helper_list+i;
		if(p->prog) continue;
		p->prog=l_strdup(prog);
		p->watch=watch?l_strdup(watch):NULL;
		p->pid=pi.hProcess;
		p->mtime=watch?y_im_last_mtime(watch):0;
		p->timer=SetTimer(NULL,0,1000,HelperTimerProc);
		p->cb=cb;
		p->exit_cb=exit_cb;
		return 0;
	}
	CloseHandle(pi.hProcess);
	return -1;
}
#else
static void  HelperExit(GPid pid,gint status,gpointer data)
{
	int i;
	for(i=0;i<4;i++)
	{
		struct im_helper *p=helper_list+i;
		if(p->pid!=pid) continue;
		if(p->watch)
		{
			time_t mtime;
			mtime=y_im_last_mtime(p->watch);
			if(mtime!=p->mtime)
			{
				p->mtime=mtime;
				if(p->cb)
					p->cb();
			}
		}
		g_free(p->prog);
		p->prog=0;
		g_free(p->watch);
		p->watch=0;
		g_source_remove(p->timer);
		g_source_remove(p->child);
		if(p->exit_cb)
			p->exit_cb((int)status);
	}
	g_spawn_close_pid(pid);
}

static gboolean HelperTimerProc(gpointer data)
{
	struct im_helper *p=data;
	if(p->watch)
	{
		time_t mtime;
		mtime=y_im_last_mtime(p->watch);
		if(mtime!=p->mtime)
		{
			p->mtime=mtime;
			if(p->cb)
				p->cb();
		}
	}
	return TRUE;
}

int y_im_run_helper(const char *prog,const char *watch,void (*cb)(void),void (*exit_cb)(int))
{
	int i;
	gint argc;
	gchar **argv;
	GPid pid;
	for(i=0;i<4;i++)
	{
		char *p=helper_list[i].prog;
		if(!p) continue;
		if(!strcmp(p,prog))
			return -1;
	}
	if(!g_shell_parse_argv(prog,&argc,&argv,NULL))
		return -1;
	if(!g_spawn_async(0,argv,0,
			G_SPAWN_DO_NOT_REAP_CHILD|G_SPAWN_SEARCH_PATH,
			0,0,&pid,0))
	{
		g_strfreev(argv);
		return -1;
	}
	g_strfreev(argv);
	for(i=0;i<4;i++)
	{
		struct im_helper *p=helper_list+i;
		if(p->prog) continue;
		p->prog=g_strdup(prog);
		p->watch=watch?g_strdup(watch):NULL;
		p->pid=pid;
		p->mtime=watch?y_im_last_mtime(watch):0;
		p->timer=g_timeout_add(1000,HelperTimerProc,p);
		p->cb=cb;
		p->exit_cb=exit_cb;
		p->child=g_child_watch_add(pid,HelperExit,p);
		return 0;
	}
	g_spawn_close_pid(pid);
	return -1;
}
#endif
#endif

int y_strchr_pos(char *s,int c)
{
	int i;
	for(i=0;s[i];i++)
	{
		if(s[i]==c)
			return i;
	}
	return -1;
}

void y_im_str_strip(char *s)
{
	int c,l;
	while((c=*s++)!=0)
	{
		if(c!=' ') continue;
		l=strlen(s);
		memmove(s-1,s,l+1);
		s--;
	}
}

void y_im_show_help(char *wh)
{
	char *p,*ps;
	p=y_im_get_config_string(wh,"help");
	if(!p) return;
	ps=strchr(p,' ');
	if(ps)
		y_xim_explore_url(ps+1);
	l_free(p);
}

int y_im_help_desc(char *wh,char *desc,int len)
{
	char *p,*ps;
	p=y_im_get_config_string(wh,"help");
	if(!p) return -1;
	ps=strchr(p,' ');
	if(ps) *ps=0;
	snprintf(desc,len,"%s",p);
	l_free(p);
	return 0;
}

int y_im_get_current(char *item,int len)
{
	char *p;
	char wh[32];
	sprintf(wh,"%d",im.Index);
	p=l_key_file_get_string(MainConfig,"IM",wh);
	if(!p) return -1;
	snprintf(item,len,"%s",p);
	l_free(p);
	return 0;
}

int y_im_get_keymap(char *name,int len)
{
	char item[128];
	char *p,*ps;
	if(0!=y_im_get_current(item,sizeof(item)))
		return -1;
	p=y_im_get_config_string(item,"keymap");
	if(!p) return -1;
	ps=strchr(p,' ');
	if(!ps)
	{
		l_free(p);
		return -1;
	}
	*ps=0;
	if(strlen(p)+1>len)
	{
		l_free(p);
		return -1;
	}
	strcpy(name,p);
	l_free(p);
	return 0;
}

int y_im_show_keymap(void)
{
	char item[128];
	char img[128];
	int top=0;
	int tran=0;
	char *p;
	int ret;
	if(0!=y_im_get_current(item,sizeof(item)))
		return -1;
	p=y_im_get_config_string(item,"keymap");
	if(!p) return -1;
	ret=l_sscanf(p,"%s %s %d %d",item,img,&top,&tran);
	l_free(p);
	if(ret<2) return -1;
	y_ui_show_image(item,img,top,tran);
	return 0;
}



static FILE *d_fp;
void y_im_debug(char *fmt,...)
{
	va_list ap;
	if(!d_fp)
	{
		d_fp=fopen("log.txt","w");
	}
	if(!d_fp) return;
	va_start(ap,fmt);
	vfprintf(d_fp,fmt,ap);
	va_end(ap);
	fflush(d_fp);
}

int y_im_diff_hand(char c1,char c2)
{
	const char *left="qwertasdfgzxcvb";
	if(c2==' ') return 0;
	c1=strchr(left,c1)?1:0;
	c2=strchr(left,c2)?1:0;
	return c1!=c2;
}

int y_im_request(int cmd)
{
	switch(cmd){
	case 1:
		YongKeyInput(YK_VIRT_REFRESH,0);
		break;
	default:
		break;
	};
	return 0;
}

void y_im_update_main_config(void)
{	
	l_key_file_free(MainConfig);
	MainConfig=l_key_file_open("yong.ini",1,y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);

#ifndef CFG_NO_MENU
	if(!MenuConfig)
	{
		const char *p;
		l_key_file_free(MenuConfig);
		p=l_key_file_get_data(MainConfig,"main","menu");
		if(p)
			MenuConfig=l_key_file_open(p,0,y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);
		if(!MenuConfig)
		{
			extern const char *ui_menu_default;
			MenuConfig=l_key_file_load(ui_menu_default,-1);
		}
	}
#endif
}

LKeyFile *y_im_get_menu_config(void)
{
	return MenuConfig;
}

void y_im_update_sub_config(const char *name)
{
	l_key_file_free(SubConfig);
	if(name)
	{
		SubConfig=l_key_file_open(name,0,y_im_get_path("HOME"),
			y_im_get_path("DATA"),NULL);
	}
	else
	{
		SubConfig=NULL;
	}
}

void y_im_free_config(void)
{
	if(SubConfig)
	{
		l_key_file_free(SubConfig);
		SubConfig=NULL;
	}
	if(MainConfig)
	{
		l_key_file_free(MainConfig);
		MainConfig=NULL;
	}
}

char *y_im_get_config_string(const char *group,const char *key)
{
#if CFG_XIM_ANDROID
	if(!MainConfig)
		y_im_update_main_config();
#endif
	if(SubConfig)
	{
		char *s=l_key_file_get_string(SubConfig,group,key);
		if(s) return s;
	}
	char *res= l_key_file_get_string(MainConfig,group,key);
	return res;
}

char *y_im_get_config_string_gb(const char *group,const char *key)
{
	if(SubConfig)
	{
		char *s=l_key_file_get_string_gb(SubConfig,group,key);
		if(s) return s;
	}
	char *res= l_key_file_get_string_gb(MainConfig,group,key);
	return res;
}

const char *y_im_get_config_data(const char *group,const char *key)
{
	if(SubConfig)
	{
		const char *s=l_key_file_get_data(SubConfig,group,key);
		if(s) return s;
	}
	return l_key_file_get_data(MainConfig,group,key);
}

int y_im_get_config_int(const char *group,const char *key)
{
	if(SubConfig)
	{
		const char *s=l_key_file_get_data(SubConfig,group,key);
		if(s) return atoi(s);
	}
	return l_key_file_get_int(MainConfig,group,key);
}

int y_im_has_config(const char *group,const char *key)
{
	if(SubConfig)
	{
		const char *s=l_key_file_get_data(SubConfig,group,key);
		if(s) return 1;
	}
	return l_key_file_get_data(MainConfig,group,key)?1:0;
}

void y_im_set_config_string(const char *group,const char *key,const char *val)
{
	if(!MainConfig)
	{
		MainConfig=l_key_file_open("yong.ini",1,y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);
		if(!MainConfig)
			return;
	}
	l_key_file_set_string(MainConfig,group,key,val);
}

void y_im_verbose(const char *fmt,...)
{
	va_list ap;
	va_start(ap,fmt);
	vprintf(fmt,ap);
	va_end(ap);
}

#ifndef CFG_XIM_ANDROID
int y_im_handle_menu(const char *cmd)
{
 	if(!strcmp(cmd,"$CONFIG"))
	{
		y_im_setup_config();
	}
	else if(!strcmp(cmd,"$RELOAD"))
	{
		YongReloadAllTip();
	}
	else if(!strcmp(cmd,"$ABOUT"))
	{
		y_im_about_self();
	}
	else if(!strcmp(cmd,"$STAT"))
	{
		char *stat=y_im_speed_stat();
		if(stat)
		{
			y_ui_show_message(stat);
			l_free(stat);
		}
	}
	else if(!strncmp(cmd,"$HELP(",6))
	{
		char temp[64];
		l_sscanf(cmd+6,"%64[^)]",temp);
		if(!strcmp(temp,"?"))
			y_im_get_current(temp,sizeof(temp));
		y_im_show_help(temp);
	}
	else if(!strncmp(cmd,"$GO(",4))
	{
		char temp[256];
		int len;
		//l_sscanf(cmd+4,"%256[^)]",temp);
		//y_xim_explore_url(temp);
		snprintf(temp,256,"%s",cmd+4);
		len=strlen(temp);
		if(len>0 && temp[len-1]==')')
			temp[len-1]=0;
		y_xim_explore_url(temp);
	}
	else if(!strcmp(cmd,"$KEYMAP"))
	{
		y_im_show_keymap();
	}
	else if(!strcmp(cmd,"$MBO"))
	{
		int ret;
		void *out=NULL;
		ret=y_im_run_tool("tool_save_user",0,0);
		if(ret!=0) return 0;
		y_im_async_wait(1000);
		ret=y_im_run_tool("tool_get_file","main",&out);
		if(ret!=0 || !out) return 0;
		y_im_backup_file(out,".bak");
		y_im_run_tool("tool_optimize",0,0);
		y_ui_show_message(YT("完成"));
	}
	else if(!strcmp(cmd,"$MBM"))
	{
		int ret;
		void *out=NULL;
		ret=y_im_run_tool("tool_save_user",0,0);
		if(ret!=0) return 0;
		y_im_async_wait(1000);
		ret=y_im_run_tool("tool_get_file","main",&out);
		if(ret!=0 || !out) return 0;
		y_im_backup_file(out,".bak");
		ret=y_im_run_tool("tool_get_file","user",&out);
		if(ret!=0 || !out) return 0;
		y_im_backup_file(out,".bak");
		ret=y_im_run_tool("tool_merge_user",0,0);
		if(ret!=0) return 0;
		y_im_remove_file(out);
		YongReloadAllTip();
		y_ui_show_message(YT("完成"));
	}
#if !defined(CFG_NO_HELPER)
	else if(!strcmp(cmd,"$MBEDIT"))
	{
		int ret;
		void *out=NULL;
		char *ed;
		char temp[256];
		ed=y_im_get_config_string("table","edit");
		if(!ed) return 0;
		ret=y_im_run_tool("tool_get_file","main",&out);
		if(ret!=0 || !out)
		{
			l_free(ed);
			return 0;
		}
		out=y_im_auto_path(out);
		sprintf(temp,"%s %s",ed,(char*)out);
		y_im_run_helper(temp,out,YongReloadAllTip,NULL);
		l_free(ed);
		l_free(out);
	}
#endif
	else if(!strncmp(cmd,"$MSG(",5))
	{
		char temp[512];
		int len;
		l_utf8_to_gb(cmd+5,temp,sizeof(temp));
		len=strlen(temp);
		if(len>0)
		{
			if(temp[len-1]==')')
				temp[len-1]=0;
			y_ui_show_message(temp);
		}
	}
	return 0;
}
#endif
