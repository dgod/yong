#include "config_ui.h"
#include "translate.h"

#include <sys/stat.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#include <conio.h>
#else
#include <unistd.h>
#include <glib.h>
#endif

#include "custom.c"
#include "sync.h"
#include "update.h"

LKeyFile *config;
LXml *custom;
double CU_SCALE=1.0;
int cu_reload_ui;
int cu_quit_ui;

const char *y_im_get_path(const char *type)
{
	const char *ret;
#if defined(CFG_XIM_ANDROID)
	if(!strcmp(type,"LIB"))
	{
		ret="/data/data/net.dgod.yong/lib";
	}
	else if(!strcmp(type,"HOME"))
	{
		ret="/sdcard/yong/.yong";
		if(!l_file_exists(ret))
			l_mkdir(ret,0700);
	}
	else
	{
		ret="/sdcard/yong";
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
			char *sys=getenv("ProgramFiles");
			if(sys)
			{
				sys=l_strdupa(sys);
				char path[MAX_PATH];
				GetCurrentDirectoryA(sizeof(path),path);
				if(l_str_has_suffix(sys," (x86)"))
					sys[strlen(sys)-6]=0;
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

LKeyFile *y_im_load_config(char *fn)
{
	return l_key_file_open(fn,1,y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);
}

void y_im_backup_file(char *path,char *suffix)
{
	char temp[256];
	sprintf(temp,"%s%s",path,suffix);
	l_file_copy(temp,path,y_im_get_path("HOME"),NULL);
}

void y_im_save_config(LKeyFile *cfg,char *file)
{
	y_im_backup_file(file,".old");	
	l_key_file_save(cfg,y_im_get_path("HOME"));
}

int y_im_set_default(int index)
{
	int def=l_key_file_get_int(config,"IM","default");
	
	if(def==index)
		return -1;
		
	l_key_file_set_int(config,"IM","default",index);
	l_key_file_save(config,y_im_get_path("HOME"));
	
	return 0;
}

void cu_config_save(void)
{
	y_im_save_config(config,"yong.ini");
}

void cu_init_all(CUCtrl ctrl,void *user)
{
	if(ctrl->init_done)
		return;
	if(!ctrl->init)
	{
		cu_config_init_default(ctrl);
	}
	else
	{
		cu_ctrl_action_run(ctrl,ctrl->init);
		cu_ctrl_action_free(ctrl,ctrl->init);
	}
	ctrl->init_done=1;
}

char **cfg_build_im_list(int *def)
{
	char **list;
	int i;
	list=l_alloc0(sizeof(char*)*64);
	for(i=0;i<63;i++)
	{
		char temp[32];
		sprintf(temp,"%d",i);
		list[i]=l_key_file_get_string(config,"IM",temp);
		if(!list[i]) break;
	}
	if(def) *def=l_key_file_get_int(config,"IM","default");
	return list;
}

int cfg_get_im_count(void)
{
	int i;
	for(i=0;i<63;i++)
	{
		char temp[32];
		const char *s;
		sprintf(temp,"%d",i);
		s=l_key_file_get_data(config,"IM",temp);
		if(!s) break;
	}
	return i;
}

int cfg_uninstall(const char *name)
{
	char **list;
	int def;
	int i;
	int ret=-1;
	list=cfg_build_im_list(&def);
	for(i=0;list[i]!=NULL;i++)
	{
		if(strcmp(name,list[i]))
			continue;

		if(def==i) def=0;
		else if(def>i) def--;
		l_key_file_set_int(config,"IM","default",def);
		
		l_key_file_remove_group(config,name);
		for(;list[i]!=NULL;i++)
		{
			char temp[32];
			sprintf(temp,"%d",i);
			l_key_file_set_string(config,"IM",temp,list[i+1]);
		}
		ret=0;
		y_im_save_config(config,"yong.ini");
		break;
	}
	l_strfreev(list);
	return ret;
}

int cfg_install(const char *name)
{
	int count;
	char temp[256];
	LKeyFile *entry;
	const char *group;
	int res=-1;
	char **keys=NULL;
	int i;
	
	count=cfg_get_im_count();
	if(count>63)
		return -1;
	if(l_str_has_suffix(name,".ini"))
	{
		entry=l_key_file_open(name,0,NULL);
	}
	else
	{
		snprintf(temp,sizeof(temp),"%s/%s",name,"entry.ini");
		entry=l_key_file_open(temp,0,y_im_get_path("HOME"),
			y_im_get_path("DATA"),NULL);
	}
	if(!entry) return -1;
	group=l_key_file_get_start_group(entry);
	if(!group || l_key_file_has_group(config,group))
		goto out;
	keys=l_key_file_get_keys(entry,group);
	if(!keys || !keys[0])
	{
		goto out;
	}
	for(i=0;keys[i]!=NULL;i++)
	{
		const char *p=l_key_file_get_data(entry,group,keys[i]);
		l_key_file_set_data(config,group,keys[i],p);
	}
	sprintf(temp,"%d",count);
	l_key_file_set_string(config,"IM",temp,group);
	res=0;
	y_im_save_config(config,"yong.ini");
out:
	l_strfreev(keys);
	l_key_file_free(entry);
	return res;
}

void cu_reload(void)
{
	cu_reload_ui=1;
	cu_quit();
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

void cu_notify_reload()
{
#ifdef _WIN32
	ShellExecute(NULL,L"open",L"yong-vim.exe",L"--reload-all",NULL,SW_SHOWNORMAL);
#else
	g_spawn_command_line_async("./yong-vim --reload-all",NULL);
#endif
}

int GetSetConfigMain(int argc,char **argv)
{
	int i;
	int reload=0;
	int set=0;
#ifdef _WIN32
	BOOL console=AttachConsole((DWORD)-1);
#endif
	for(i=0;i<argc;i++)
	{
		if(!strcmp(argv[i],"--get") && i+2<argc)
		{
			const char *group=argv[++i];
			const char *key=argv[++i];
			char *val=l_key_file_get_string(config,group,key);
			if(!val)
				return -1;
			printf("%s\n",val);
#ifdef _WIN32
			if(console)
				_cprintf("%s\n",val);
#endif
			l_free(val);
		}
		else if(!strcmp(argv[i],"--set") && i+3<argc)
		{
			set++;
			const char *group=argv[++i];
			const char *key=argv[++i];
			const char *val=argv[++i];
			if(!strcmp(val,"null"))
				val=NULL;
			l_key_file_set_string(config,group,key,val);
		}
		else if(!strcmp(argv[i],"--reload"))
		{
			reload=1;
		}
		else
		{
			return -2;
		}
	}
	if(set)
	{
		cu_config_save();
		if(reload)
		{
			cu_notify_reload();
		}
	}
	return 0;
}

int main(int arc,char *arg[])
{
	CUCtrl win;
	char *temp;
	int i;
	
	y_im_set_exec();
	
	config=y_im_load_config("yong.ini");
	if(!config) return -1;
	
	for(i=1;i<arc;i++)
	{
		if(!strncmp(arg[i],"--install=",10))
		{
			exit(cfg_install(arg[i]+10));
		}
		else if(!strncmp(arg[i],"--uninstall=",12))
		{
			exit(cfg_uninstall(arg[i]+12));
		}
		else if(!strcmp(arg[i],"--sync"))
		{
			i++;
			return SyncMain(arc-i,arg+i);
		}
		else if(!strcmp(arg[i],"--update"))
		{
			return UpdateMain();
		}
		else if(!strcmp(arg[i],"--get") || !strcmp(arg[i],"--set"))
		{
			return GetSetConfigMain(arc-1,arg+1);
		}
	}
	
	temp=l_key_file_get_string(config,"main","config");
	if(temp && temp[0])
	{
		char *p;
		p=l_file_get_contents(temp,NULL,y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);
		if(!p) return -1;
		custom=l_xml_load(p);
		l_free(p);
		if(!custom) return -1;
	}
	else
		custom=l_xml_load((const char*)config_custom);
	l_free(temp);
	
	temp=l_key_file_get_string(config,"main","translate");
	if(temp)
	{
		y_translate_init(temp);
		l_free(temp);
	}
	
	cu_init();

reload:
	if(cu_reload_ui)
	{
		cu_reload_ui=0;
		config=y_im_load_config("yong.ini");
		if(!config) return -1;
	}
	cu_quit_ui=0;

	win=cu_ctrl_new(NULL,custom->root.child);
	if(!win) return -1;

	cu_ctrl_foreach(win,cu_init_all,NULL);

	cu_ctrl_foreach(win,cu_init_all,NULL);
	cu_show_page("page-im");
	cu_ctrl_show_self(win,1);

	cu_loop();
	
	cu_ctrl_free(win);win=NULL;
	l_key_file_free(config);config=NULL;
	if(cu_reload_ui) goto reload;

	l_xml_free(custom);custom=NULL;

	return 0;
}
