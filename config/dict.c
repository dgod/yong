#ifndef CFG_XIM_ANDROID

#include <llib.h>
#include "config_ui.h"
#include "custom_dict.c"

#ifdef __linux__
#include <glib.h>
#endif

struct dict_item{
	void *next;
	char *key;
	uint32_t pos;
};

struct y_dict{
	char *file;
	LHashTable * index;	/* index of the dict */
};
static struct y_dict *l_dict;

static void dict_item_free(struct dict_item *p)
{
	if(!p) return;
	l_free(p->key);
	l_free(p);
}

static struct y_dict *y_dict_open(const char *file)
{
	char *content=l_file_get_contents(file,NULL,y_im_get_path("HOME"),
			y_im_get_path("DATA"),NULL);
	if(!content)
		return NULL;
	struct y_dict *dict=l_new(struct y_dict);
	dict->file=content;
	dict->index=L_HASH_TABLE_STRING(struct dict_item,key,7001);
	for(int next=1,pos=0;;)
	{
		char line[512];
		char *p=strpbrk(content+pos,"\r\n");
		if(!p)
			break;
		int current=pos;
		int len=(int)(size_t)(p-content-pos);
		if(len>=512)
			break;
		l_strncpy(line,content+pos,len);
		pos=(int)(size_t)(p-content+1);
		if(content[pos]=='\r')
			pos++;
		if(content[pos]=='\n')
			pos++;
		if(len==0)
		{
			next=1;
			continue;
		}
		if(next==0) continue;
		char key[64];
		len=0;
		for(int i=0;i<63;i++)
		{
			if(isspace(line[i]))
				break;
			if(line[i]=='$' && line[i+1]=='_')
				key[len++]=' ';
			else
				key[len++]=line[i];
		}
		key[len]=0;
		struct dict_item *item=l_new(struct dict_item);
		item->next=NULL;
		item->key=l_strdup(key);
		item->pos=current;
		item=l_hash_table_replace(dict->index,item);
		dict_item_free(item);
		next=0;
	}
	return dict;
}

static void y_dict_close(struct y_dict *dict)
{
	if(!dict)
		return;
	l_hash_table_free(dict->index,(LFreeFunc)dict_item_free);
	l_free(dict->file);
	l_free(dict);
}

char *y_dict_query(struct y_dict *dic,const char *s)
{
	if(!dic)
		return NULL;
	char temp[8192];
	l_utf8_to_gb(s,temp,sizeof(temp));
	struct dict_item *item=l_hash_table_lookup(dic->index,temp);
	if(!item)
		return NULL;
	const char *begin=dic->file+item->pos;
	const char *end=strstr(begin,"\n\n");
	if(!end)
		end=strstr(begin,"\r\n\r\n");
	if(!end)
	{
		l_gb_to_utf8(begin,temp,sizeof(temp));
	}
	else
	{
		char *gb=l_strndup(begin,(size_t)(end-begin));
		l_gb_to_utf8(gb,temp,sizeof(temp));
		l_free(gb);
	}
	return l_strdup(temp);
}

static int y_dict_query_network(const char *s)
{
	char url[256];
	char temp[256];
	int len=l_gb_strlen(s,-1);
	if(len>64)
		return -1;
	int eng=len==strlen(s);
	encodeURIComponent(s,temp,sizeof(temp));
	char *site=l_key_file_get_string(config,"IM",eng?"dict_en":"dict_cn");
	if(site)
	{
		sprintf(url,site,temp);
		l_free(site);
	}
	else
	{
		site=eng?"https://www.iciba.com/word?w=%s":
				"https://www.zdic.net/hans/%s";
		sprintf(url,site,temp);
	}
#ifdef _WIN32
	ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOW);
#else
	char *args[]={"xdg-open",url,0};
	g_spawn_async(NULL,args,NULL,
		G_SPAWN_SEARCH_PATH|G_SPAWN_STDOUT_TO_DEV_NULL,
		0,0,0,0);
#endif
	return 0;
}

int DictLocal(CUCtrl p,int arc,char **arg)
{
	CUCtrl root=p->parent?p->parent:p;
	CUCtrl name=cu_ctrl_from_group(root,"query");
	CUCtrl result=cu_ctrl_from_group(root,"result");
	char *query_val=cu_ctrl_get_self(name);
	if(!query_val[0])
	{
		l_free(query_val);
		return 0;
	}
	char *result_val=y_dict_query(l_dict,query_val);
	l_free(query_val);
	if(!result_val)
	{
		cu_ctrl_set_self(result,"");
		return 0;
	}
	cu_ctrl_set_self(result,result_val);
	l_free(result_val);
	return 0;
}

int DictNetwork(CUCtrl p,int arc,char **arg)
{
	CUCtrl root=p->parent;
	CUCtrl name=cu_ctrl_from_group(root,"query");
	char *query_val=cu_ctrl_get_self(name);
	if(!query_val[0])
	{
		l_free(query_val);
		return 0;
	}
	y_dict_query_network(query_val);
	l_free(query_val);
	return 0;
}

static void activate(CULoopArg *arg)
{
	CUCtrl win=cu_ctrl_new(NULL,arg->custom->root.child);
	assert(win!=NULL);
	cu_ctrl_show_self(win,1);
	arg->win=win;
	l_dict=y_dict_open("dict.txt");
}

static void cmdline(CULoopArg *arg)
{
	for(int i=0;i<arg->argc-1;i++)
	{
		if(!strcmp(arg->argv[i],"--query"))
		{
			CUCtrl name=cu_ctrl_from_group(arg->win,"query");
			cu_ctrl_set_self(name,arg->argv[i+1]);
			DictLocal(arg->win,0,NULL);
			break;
		}
	}
}

int DictMain(int argc,char *argv[])
{
	cu_init();
	LXml *custom=l_xml_load((const char*)config_dict);
	CULoopArg loop_arg={custom,"net.dgod.yong.dict",argc,argv,cmdline};
	cu_loop((void*)activate,&loop_arg);
	l_xml_free(custom);
	y_dict_close(l_dict);
	return 0;
}

#endif // CFG_XIM_ANDROID
