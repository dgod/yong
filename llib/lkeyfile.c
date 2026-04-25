#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "llib.h"

typedef struct _keyvalue{
	struct _keyvalue *next;
	char *value;
	char key[];
}KeyValue;
static_assert(sizeof(KeyValue)==2*sizeof(void*),"KeyValue size not compacted");

struct _lkeyfile{
	char *file;
	bool dirty;
	char inherit;
	KeyValue *line;
	struct _lkeyfile *overlay;
};

static KeyValue *kv_create(const char *key,const char *value)
{
	int key_len=key?strlen(key):0;
	KeyValue *kv=l_alloc(sizeof(KeyValue)+key_len+1);
	kv->value=value?l_strdup(value):NULL;
	if(key)
		strcpy(kv->key,key);
	else
		kv->key[0]=0;
	return kv;
}

LKeyFile *l_key_file_open(const char *file,int create,...)
{
	LKeyFile *key_file=NULL;
	size_t length;
	va_list ap;
	va_start(ap,create);
	char *data=l_file_vget_contents(file,&length,ap);
	va_end(ap);
	if(data!=NULL)
	{
		key_file=l_key_file_load(data,(ssize_t)length);
		l_free(data);
		key_file->file=l_strdup(file);
	}
	else if(create!=0)
	{
		key_file=l_new0(LKeyFile);
		key_file->file=l_strdup(file);
	}
	return key_file;
}

LKeyFile *l_key_file_load(const char *data,ssize_t length)
{
	if(length==-1)
		length=strlen(data);
	const char *end=data+length;
	if(length>=3 && !memcmp(data,"\xef\xbb\xbf",3))
		data+=3;
	LKeyFile *key_file=l_new0(LKeyFile);		
	while(data<end)
	{
		char line[1024];
		int i;
		for(i=0;i<sizeof(line)-1 && data<end;)
		{
			int c=*data++;
			if(c=='\r') continue;
			if(c=='\n')	break;
			line[i++]=c;
		}
		line[i]=0;
		char *p=l_str_trim(line);
		KeyValue *kv;
		if(p[0]=='[')
		{
			char *d=strchr(p+1,']');
			if(!d) continue;
			*d=0;
			kv=kv_create(p+1,NULL);
		}
		else if(p[0]=='#' || p[0]==';' || !p[0])
		{
			kv=kv_create(NULL,p);
		}
		else
		{
			char *d=strchr(p,'=');
			if(!d || d==p) continue;
			*d++=0;
			kv=kv_create(l_str_trim_right(p),l_str_trim_left(d));
		}
		key_file->line=l_slist_append(key_file->line,kv);
	}
	return key_file;
}

static void l_keyvalue_free(KeyValue *k)
{
	l_free(k->value);
	l_free(k);
}

void l_key_file_free(LKeyFile *key_file)
{
	if(!key_file) return;
	l_free(key_file->file);
	l_slist_free(key_file->line,(LFreeFunc)l_keyvalue_free);
	l_free(key_file);
}

int l_key_file_save(LKeyFile *key_file,const char *path)
{
	if(!key_file || !key_file->file)
	{
		// printf("bad key file\n");
		return -1;
	}
	if(!key_file->dirty)
	{
		return 0;
	}
	FILE *fp=l_file_open(key_file->file,"w",path,0);
	if(!fp)
	{
		// printf("open key file fail: %s\n",key_file->file);
		return -1;
	}
	fprintf(fp,"\xef\xbb\xbf");
	for(KeyValue *p=key_file->line;p!=NULL;p=p->next)
	{
		if(!p->value)
			fprintf(fp,"[%s]\n",p->key);
		else if(!p->key[0])
			fprintf(fp,"%s\n",p->value);
		else
			fprintf(fp,"%s=%s\n",p->key,p->value);
	}
	fclose(fp);
	key_file->dirty=false;
	return 0;
}

static KeyValue *get_group(LKeyFile *key_file,const char *group)
{
	for(KeyValue *p=key_file->line;p!=NULL;p=p->next)
	{
		if(p->value) continue;
		if(!strcmp(p->key,group))
			return p;
	}
	return NULL;
}

const char *l_key_file_get_data(LKeyFile *key_file,const char *group,const char *key)
{
	KeyValue *p;
	KeyValue *g=NULL;

	if(!key_file || !group || !key)
		return NULL;

	if(key_file->overlay)
	{
		const char *res=l_key_file_get_data(key_file->overlay,group,key);
		if(res!=NULL)
			return res;
	}

PARENT:
	g=get_group(key_file,group);
	if(!g)
	{
		return NULL;
	}

	for(p=g->next;p!=NULL;p=p->next)
	{
		if(!p->value) break;
		if(!strcmp(p->key,key))
		{
			return p->value;
		}
	}

	if(key_file->inherit)
	{
		const char *p=strrchr(group,key_file->inherit);
		if(p!=NULL)
		{
			group=l_strndupa(group,(int)(size_t)(p-group));
			goto PARENT;
		}
	}

	return NULL;
}

char *l_key_file_get_string(LKeyFile *key_file,const char *group,const char *key)
{
	char temp[256];
	int i,pos,c;
	const char *data=l_key_file_get_data(key_file,group,key);
	if(!data)
		return NULL;
	for(i=pos=0;pos<250 && (c=data[i])!=0;i++)
	{
		if(c=='\\')
		{
			c=data[++i];	
			switch(c){
			case '\\':
				temp[pos++]='\\';
				break;
			case 's':
				temp[pos++]=' ';
				break;
			case 'n':
				temp[pos++]='\n';
				break;
			case 'r':
				temp[pos++]='\r';
				break;
			case 't':
				temp[pos++]='\t';
				break;
			case '\'':
				temp[pos++]='\'';
				break;
			case '\"':
				temp[pos++]='\"';
				break;
			default:
				temp[pos++]='\\';
				temp[pos++]=c;
				break;
			}
		}
		else
		{
			temp[pos++]=c;
		}		
	}
	temp[pos]=0;
	return l_strdup(temp);
}

char *l_key_file_get_string_gb(LKeyFile *key_file,const char *group,const char *key)
{
	char *s=l_key_file_get_string(key_file,group,key);
	if(!s)
		return NULL;
	if(l_str_is_ascii(s))
		return s;
	char temp[256];
	l_utf8_to_gb(s,temp,sizeof(temp));
	l_free(s);
	return l_strdup(temp);
}

int l_key_file_get_int(LKeyFile *key_file,const char *group,const char *key)
{
	const char *data=l_key_file_get_data(key_file,group,key);
	if(!data)
		return 0;
	return atoi(data);
}

int l_key_file_set_data(LKeyFile *key_file,const char *group,const char *key,const char *value)
{
	KeyValue *p;
	KeyValue *prev;
	
	if(!group)
		return -1;
	KeyValue *g=get_group(key_file,group);
	if(unlikely(!key))
	{
		if(!g) return 0;
		for(p=g->next;p!=NULL;p=g->next)
		{
			if(!p->value) break;
			g->next=p->next;
			l_keyvalue_free(p);
		}
		key_file->line=l_slist_remove(key_file->line,g);
		l_keyvalue_free(g);
		key_file->dirty=true;
		return 0;
	}
	if(!g)
	{
		if(!key) return 0;
		g=kv_create(group,NULL);
		key_file->line=l_slist_append(key_file->line,g);
		key_file->dirty=true;
	}

	for(p=g,prev=NULL;p->next!=NULL;prev=p,p=p->next)
	{
		if(!p->next->value) break;
		if(!p->next->key[0]) continue;
		if(!strcmp(p->next->key,key))
		{
			if(value)
			{
				p=p->next;
				if(strcmp(p->value,value))
				{
					l_free(p->value);
					p->value=l_strdup(value);
					key_file->dirty=true;
				}
			}
			else
			{
				g=p->next;
				p->next=g->next;
				l_keyvalue_free(g);
				key_file->dirty=true;
			}
			return 0;
		}
	}
	if(value)
	{
		if(prev && !p->key[0])
			p=prev;
		g=kv_create(key,value);
		g->next=p->next;
		p->next=g;
		key_file->dirty=true;
	}	

	return 0;
}

int l_key_file_set_string(LKeyFile *key_file,const char *group,const char *key,const char *value)
{
	char temp[256];
	int i,pos,c;

	if(!key || !value)
	{
		return l_key_file_set_data(key_file,group,key,value);
	}

	for(i=pos=0;pos<254 && (c=value[i])!=0;i++)
	{
		switch(c){
		case '\\':
			temp[pos++]='\\';temp[pos++]='\\';
			break;
		case '\'':
			temp[pos++]='\\';temp[pos++]='\'';
			break;
		case '\"':
			temp[pos++]='\\';temp[pos++]='\"';
			break;
		case '\n':
			temp[pos++]='\\';temp[pos++]='\n';
			break;
		case '\r':
			temp[pos++]='\\';temp[pos++]='\r';
			break;		

		default:
			temp[pos++]=c;
			break;
		}
	}
	temp[pos]=0;
	return l_key_file_set_data(key_file,group,key,temp);
}

int l_key_file_set_int(LKeyFile *key_file,const char *group,const char *key,int value)
{
	char temp[32];
	sprintf(temp,"%d",value);
	return l_key_file_set_data(key_file,group,key,temp);
}

void l_key_file_set_dirty(LKeyFile *key_file)
{
	key_file->dirty=true;
}

bool l_key_file_get_dirty(LKeyFile *key_file)
{
	return key_file->dirty;
}

bool l_key_file_has_group(LKeyFile *key_file,const char *group)
{
	KeyValue *g=get_group(key_file,group);
	return g!=NULL;
}

const char *l_key_file_get_start_group(LKeyFile *key_file)
{
	for(KeyValue *g=key_file->line;g!=NULL;g=g->next)
	{
		if(g->value) continue;
		return g->key;
	}
	return NULL;
}

char **l_key_file_get_groups(LKeyFile *key_file)
{
	LPtrArray list=L_PTR_ARRAY_INIT;
	KeyValue *p;
	
	for(p=key_file->line;p!=NULL;p=p->next)
	{
		if(p->value) continue;
		if(!p->key[0]) continue;
		l_ptr_array_append(&list,l_strdup(p->key));
	}
	l_ptr_array_append(&list,NULL);
	return (char**)list.ptr;
}

char **l_key_file_get_keys(LKeyFile *key_file,const char *group)
{
	KeyValue *p=get_group(key_file,group);
	if(!p) return NULL;
	LPtrArray list=L_PTR_ARRAY_INIT;
	for(p=p->next;p!=NULL;p=p->next)
	{
		if(!p->value) break;
		if(!p->key[0]) continue;
		l_ptr_array_append(&list,l_strdup(p->key));
	}
	l_ptr_array_append(&list,NULL);
	return (char**)list.ptr;
}

void l_key_file_set_inherit(LKeyFile *key_file,char delimiter)
{
	key_file->inherit=delimiter;
}

void l_key_file_set_overlay(LKeyFile *key_file,LKeyFile *overlay)
{
	key_file->overlay=overlay;
}
