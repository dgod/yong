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
	FILE *fp;
	LHashTable *index;
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
	FILE *fp=l_file_open(file,"rb",y_im_get_path("HOME"),
			y_im_get_path("DATA"),NULL);
	if(!fp)
		return NULL;
	struct y_dict *dict=l_new(struct y_dict);
	dict->fp=fp;
	dict->index=L_HASH_TABLE_STRING(struct dict_item,key,7001);
	LBuffer *idx_file=NULL;
	uint32_t file_size=(uint32_t)l_filep_size(fp);
	if(file_size>0x1000000)
	{
		size_t length;
		char *content=l_file_get_contents("dict.txt~",&length,y_im_get_path("HOME"),
			y_im_get_path("DATA"),NULL);
		if(content)
		{
			if(length<16 || l_read_u32(content)!=file_size)
			{
				l_free(content);
				goto reindex;
			}
			for(int i=4;i+6<length;)
			{
				uint32_t pos=l_read_u32(content+i);
				i+=4;
				uint8_t len=l_read_u8(content+i);
				i++;
				if(len==0 || i+len>length)
					break;
				struct dict_item *item=l_new(struct dict_item);
				item->key=l_strndup(content+i,len);
				item->pos=pos;
				item=l_hash_table_replace(dict->index,item);
				if(item)
					dict_item_free(item);
				i+=len;
			}
			l_free(content);
			return dict;
		}
reindex:
		idx_file=l_buffer_new(file_size/20);
		l_buffer_append(idx_file,&file_size,4);
	}
	for(bool start_of_entry=true;;)
	{
		char line[512];
		int32_t pos=start_of_entry?ftell(fp):0;
		int len=l_get_line(line,512,fp);
		if(len<0)
			break;
		if(len==0)
		{
			start_of_entry=1;
			continue;
		}
		if(start_of_entry==0)
			continue;
		char key[64];
		len=0;
		for(int i=0;i<63;i++)
		{
			if(isspace(line[i]) || !line[i])
				break;
			if(line[i]=='$' && line[i+1]=='_')
				key[len++]=' ';
			else
				key[len++]=line[i];
		}
		key[len]=0;
		if(idx_file)
		{
			l_buffer_append(idx_file,&pos,4);
			l_buffer_append_b(idx_file,len);
			l_buffer_append(idx_file,key,len);
		}
		struct dict_item *item=l_new(struct dict_item);
		item->key=l_strdup(key);
		item->pos=pos;
		item=l_hash_table_replace(dict->index,item);
		if(item)
			dict_item_free(item);
		start_of_entry=false;
	}
	if(idx_file)
	{
		l_file_set_contents("dict.txt~",idx_file->data,idx_file->len,y_im_get_path("HOME"),NULL);
		l_buffer_free(idx_file);
	}
	return dict;
}

static void y_dict_close(struct y_dict *dict)
{
	if(!dict)
		return;
	l_hash_table_free(dict->index,(LFreeFunc)dict_item_free);
	fclose(dict->fp);
	l_free(dict);
}

char *y_dict_query(struct y_dict *dic,const char *query)
{
	if(!dic)
		return NULL;
	char data[8192],temp[8192];
	l_utf8_to_gb(query,temp,sizeof(temp));
	struct dict_item *item=l_hash_table_lookup(dic->index,temp);
	if(!item)
		return NULL;
	fseek(dic->fp,item->pos,SEEK_SET);
	int len=(int)fread(data,1,8191,dic->fp);
	if(len<1) return NULL;
	data[len]=0;
	char *s=strstr(data,"\n\n");
	if(!s) s=strstr(data,"\r\n\r\n");
	if(s) *s=0;
	l_gb_to_utf8(data,temp,sizeof(temp));
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
		snprintf(url,sizeof(url),site,temp);
		l_free(site);
	}
	else
	{
		site=eng?"https://www.iciba.com/word?w=%s":
				"https://www.zdic.net/hans/%s";
		snprintf(url,sizeof(url),site,temp);
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
	// uint64_t begin=l_ticks();
	l_dict=y_dict_open("dict.txt");
	// printf("load dict %dms\n",(int)(l_ticks()-begin));
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
