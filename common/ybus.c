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

#ifndef _WIN32
#include <dirent.h>
#endif

static YBUS_PLUGIN *plugin_list;
static YBUS_CONNECT *conn_list;
static YBUS_CONNECT *conn_active;
static int onspot;
static int def_lang;
static time_t last_recycle;

int ybus_init(void)
{
	return 0;
}

void ybus_destroy(void)
{
	while(conn_list)
		ybus_free_connect(conn_list);
	conn_active=NULL;
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

static void xim_ybus_update_config(void)
{
	def_lang=y_im_get_config_int("IM","lang");
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

static void ybus_recycle_connect(time_t now);

YBUS_CONNECT *ybus_add_connect(YBUS_PLUGIN *plugin,CONN_ID conn_id)
{
	YBUS_CONNECT *conn;
	time_t now=time(NULL);
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
	
	ybus_recycle_connect(now);
	return conn;
}

void ybus_free_connect(YBUS_CONNECT *conn)
{
	YBUS_PLUGIN *plugin;
	if(!conn) return;
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
	conn->alive=time(NULL);
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
	return 0;
}

int ybus_on_focus_in(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	time_t now;
	
	conn=ybus_find_connect(plugin,conn_id);
	if(!conn)
	{
		return 0;
	}
	if(conn!=conn_active)
		YongResetIM();
	now=time(NULL);
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
	YongMoveInput(client->x,client->y);
	ybus_recycle_connect(now);
	
	return 0;
}

int ybus_on_focus_out(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id)
{
	YBUS_CONNECT *conn;
	YBUS_CLIENT *client;
	
	conn=ybus_find_connect(plugin,conn_id);
	if(!conn) return 0;
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
	return 0;
}

int ybus_on_key(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id,int key)
{
	return y_im_input_key(key);
}

int ybus_on_tool(YBUS_PLUGIN *plugin,int type,int param)
{
	int res=0;
	switch(type){
	case YBUS_TOOL_SET_LANG:
	{
		int ret;
		YBUS_CONNECT *conn;
		YBUS_CLIENT *client;
		if(param!=LANG_CN && param!=LANG_EN)
			break;
		ret=ybus_get_active(&conn,&client);
		if(ret!=0) break;
		if(conn->state==0) break;
		if(conn->lang==param) break;
		YongSetLang(param);
		res=1;
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
	}
	default:
		break;
	}
	return res;
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
	n=scandir("/proc",&namelist,process_filter,alphasort);
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
	return count;
}
#endif

static void ybus_recycle_connect(time_t now)
{
	int list[4096];
	int count;
	YBUS_CONNECT *p,*n;
	
	if(!(now>last_recycle+60 || now<last_recycle))
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
		if(p->alive<now && p->alive+60>now)
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

void xim_ybus_forward_key(int key)
{
	YBUS_CONNECT *conn=conn_active;
	YBUS_PLUGIN *plugin;
	if(!conn || !conn->active)
		return;
	plugin=conn->plugin;
	if(!plugin->send_key)
		return;
	plugin->send_key(conn->id,conn->active->id,key);
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

int y_xim_init_default(Y_XIM *x)
{
	int ybus_xim_init(void);
	int ybus_lcall_init(void);
	int ybus_wayland_init(void);
	
	ybus_xim_init();
	ybus_lcall_init();


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
