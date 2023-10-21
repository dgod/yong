/* The PID of a window owner is stored in the X property _NET_WM_PID */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "llib.h"

#include "xim.h"
#include "ybus.h"
#include "im.h"
#include "common.h"
#include "translate.h"

#ifndef _WIN32
#include <dirent.h>
#endif

static YBUS_PLUGIN *plugin_list;
static YBUS_CONNECT *conn_list;
static YBUS_CONNECT *conn_active;
static int onspot;
static int def_lang;
static int64_t last_recycle;

static YBUS_CONNECT *conn_wm;
typedef struct{
	int valid;
	int x,y;
}WM_FOCUS;
static WM_FOCUS wm_focus;
static char *wm_icon[2];

int ybus_init(void)
{
	return 0;
}

void ybus_destroy(void)
{
	while(conn_list)
		ybus_free_connect(conn_list);
	conn_active=NULL;
	
	l_free(wm_icon[0]);
	wm_icon[0]=NULL;
	l_free(wm_icon[1]);
	wm_icon[1]=NULL;
}

void ybus_add_plugin(YBUS_PLUGIN *plugin)
{
	plugin_list=(YBUS_PLUGIN*)l_slist_prepend((LSList*)plugin_list,(LSList*)plugin);
}

int ybus_init_plugins(void)
{
	YBUS_PLUGIN *p,*n;
	int trigger;
		
	trigger=y_im_get_key("trigger",-1,CTRL_SPACE);
	onspot=y_im_get_config_int("IM","onspot");
	def_lang=y_im_get_config_int("IM","lang");
	for(p=plugin_list;p!=NULL;p=n)
	{
		n=p->next;
		p->config(0,0,"trigger",trigger);
		p->config(0,0,"onspot",onspot);
		if(0!=p->init())
		{
			plugin_list=l_slist_remove(plugin_list,p);
			//return -1;
		}
	}
	if(l_slist_length(plugin_list)==0)
		return -1;
	return 0;
}

int64_t ybus_now(void)
{
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return tv.tv_sec*1000+tv.tv_usec/1000;
}

static void xim_ybus_update_config(void)
{
	YBUS_PLUGIN *p,*n;
	onspot=y_im_get_config_int("IM","onspot");
	def_lang=y_im_get_config_int("IM","lang");
	for(p=plugin_list;p!=NULL;p=n)
	{
		n=p->next;
		p->config(0,0,"onspot",onspot);
	}
}

YBUS_CONNECT *ybus_find_connect(YBUS_PLUGIN *plugin,CONN_ID conn_id)
{
	YBUS_CONNECT *p;
	if(conn_active)
	{
		if(plugin->match_connect)
		{
			if(plugin->match_connect(conn_id,conn_active->id))
				return conn_active;
		}
		else
		{
			if(conn_id==conn_active->id)
				return conn_active;
		}
	}
	for(p=conn_list;p!=NULL;p=p->next)
	{
		if(p->plugin!=plugin) continue;
		if(plugin->match_connect)
		{
			if(plugin->match_connect(conn_id,p->id))
				return p;
		}
		else
		{
			if(conn_id==p->id)
				return p;
		}
	}
	return NULL;
}

static void ybus_recycle_connect(int64_t now);

YBUS_CONNECT *ybus_add_connect(YBUS_PLUGIN *plugin,CONN_ID conn_id)
{
	YBUS_CONNECT *conn;
	int64_t now=ybus_now();
	conn=ybus_find_connect(plugin,conn_id);
	if(conn) return conn;
	conn=l_new0(YBUS_CONNECT);
	conn->plugin=plugin;
	if(plugin->copy_connect_id)
		conn->id=plugin->copy_connect_id(conn_id);
	else
		conn->id=conn_id;
	conn->lang=LANG_CN;
	conn->biaodian=im.Biaodian;
	conn->corner=CORNER_HALF;
	conn->trad=im.TradDef;
	conn->alive=now;
	conn_list=l_slist_prepend(conn_list,conn);
	if(plugin->getpid)
		conn->pid=plugin->getpid(conn_id);
	
	ybus_recycle_connect(now);
	return conn;
}

void ybus_free_connect(YBUS_CONNECT *conn)
{
	YBUS_PLUGIN *plugin;
	if(!conn) return;
	if(conn==conn_wm)
	{
		conn_wm=NULL;
	}
	plugin=conn->plugin;
	if(conn_active==conn)
	{
		YongShowInput(0);
		YongShowMain(0);
		YongResetIM();
		conn_active=NULL;
	}
	while(conn->client!=NULL)
	{
		ybus_free_client(conn,(YBUS_CLIENT*)conn->client);
	}
	if(plugin->free_connect_id)
		plugin->free_connect_id(conn->id);
	conn_list=l_slist_remove(conn_list,(YBUS_CONNECT*)conn);
	l_free(conn);
}

YBUS_CLIENT *ybus_find_client(YBUS_CONNECT *conn,CLIENT_ID client_id)
{
	if(!conn)
		return NULL;
	YBUS_PLUGIN *plugin=conn->plugin;
	YBUS_CLIENT *p;
	if(conn->active)
	{
		if(plugin->match_client)
		{
			if(plugin->match_client(client_id,conn->active->id))
				return conn->active;
		}
		else
		{
			if(conn->active->id==client_id)
				return conn->active;
		}
	}
	for(p=(YBUS_CLIENT*)conn->client;p!=NULL;p=p->next)
	{
		if(plugin->match_client)
		{
			if(plugin->match_client(client_id,p->id))
				return p;
		}
		else
		{
			if(client_id==p->id)
				return p;
		}
	}
	return NULL;
}

YBUS_CLIENT *ybus_add_client(YBUS_CONNECT *conn,CLIENT_ID client_id,size_t child)
{
	YBUS_PLUGIN *plugin=conn->plugin;
	YBUS_CLIENT *client;
	conn->alive=ybus_now();
	client=ybus_find_client(conn,client_id);
	if(client) return client;
	client=l_alloc0(sizeof(YBUS_CLIENT)+child);
	if(plugin->copy_client_id)
		client->id=plugin->copy_client_id(client_id);
	else
		client->id=client_id;
	client->x=POSITION_ORIG;
	client->y=POSITION_ORIG;
	conn->client=l_slist_prepend(conn->client,client);
	return client;
}

void ybus_free_client(YBUS_CONNECT *conn,YBUS_CLIENT *client)
{
	YBUS_PLUGIN *plugin=conn->plugin;
	if(!conn || !client)
		return;
	if(conn==conn_active && client==conn->active)
	{
		YongShowInput(0);
		YongShowMain(0);
		YongResetIM();
	}
	conn->client=l_slist_remove(conn->client,client);
	if(plugin->free_client_id)
		plugin->free_client_id(client->id);
	l_free(client);
	if(client==conn->active)
		conn->active=NULL;
}

int ybus_get_active(YBUS_CONNECT **conn,YBUS_CLIENT **client)
{
	if(!conn_active) return -1;
	if(conn) *conn=conn_active;
	if(!conn_active->active) return -1;
	if(client) *client=conn_active->active;
	return 0;
}

void *ybus_get_priv(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;	
	conn=ybus_find_connect(plugin,conn_id);
	if(!conn) return 0;
	client=ybus_find_client(conn,client_id);
	if(!client) return 0;
	return client->priv;
}

static void wm_notify_state(void *param)
{
	if(!conn_wm)
		return;
	YBUS_CONNECT *conn;
	int state=0;
	int ret=ybus_get_active(&conn,NULL);
	if(ret==0)
		state=conn->state;
	conn_wm->plugin->wm_state(conn_wm->id,state);
	if(wm_icon[0])
		conn_wm->plugin->wm_icon(conn_wm->id,wm_icon[0],wm_icon[1]?wm_icon[1]:wm_icon[0]);
}

int ybus_on_open(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	
	conn=ybus_find_connect(plugin,conn_id);
	if(!conn)
	{
		return 0;
	}
	conn->state=1;
	client=ybus_find_client(conn,client_id);
	if(!client)
	{
		return 0;
	}
	client->state=1;
	YongResetIM();
	YongSetLang(def_lang);
	if(conn_active==NULL)
	{
		conn_active=conn;
	}
	if(conn->active==NULL)
	{
		// gtk3 have a bug in dialog focus
		conn->active=client;
	}
	if(conn==conn_active && client==conn->active)
	{
		YongShowInput(1);
		YongShowMain(1);
	}
	wm_notify_state(NULL);	
	return 0;
}

int ybus_on_close(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	conn=ybus_find_connect(plugin,conn_id);
	if(!conn)
	{
		return 0;
	}
	conn->state=0;
	client=ybus_find_client(conn,client_id);
	if(!client)
	{
		return 0;
	}
	client->state=0;
	if(conn==conn_active)
	{
		YongShowInput(0);
		YongShowMain(0);
		YongResetIM();
	}
	wm_notify_state(NULL);
	return 0;
}

int ybus_on_focus_in(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	time_t now;
	
	conn=ybus_find_connect(plugin,conn_id);
	//printf("focus in %s:%p:%p\n",plugin->name,conn,(void*)client_id);
	if(!conn)
	{
		return 0;
	}
	if(conn!=conn_active)
		YongResetIM();
	now=ybus_now();
	conn->alive=now;
	conn_active=conn;
	conn->focus=1;
	client=ybus_find_client(conn,client_id);
	if(!client)
	{
		return 0;
	}
	conn->active=client;
	if(!client->state && conn->state && plugin->open_im)
	{
		plugin->open_im(conn_id,client_id);
	}
	YongShowInput(conn->state);
	YongShowMain(conn->state);
	if(client->track)
		YongMoveInput(client->x,client->y);
	ybus_recycle_connect(now);
	wm_notify_state(NULL);
	
	return 0;
}

int ybus_on_focus_out(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	
	conn=ybus_find_connect(plugin,conn_id);
	//printf("focus out %s:%p:%p\n",plugin->name,conn,(void*)client_id);
	if(!conn)
		return 0;
	if(conn_active!=conn)
		return 0;
	client=ybus_find_client(conn,client_id);
	if(!client) return 0;
	if(client!=conn->active)
		return 0;
	conn_active=NULL;
	conn->focus=0;
	conn->active=NULL;
	YongShowInput(0);
	YongShowMain(0);
	if(plugin->preedit_clear)
		plugin->preedit_clear(conn_id,client_id);
	wm_notify_state(NULL);
	return 0;
}

int ybus_on_key(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id,int key)
{
	return y_im_input_key(key);
}

int ybus_on_cursor(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id,int x,int y,int rel)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client,*active=NULL;
	conn=ybus_find_connect(plugin,conn_id);
	if(!conn)
		return -1;
	client=ybus_find_client(conn,client_id);
	if(!client)
		return -1;
	if(!rel)
	{
		client->track=1;
		client->x=x;
		client->y=y;
		client->rel=0;
	}
	else
	{
		client->rel=1;
		if(wm_focus.valid)
		{
			client->track=1;
			client->x=x+wm_focus.x;
			client->y=y+wm_focus.y;
		}
		else
		{
			client->track=0;
		}
	}
	if(client->track)
	{
		ybus_get_active(NULL,&active);
		if(active==client)
		{
			//fprintf(stderr,"move %d %d\n",client->x,client->y);
			YongMoveInput(client->x,client->y);
		}
	}
	return 0;
}

int ybus_on_tool(YBUS_PLUGIN *plugin,CONN_ID conn_id,int type,int param)
{
	int res=0;
	switch(type){
	case YBUS_TOOL_SET_LANG:
	{
		int ret;
		YBUS_CONNECT *conn;
		YBUS_CLIENT *client;
		// fprintf(stderr,"set lang %d\n",param);
		if(param!=LANG_CN && param!=LANG_EN)
			break;
		ret=ybus_get_active(&conn,&client);
		if(ret!=0) break;
		if(conn->state==0) break;
		if(conn->lang==param) break;
		YongSetLang(param);
		res=1;
		// fprintf(stderr,"changed\n");
		break;
	}
	case YBUS_TOOL_GET_LANG:
	{
		int ret;
		YBUS_CONNECT *conn;
		YBUS_CLIENT *client;
		ret=ybus_get_active(&conn,&client);
		if(ret!=0) return -1;
		if(conn->state==0) return -1;
		res=conn->lang;
		break;
	}
	case YBUS_TOOL_SET_WM:
	{
		YBUS_CONNECT *conn;
		conn=ybus_find_connect(plugin,conn_id);
		if(!conn)
			break;
		conn_wm=conn;
		res=getpid();
		y_ui_idle_add(wm_notify_state,NULL);
		break;
	}
	case YBUS_TOOL_TRIGGER:
	{
		int ret;
		YBUS_CONNECT *conn;
		YBUS_CLIENT *client;
		ret=ybus_get_active(&conn,&client);
		if(ret!=0) return -1;
		if(param==-1){
			param=!conn->state;
		}
		if(param==0)
			ybus_on_close(plugin,conn->id,client->id);
		else if(param==1)
			ybus_on_open(plugin,conn->id,client->id);
		res=conn->state;
		break;
	}
	case YBUS_TOOL_CONFIG:
	{
		y_im_setup_config();
		break;
	}
	case YBUS_TOOL_WM_FOCUS:
	{
		if(param==-1)
		{
			wm_focus.valid=0;
			break;
		}
		int ox=wm_focus.x,oy=wm_focus.y;
		wm_focus.y=(int)(int16_t)(param>>16&0xffff);
		wm_focus.x=(int)(int16_t)(param&0xffff);
		wm_focus.valid=1;
		YBUS_CLIENT *client=NULL;
		ybus_get_active(NULL,&client);
		if(client!=NULL && client->track && client->rel)
		{
			client->x=client->x-ox+wm_focus.x;
			client->y=client->y-oy+wm_focus.y;
			YongMoveInput(client->x,client->y);
		}
		//fprintf(stderr,"focus %d %d\n",wm_focus.x,wm_focus.y);
		break;
	}
	case YBUS_TOOL_RELOAD_ALL:
	{
		YongReloadAll();
		break;
	}
	case YBUS_TOOL_KEYBOARD:
	{
		y_kbd_select(param,0);
		break;
	}
	default:
		break;
	}
	return res;
}

int ybus_wm_ready(void)
{
	return conn_wm?1:0;
}

void ybus_wm_icon(const char *icon1,const char *icon2)
{
	l_free(wm_icon[0]);
	wm_icon[0]=l_path_resolve(icon1);
	l_free(wm_icon[1]);
	wm_icon[1]=l_path_resolve(icon2);
	if(!conn_wm)
		return;
	if(!wm_icon[0])
		return;
	conn_wm->plugin->wm_icon(conn_wm->id,wm_icon[0],wm_icon[1]?wm_icon[1]:wm_icon[0]);
}

static int process_compare(const int *v1,const int *v2)
{
	return *v1-*v2;
}

#ifdef _WIN32
static int get_process_list(int list[],int max)
{
	BOOL ret;
	DWORD bytes;
	int count;
	ret=EnumProcess((DWORD*)list,max*sizeof(int),&bytes);
	if(!ret) return 0;
	count=(int)bytes/sizeof(DWORD);
	qsort(list,count,sizeof(int),(LCmpFunc)process_compare);
	return count;
}
#else

static int process_filter(const struct dirent *d)
{
	const char *p;
	size_t len;
	if(!(d->d_type & DT_DIR))
		return 0;
	p=d->d_name;
	len=strspn(p,"0123456789");
	if(p[len]!=0)
		return 0;
	return 1;
}

static int get_process_list(int list[],int max)
{
	struct dirent **namelist;
	int count,i,n;
	n=scandir("/proc",&namelist,process_filter,NULL);
	if(n<0) return 0;
	count=MIN(n,max);
	for(i=0;i<n;i++)
	{
		struct dirent *d=namelist[i];
		if(i<max)
			list[i]=atoi(d->d_name);
		free(d);
	}
	free(namelist);
	qsort(list,count,sizeof(int),(LCmpFunc)process_compare);
	return count;
}
#endif

static void ybus_recycle_connect(int64_t now)
{
	int list[4096];
	int count;
	YBUS_CONNECT *p,*n;
	
	if(!(now>last_recycle+60000 || now<last_recycle))
	{
		return;
	}
	last_recycle=now;
	count=get_process_list(list,L_ARRAY_SIZE(list));
	if(count<=0 || count>=L_ARRAY_SIZE(list))
		return;
	for(p=conn_list;p!=NULL;p=n)
	{
		YBUS_PLUGIN *plugin=p->plugin;
		n=p->next;
		if(p->alive<now && p->alive+60000>now)
			continue;
		p->alive=now;
		if(p->pid==0)
		{
			if(plugin->getpid)
			{
				p->pid=plugin->getpid(p->id);
				if(p->pid==0)
					continue;
			}
		}
		if(bsearch(&p->pid,list,count,sizeof(int),(LCmpFunc)process_compare))
			continue;
		ybus_free_connect(p);
	}
}

static CONNECT_ID xim_ybus_id={
	.x=POSITION_ORIG,
	.y=POSITION_ORIG
};

int xim_ybus_init(void)
{
	ybus_init();
	return ybus_init_plugins();
}

void xim_ybus_destroy(void)
{
	ybus_destroy();
}

void xim_ybus_enable(int enable)
{
	YBUS_CONNECT *conn=conn_active;
	YBUS_PLUGIN *plugin;
	if(!conn || !conn->active)
		return;
	if(enable==conn->state)
		return;
	if(enable==-1)
		enable=!conn->state;
	plugin=conn->plugin;
	if(enable)
	{
		if(!plugin->open_im)
			return;
		plugin->open_im(conn->id,conn->active->id);
	}
	else
	{
		if(!plugin->close_im)
			return;
		plugin->close_im(conn->id,conn->active->id);
	}
}

int xim_ybus_trigger_key(int key)
{
	YBUS_PLUGIN *plugin;
	
	for(plugin=plugin_list;plugin;plugin=plugin->next)
	{
		if(!plugin->config) continue;
		plugin->config(0,0,"trigger",key);
	}
	return 0;
}

CONNECT_ID *xim_ybus_get_connect(void)
{
	CONNECT_ID *id=&xim_ybus_id;
	if(!conn_active || !conn_active->active)
	{
		return NULL;
	}
	id->state=conn_active->state;
	id->lang=conn_active->lang;
	id->track=conn_active->active->track;
	id->biaodian=conn_active->biaodian;
	id->trad=conn_active->trad;
	id->focus=conn_active->focus;
	id->corner=conn_active->corner;
	return id;
}

void xim_ybus_put_connect(CONNECT_ID *id)
{
	if(id->dummy)
	{
		YBUS_CONNECT *p;
		for(p=conn_list;p!=NULL;p=p->next)
		{
			p->corner=id->corner;
			p->lang=id->lang;
			p->biaodian=id->biaodian;
			p->trad=id->trad;
		}
		return;
	}
	if(!conn_active)
	{
		return;
	}
	conn_active->corner=id->corner;
	conn_active->lang=id->lang;
	conn_active->biaodian=id->biaodian;
	conn_active->trad=id->trad;
}

void xim_ybus_forward_key(int key,int repeat)
{
	YBUS_CONNECT *conn=conn_active;
	YBUS_PLUGIN *plugin;
	if(!conn || !conn->active)
		return;
	plugin=conn->plugin;
	if(!plugin->send_key)
		return;
	do{
		plugin->send_key(conn->id,conn->active->id,key);
	}while(--repeat>0);
}

void xim_ybus_send_string(const char *s,int flags)
{
	YBUS_CONNECT *conn=conn_active;
	YBUS_PLUGIN *plugin;
	if(!conn || !conn->active)
		return;
	plugin=conn->plugin;
	plugin->send_string(conn->id,conn->active->id,s,flags);
}

int xim_ybus_preedit_clear(void)
{
	YBUS_CONNECT *conn=conn_active;
	YBUS_PLUGIN *plugin;
	//if(!onspot) return 0;
	if(!conn || !conn->active)
		return -1;
	plugin=conn->plugin;
	if(!plugin->preedit_clear)
		return -1;
	plugin->preedit_clear(conn->id,conn->active->id);
	return 0;
}

int xim_ybus_preedit_draw(const char *s,int len)
{
	YBUS_CONNECT *conn=conn_active;
	YBUS_PLUGIN *plugin;
	char temp[512];
	//if(!onspot) return 0;
	if(!conn || !conn->active)
		return -1;
	plugin=conn->plugin;
	if(!plugin->preedit_draw)
		return -1;
	y_im_str_encode_r(s,temp);
	plugin->preedit_draw(conn->id,conn->active->id,temp);
	return 0;
}

static void upload_clipboard_cb(int code)
{
	if(code==0)
		y_ui_show_tip(YT("上传成功"));
	else
		y_ui_show_tip(YT("上传失败"));
}

/*
static void download_clipboard_cb(int code)
{
	if(code==0)
		y_ui_show_tip(YT("下载成功"));
	else
		y_ui_show_tip(YT("下载失败"));
}*/
static void download_clipboard_cb(const char *text,void *user)
{
	(void)user;
	if(text==NULL)
	{
		y_ui_show_tip(YT("下载失败"));
		return;
	}
	if(!y_ui.set_select)
	{
		y_ui_show_tip("不支持剪贴板粘贴");
		return;
	}
	y_ui.set_select(text);
	y_ui_show_tip(YT("下载成功"));
}

static void xim_action(const char *s)
{
	if(!strcmp(s,"copyCloud"))
	{
		y_im_run_helper("yong-config --sync --upload-clipboard",NULL,NULL,upload_clipboard_cb);
	}
	else if(!strcmp(s,"pasteCloud"))
	{
		char *argv[]={"yong-config","--sync","--download-clipboard",NULL};
		y_im_async_spawn(argv,download_clipboard_cb,NULL);
		// y_im_run_helper("yong-config --sync --download-clipboard",NULL,NULL,download_clipboard_cb);
	}
}

static void xim_explore_url(const char *s)
{
	if(l_str_has_prefix(s,"action:"))
	{
		xim_action(s+7);
		return;
	}
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

int y_xim_init_default(Y_XIM *x)
{
	int ybus_xim_init(void);
	int ybus_lcall_init(void);
	int ybus_wayland_init(void);
	int ybus_ibus_init(void);
	
	ybus_xim_init();
	ybus_lcall_init();
	ybus_ibus_init();


	x->init=xim_ybus_init;
	x->destroy=xim_ybus_destroy;
	x->enable=xim_ybus_enable;
	x->forward_key=xim_ybus_forward_key;
	x->trigger_key=xim_ybus_trigger_key;
	x->send_string=xim_ybus_send_string;
	x->preedit_clear=xim_ybus_preedit_clear;
	x->preedit_draw=xim_ybus_preedit_draw;
	x->get_connect=xim_ybus_get_connect;
	x->put_connect=xim_ybus_put_connect;
	x->update_config=xim_ybus_update_config;
	x->explore_url=xim_explore_url;
	x->name="ybus";
	return 0;
}
