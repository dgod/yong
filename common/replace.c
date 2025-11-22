#ifndef DCFG_NO_REPLACE

#include "llib.h"
#include "gbk.h"
#include "common.h"
#include "translate.h"

#include <stdlib.h>

struct item{
	struct item *next;
	uint32_t code;
	char **list;
};

typedef struct{
	struct item *begin;
	struct item *end;
	struct item *any;
	LHashTable *all;
	int enable;
}Y_REPLACE;

static Y_REPLACE *R;

static void item_free(struct item *p)
{
	if(!p) return;
	l_strfreev(p->list);
	l_free(p);
}

int y_replace_free(void)
{
	if(!R) 
		return 0;
	item_free(R->begin);
	item_free(R->end);
	item_free(R->any);
	l_hash_table_free(R->all,(LFreeFunc)item_free);
	l_free(R);
	R=NULL;
	return 0;
}

int y_replace_init(const char *file)
{
	FILE *fp;
	int ret;
	char line[128];
	const uint8_t *p;
	y_replace_free();
	if(!file) return 0;
	fp=y_im_open_file(file,"rb");
	if(!fp) return 0;
	R=l_new0(Y_REPLACE);
	R->all=L_HASH_TABLE_INT(struct item,code,1023);
	for(int i=0;i<3;)
	{
		ret=l_get_line(line,sizeof(line),fp);
		if(ret<0) break;
		if(ret==0) continue;
		if(line[0]=='#') continue;
		p=(const uint8_t*)line;
		if(p[0]=='^' && p[1]==' ')
		{
			p+=2;
			if(!R->begin)
			{
				R->begin=l_new0(struct item);
				R->begin->list=l_strsplit((const char*)p,' ');
				i++;
			}
		}
		else if(p[0]=='$' && p[1]==' ')
		{
			p+=2;
			if(!R->end)
			{
				R->end=l_new0(struct item);
				R->end->list=l_strsplit((const char*)p,' ');
				i++;
			}
		}
		else if(p[0]=='*' && p[1]==' ')
		{
			p+=2;
			if(!R->any)
			{
				R->any=l_new0(struct item);
				R->any->list=l_strsplit((const char*)p,' ');
				i++;
			}
		}
		else
		{
			goto rule;
		}
	}
	while(ret>=0)
	{
		struct item *it;
		uint32_t code;
		ret=l_get_line(line,sizeof(line),fp);
		if(ret<0) break;
		if(ret==0) continue;
		p=(const uint8_t*)line;
rule:
		code=l_gb_to_char(p);
		p=l_gb_next_char(p);
		if(!p || p[0]!=' ')
			break;
		p++;
		it=l_new0(struct item);
		it->code=code;
		it->list=l_strsplit((const char*)p,' ');
		it=l_hash_table_replace(R->all,it);
		if(it!=NULL) item_free(it);
	}
	fclose(fp);
	if(!R->any)
	{
		R->any=l_new0(struct item);
		R->any->list=l_strsplit("?",' ');
	}
	//printf("%p %p %p %p\n",R->begin,R->end,R->any,R->all);
	return 0;
}

static int replace_item(char *out,int pos,struct item *it,int index,int bie)
{
	int len;
	const char *entry;
	const char *middle;
	
	if(!it)
	{
		return pos;
	}
	len=l_strv_length(it->list);
	if(!len) return pos;
	index=index%len;
	entry=it->list[index];
	middle=l_gb_strchr(entry,'?');
	if(!middle || it==R->begin || it==R->end)
	{
		len=strlen(entry);
		if(len+pos<511)
		{
			strcpy(out+pos,entry);
			pos+=len;
		}
	}
	else
	{
		char orig[8]={0};
		int biaodian;
		l_char_to_gb(it->code,orig);
		len=(int)(size_t)(middle-entry);
		biaodian=gb_is_biaodian((uint8_t*)orig);
		if(len>0 && len+pos<511 && !biaodian)
		{
			memcpy(out+pos,entry,len);
			pos+=len;
			out[pos]=0;
		}
		len=strlen(orig);
		if(len>0 && len+pos<511)
		{
			strcpy(out+pos,orig);
			pos+=len;
		}
		middle++;
		len=strlen(middle);
		if(len>0 && len+pos<511 && !biaodian)
		{
			memcpy(out+pos,middle,len);
			pos+=len;
			out[pos]=0;
		}
	}
	return pos;
}

int y_replace_string(const char *in,void (*output)(const char *,int),int flags)
{
	if(!R || !R->enable)
	{
		output(in,flags);
		return 0;
	}
	else
	{
		char temp[512];
		int pos=0;
		const uint8_t *p=(const uint8_t *)in;
		struct item key,*it;
		int i=rand();
		int bie=0;

		temp[0]=0;
		pos=replace_item(temp,pos,R->begin,i,bie);
		while(p!=NULL)
		{
			key.code=l_gb_to_char(p);
			if(!key.code)
				break;
			p=l_gb_next_char(p);
			it=L_HASH_TABLE_LOOKUP_INT(R->all,key.code);
			if(!it && R->any)
			{
				it=&key;
				key.list=R->any->list;
			}
			if(p[0]==0)
				bie|=2;			
			pos=replace_item(temp,pos,it,i,bie);
			if(bie==0) bie=1;
		}
		pos=replace_item(temp,pos,R->end,i,bie);
		output(temp,flags);
	}
	return 0;
}

bool y_replace_enable(int enable)
{
	if(!R) return false;
	if(enable==-1)
		R->enable=!R->enable;
	else
		R->enable=enable;
	if(R->enable)
		y_ui_show_tip(YT("开启文字替换功能"));
	else
		y_ui_show_tip(YT("关闭文字替换功能"));
	return true;
}

#endif/*CFG_NO_REPLACE*/
