#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "ltypes.h"
#include "lmem.h"
#include "lkeyfile.h"
#include "lslist.h"
#include "lfile.h"
#include "lmacros.h"
#include "larray.h"
#include "lstring.h"

typedef struct _keyvalue{
	struct _keyvalue *next;
	char *key;
	char *value;
}KeyValue;

struct _lkeyfile{
	char *file;
	int utf8;
	int dirty;
	KeyValue *line;
};

/*
LKeyFile *l_key_file_open(const char *file,int create,...)
{
	LKeyFile *key_file;
	
	key_file=l_new0(LKeyFile);
	if(file)
	{
		FILE *fp;
		va_list ap;
		key_file->utf8=1;
		key_file->file=l_strdup(file);
		va_start(ap,create);
		fp=l_file_vopen(file,"r",ap,NULL);
		va_end(ap);
		if(fp)
		{
			KeyValue *kv;
			char line[1024];
			int count;
			for(count=0;l_get_line(line,1024,fp)>=0;count++)
			{
				char *p;
				if(count==0 && !memcmp(line,"\xef\xbb\xbf",3))
				{
					int len=strlen(line+3)+1;
					memmove(line,line+3,len);
					key_file->utf8=1;
				}
				for(p=line;isspace(*p);p++);
				if(p[0]=='[')
				{
					char *d=strchr(p+1,']');
					if(!d) continue;*d=0;
					kv=l_new0(KeyValue);
					kv->key=l_strdup(p+1);
				}
				else if(p[0]=='#' || p[0]==';' || !p[0])
				{
					kv=l_new0(KeyValue);
					kv->value=l_strdup(p);
				}
				else
				{
					char *d=strchr(p,'=');
					if(!d) continue;*d++=0;
					kv=l_new(KeyValue);
					kv->key=l_strdup(p);
					for(p=kv->key;*p && !isspace(*p);p++);*p=0;
					for(p=d;!(*p&0x80) && isspace(*p);p++);
					kv->value=l_strdup(p);
				}
				key_file->line=l_slist_append(key_file->line,kv);
			}
			fclose(fp);
		}
		else if(!create)
		{
			l_free(key_file->file);
			l_free(key_file);
			key_file=NULL;
		}
	}
	return key_file;
}
*/

LKeyFile *l_key_file_open(const char *file,int create,...)
{
	LKeyFile *key_file=NULL;
	char *data;
	size_t length;
	va_list ap;
	va_start(ap,create);
	data=l_file_vget_contents(file,&length,ap);
	va_end(ap);
	if(data!=NULL)
	{
		key_file=l_key_file_load(data,length);
		l_free(data);
		key_file->file=l_strdup(file);
	}
	else if(create!=0)
	{
		key_file=l_new0(LKeyFile);
		key_file->utf8=1;
		key_file->file=l_strdup(file);
	}
	return key_file;
}

LKeyFile *l_key_file_load(const char *data,size_t length)
{
	LKeyFile *key_file;
	KeyValue *kv;
	const char *end=data;
	char line[1024];
	int count;
	
	if(length==-1)
		length=strlen(data);
	end=data+length;
	
	key_file=l_new0(LKeyFile);
	key_file->utf8=1;
	key_file->file=NULL;
		
	for(count=0;data<end;count++)
	{
		int i;
		char *p;

		for(i=0;i<sizeof(line)-1 && data<end;)
		{
			int c=*data++;
			if(c=='\r') continue;
			if(c=='\n')
			{
				break;
			}
			line[i++]=c;
		}
		line[i]=0;
		l_str_trim_right(line);
		if(count==0 && !memcmp(line,"\xef\xbb\xbf",3))
		{
			int len=strlen(line+3)+1;
			memmove(line,line+3,len);
			key_file->utf8=1;
		}
		for(p=line;isspace(*p);p++);
		if(p[0]=='[')
		{
			char *d=strchr(p+1,']');
			if(!d) continue;
			*d=0;
			kv=l_new0(KeyValue);
			kv->key=l_strdup(p+1);
		}
		else if(p[0]=='#' || p[0]==';' || !p[0])
		{
			kv=l_new0(KeyValue);
			kv->value=l_strdup(p);
		}
		else
		{
			char *d=strchr(p,'=');
			if(!d) continue;
			*d++=0;
			kv=l_new(KeyValue);
			kv->key=l_strdup(p);
			for(p=kv->key;*p && !isspace(*p);p++)
			{
			}
			*p=0;
			for(p=d;!(*p&0x80) && isspace(*p);p++);
			kv->value=l_strdup(p);
		}
		key_file->line=l_slist_append(key_file->line,kv);
	}
	return key_file;
}

static void l_keyvalue_free(void *data)
{
	KeyValue *k=data;
	l_free(k->key);
	l_free(k->value);
	l_free(k);
}

void l_key_file_free(LKeyFile *key_file)
{
	if(!key_file) return;
	l_free(key_file->file);
	l_slist_free(key_file->line,l_keyvalue_free);
	l_free(key_file);
}

int l_key_file_save(LKeyFile *key_file,const char *path)
{
	FILE *fp;
	KeyValue *p;
	if(!key_file || !key_file->file)
	{
		printf("bad key file\n");
		return -1;
	}
	if(!key_file->dirty)
	{
		//printf("not dirty\n");
		return 0;
	}
	fp=l_file_open(key_file->file,"w",path,0);
	if(!fp)
	{
		printf("open key file fail\n");
		return -1;
	}
	if(key_file->utf8)
	{
		fprintf(fp,"\xef\xbb\xbf");
	}
	for(p=key_file->line;p!=NULL;p=p->next)
	{
		if(!p->value)
			fprintf(fp,"[%s]\n",p->key);
		else if(!p->key)
			fprintf(fp,"%s\n",p->value);
		else
			fprintf(fp,"%s=%s\n",p->key,p->value);
	}
	fclose(fp);
	key_file->dirty=0;
	return 0;
}

const char *l_key_file_get_data(LKeyFile *key_file,const char *group,const char *key)
{
	KeyValue *p;
	KeyValue *g=NULL;
	

	for(p=key_file->line;p!=NULL;p=p->next)
	{
		if(p->value) continue;
		if(!strcmp(p->key,group))
		{
			g=p;
			break;
		}
	}
	if(!g) return 0;

	for(p=p->next;p!=NULL;p=p->next)
	{
		if(!p->value) break;
		if(!p->key) continue;
		if(!strcmp(p->key,key))
		{
			return p->value;
		}
	}

	return 0;
}

char *l_key_file_get_string(LKeyFile *key_file,const char *group,const char *key)
{
	char temp[256];
	int i,pos,c;
	const char *data=l_key_file_get_data(key_file,group,key);
	if(!data) return 0;
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

int l_key_file_get_int(LKeyFile *key_file,const char *group,const char *key)
{
	const char *data=l_key_file_get_data(key_file,group,key);
	if(!data) return 0;
	return atoi(data);
}

int l_key_file_set_data(LKeyFile *key_file,const char *group,const char *key,const char *value)
{
	KeyValue *p;
	KeyValue *g=NULL;
	KeyValue *prev;
	
	if(!group) return -1;
	for(p=key_file->line;p!=NULL;p=p->next)
	{
		if(p->value) continue;
		if(!strcmp(p->key,group))
		{
			g=p;
			break;
		}
	}
	if(L_UNLIKELY(!key))
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
		key_file->dirty++;
		return 0;
	}
	if(!g)
	{
		if(!key) return 0;
		g=l_new(KeyValue);
		g->value=0;
		g->key=l_strdup(group);
		key_file->line=l_slist_append(key_file->line,g);
		key_file->dirty++;
	}

	for(p=g,prev=NULL;p->next!=NULL;prev=p,p=p->next)
	{
		if(!p->next->value) break;
		if(!p->next->key) continue;
		if(!strcmp(p->next->key,key))
		{
			if(value)
			{
				p=p->next;
				if(strcmp(p->value,value))
				{
					l_free(p->value);
					p->value=l_strdup(value);
					key_file->dirty++;
				}
			}
			else
			{
				g=p->next;
				p->next=g->next;
				l_keyvalue_free(g);
				key_file->dirty++;
			}
			return 0;
		}
	}
	if(value)
	{
		if(prev && !p->key)
			p=prev;
		g=l_new(KeyValue);
		g->key=l_strdup(key);
		g->value=l_strdup(value);
		g->next=p->next;
		p->next=g;
		key_file->dirty++;
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
	key_file->dirty=1;
}

bool l_key_file_has_group(LKeyFile *key_file,const char *group)
{
	KeyValue *g;
	for(g=key_file->line;g!=NULL;g=g->next)
	{
		if(g->value) continue;
		if(!strcmp(g->key,group))
			return true;
	}
	return false;
}

const char *l_key_file_get_start_group(LKeyFile *key_file)
{
	KeyValue *g;
	for(g=key_file->line;g!=NULL;g=g->next)
	{
		if(g->value) continue;
		return g->key;
	}
	return NULL;
}

char **l_key_file_get_keys(LKeyFile *key_file,const char *group)
{
	LArray *list;
	KeyValue *p;
	char **res;

	for(p=key_file->line;p!=NULL;p=p->next)
	{
		if(p->value) continue;
		if(!strcmp(p->key,group))
		{
			break;
		}
	}
	if(!p) return 0;
	list=l_ptr_array_new(8);
	for(p=p->next;p!=NULL;p=p->next)
	{
		if(!p->value) break;
		if(!p->key) continue;
		l_ptr_array_append(list,l_strdup(p->key));
	}
	l_ptr_array_append(list,NULL);
	res=(char**)list->data;
	list->data=NULL;
	list->len=0;
	list->count=0;
	l_ptr_array_free(list,NULL);
	return res;
}
