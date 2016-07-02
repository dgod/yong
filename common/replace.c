#ifndef DCFG_NO_REPLACE

#include "llib.h"
#include "gbk.h"
#include "common.h"

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

static unsigned item_hash(struct item *p)
{
	return p->code;
}

static int item_cmp(const struct item *v1,const struct item *v2)
{
	return (int)(v1->code-v2->code);
}

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
	int i,ret;
	char line[128];
	const uint8_t *p;
	y_replace_free();
	if(!file) return 0;
	fp=y_im_open_file(file,"rb");
	if(!fp) return 0;
	R=l_new0(Y_REPLACE);
	R->all=l_hash_table_new(1023,(LHashFunc)item_hash,(LCmpFunc)item_cmp);
	for(i=0;i<3;)
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
		if(gb_is_gbk((uint8_t*)p))
		{
			code=p[0]|(p[1]<<8);
			p+=2;
		}
		else if(gb_is_gb18030((uint8_t*)p))
		{
			code=p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24);
			p+=4;
		}
		else if(gb_is_ascii((uint8_t*)p))
		{
			code=p[0];
			p++;
		}
		else
		{
			break;
		}
		if(p[0]!=' ')
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
	middle=gb_strchr((const uint8_t*)entry,'?');
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
		orig[0]=(it->code>>0)&0xff;
		orig[1]=(it->code>>8)&0xff;
		orig[2]=(it->code>>16)&0xff;
		orig[3]=(it->code>>24)&0xff;
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
		while(p[0]!=0)
		{
			if(gb_is_gbk(p))
			{
				key.code=p[0]|(p[1]<<8);
				p+=2;
			}
			else if(gb_is_gb18030(p))
			{
				key.code=p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24);
				p+=4;
			}
			else
			{
				key.code=p[0];
				p++;
			}
			it=l_hash_table_find(R->all,&key);
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

int y_replace_enable(int enable)
{
	if(!R) return 0;
	if(enable==-1)
		R->enable=!R->enable;
	else
		R->enable=enable;
	return 0;
}

#endif/*CFG_NO_REPLACE*/
