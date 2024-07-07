#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include "llib.h"

static inline int gb_is_gb2312(const uint8_t *s)
{
	return s[0]<=0xFE && s[0]>=0xA1 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline int gb_is_gbk_ext(const uint8_t *s)
{
	return (s[0]<=0xFE && s[0]>=0x81 && s[1]<=0xFE && s[1]>=0x40 && s[1]!=0x7F);
}

static inline int gb_is_gbk(const void *s)
{
	return gb_is_gb2312(s) || gb_is_gbk_ext(s);
}

static inline int gb_is_gb18030_ext(const void *p)
{
	const uint8_t *s=p;
	return s[0]<=0xFE && s[0]>=0x81 && s[1]<=0x39 && s[1]>=0x30 &&
		s[2]<=0xFE && s[2]>=0x81 && s[3]<=0x39 && s[3]>=0x30;
}

static inline bool is_recursive(const L_ESCAPE_CONFIG *config)
{
	return config->surround[0] != 0 && config->surround[1] != 0  && config->surround[0] != config->surround[1];
}

static inline char escape_item_find_r(const L_ESCAPE_CONFIG *config,uint8_t c)
{
	if(c>=0x80)
		return 0;
	if(c==config->lead)
		return config->lead;
	if(config->surround[0] && (c==config->surround[0] || c==config->surround[1]))
		return c;
	if(config->env[0] && (c==config->env[0] || c==config->env[1] || c==config->env[2]))
		return c;
	if(c==config->sep)
		return config->sep;
	for(int i=0;i<config->count;i++)
	{
		if(config->map[i].to == c)
			return config->map[i].from;
	}
	return 0;
}

static int l_str_splice_inplace(char *s,int size,int start,int num,const char *item)
{
	if(!item)
	{
		strcpy(s+start,s+start+num);
		return 0;
	}
	int item_len=strlen(item);
	if(item_len==num)
	{
		memcpy(s+start,item,item_len);
		return item_len;
	}
	if(item_len<num)
	{
		memcpy(s+start,item,item_len);
		memmove(s+start+item_len,s+start+num,strlen(s+start+num)+1);
		return item_len;
	}
	int left=strlen(s+start+num)+1;
	if(start+item_len+left>size)
		return item_len;
	memmove(s+start+item_len,s+start+num,left);
	memcpy(s+start,item,item_len);
	return item_len;
}

void *l_unescape(const void *in, char *out, int size, const L_ESCAPE_CONFIG *config)
{
	const uint8_t *p=in;
	bool recursive = p[0]==config->surround[0] && is_recursive(config);
	int surround=0;
	int pos=0;
	while(*p!=0)
	{
		uint32_t c;
		if((config->flags&L_ESCAPE_GB)!=0)
		{
			if(gb_is_gbk(p))
			{
				if(size>0 && pos+2>=size)
					return NULL;
				if(out)
				{
					out[pos++]=*p++;
					out[pos++]=*p++;
				}
				else
				{
					pos+=2;
					p+=2;
				}
				continue;
			}
			else if(gb_is_gb18030_ext(p))
			{
				if(size>0 && pos+2>=size)
					return NULL;
				if(out)
				{
					out[pos++]=*p++;
					out[pos++]=*p++;
					out[pos++]=*p++;
					out[pos++]=*p++;
				}
				else
				{
					pos+=4;
					p+=4;
				}
				continue;
			}
		}
		c=*p++;
		if(size>0 && pos+1>=size)
			return NULL;
		if(c==config->env[0])
		{
			if(*p!=config->env[1])
			{
				goto LEAD;
			}
			c=*p++;
			const char *end=strchr((const char*)p,config->env[2]);
			if(!end)
				return NULL;
			int len=(int)(size_t)(end-(const char*)p);
			if(len<=1)
				return NULL;
			char name[len+1];
			memcpy(name,p,len);
			name[len]=0;
			char env[256];
			char *val=l_getenv(name,env,sizeof(env));
			if(val && (config->flags&L_ESCAPE_GB)!=0)
			{
				char temp[256];
				l_utf8_to_gb(env,temp,sizeof(temp));
				strcpy(val,temp);
			}
			if((const void*)in==(const void*)out)
			{
				int start=pos;
				int num=(int)(size_t)(end-out-pos+1);
				int val_len=l_str_splice_inplace(out,size,start,num,val);
				if(val_len<0)
					return NULL;
				p=(const uint8_t*)out+start+val_len;
				pos=start+val_len;
			}
			else
			{
				if(val)
				{
					int val_len=strlen(val);
					if(pos+val_len>=size)
						return NULL;
					memcpy(out+pos,val,val_len);
					p+=len+1;
					pos+=val_len;
				}
			}
		}
		else if(c==config->lead)
		{
			char n;
LEAD:
			n=*p++;
			if(n==0)
			{
				if(out)
					out[pos]=c;
				pos++;
				break;
			}
			char from=escape_item_find_r(config,n);
			if(from)
			{
				if(from==L_ESCAPE_HEX)
				{
					if(!isxdigit(p[0]))
						return NULL;
					char temp[4]={p[0]};
					if(isxdigit(p[1]))
					{
						temp[1]=p[1];
						p+=2;
					}
					else
					{
						p++;
					}
					if(out)
						out[pos]=(uint8_t)strtol(temp,NULL,16);
					pos++;
				}
				else
				{
					if(out)
						out[pos]=from;
					pos++;
				}
				continue;
			}
			if((config->flags&L_ESCAPE_NEXT)!=0)
			{
				if(out)
					out[pos]=n;
				pos++;
			}
			else
			{
				if(out)
				{
					out[pos++]=c;
					out[pos++]=n;
				}
				else
				{
					pos+=2;
				}
			}
		}
		else if(c==config->sep)
		{
			if((config->flags&L_ESCAPE_LAST)!=0)
			{
				pos=0;
			}
			else
			{
				break;
			}
		}
		else if(c==config->surround[0])
		{
			if(recursive)
			{
				if(surround>0)
				{
					if(out)
						out[pos]=c;
					pos++;
				}
				surround++;
			}
		}
		else if(c==config->surround[1])
		{
			if(recursive)
			{
				surround--;
				if(surround>0)
				{
					if(out)
						out[pos]=c;
					pos++;
				}
				else if(surround<=0)
					break;
			}
		}
		else
		{
			if(out)
				out[pos]=c;
			pos++;
		}
	}
	if(out)
		out[pos]=0;
	return (void*)p;
}

#ifdef USE_UNESCAPE_ARRAY
char **l_unescape_array(const void *in, const L_ESCAPE_CONFIG *config)
{
	const int size=256;
	char out[size];
	const uint8_t *p=in;
	bool recursive = p[0]==config->surround[0] && is_recursive(config);
	int surround=0;
	int pos=0;
	LPtrArray arr=L_PTR_ARRAY_INIT;
	while(*p!=0)
	{
		uint32_t c;
		if((config->flags&L_ESCAPE_GB)!=0)
		{
			if(gb_is_gbk(p))
			{
				if(pos+2>=size)
					goto ERR;
				out[pos++]=*p++;
				out[pos++]=*p++;
				continue;
			}
			else if(gb_is_gb18030_ext(p))
			{
				if(pos+2>=size)
					goto ERR;
				out[pos++]=*p++;
				out[pos++]=*p++;
				out[pos++]=*p++;
				out[pos++]=*p++;
				continue;
			}
		}
		c=*p++;
		if(pos+1>=size)
			goto ERR;
		if(c==config->env[0])
		{
			if(*p!=config->env[1])
			{
				goto LEAD;
			}
			c=*p++;
			const char *end=strchr((const char*)p,config->env[2]);
			if(!end)
				goto ERR;
			int len=(int)(size_t)(end-(const char*)p);
			if(len<=1)
				goto ERR;
			char name[len+1];
			memcpy(name,p,len);
			name[len]=0;
			char env[256];
			char *val=l_getenv(name,env,sizeof(env));
			if(val && (config->flags&L_ESCAPE_GB)!=0)
			{
				char temp[256];
				l_utf8_to_gb(env,temp,sizeof(temp));
				strcpy(val,temp);
			}
			if((const void*)in==(const void*)out)
			{
				int start=pos;
				int num=(int)(size_t)(end-out-pos+1);
				int val_len=l_str_splice_inplace(out,size,start,num,val);
				if(val_len<0)
					goto ERR;
				p=(const uint8_t*)out+start+val_len;
				pos=start+val_len;
			}
		}
		else if(c==config->lead)
		{
LEAD:
			char n=*p++;
			if(n==0)
			{
				out[pos++]=c;
				break;
			}
			char from=escape_item_find_r(config,n);
			if(from)
			{
				if(from==L_ESCAPE_HEX)
				{
					if(!isxdigit(p[0]))
						goto ERR;
					char temp[4]={p[0]};
					if(isxdigit(p[1]))
					{
						temp[1]=p[1];
						p+=2;
					}
					else
					{
						p++;
					}
					out[pos]=(uint8_t)strtol(temp,NULL,16);
					pos++;
				}
				else
				{
					out[pos++]=from;
				}
				continue;
			}
			if((config->flags&L_ESCAPE_NEXT)!=0)
			{
				out[pos++]=n;
			}
			else
			{
				out[pos++]=c;
				out[pos++]=n;
			}
		}
		else if(c==config->sep)
		{
			out[pos]=0;
			l_ptr_array_append(&arr,l_strdup(out));
			pos=0;
		}
		else if(c==config->surround[0])
		{
			if(recursive)
			{
				if(surround>0)
					out[pos++]=c;
				surround++;
			}
		}
		else if(c==config->surround[1])
		{
			if(recursive)
			{
				surround--;
				if(surround>0)
					out[pos++]=c;
				else if(surround<=0)
				{
					out[pos]=0;
					l_ptr_array_append(&arr,l_strdup(out));
					break;
				}
			}
		}
		else
		{
			out[pos++]=c;
		}
	}
	l_ptr_array_append(&arr,NULL);
	char **res=(char **)arr.ptr;
	return res;
ERR:
	l_ptr_array_clear(&arr,l_free);
	l_free(arr.data);
	return NULL;
}
#endif

static inline char escape_item_find(const L_ESCAPE_CONFIG *config,uint32_t c)
{
	if(c>=0x80)
		return 0;
	if(c==config->lead)
		return config->lead;
	if(config->surround[0] && (c==config->surround[0] || c==config->surround[1]))
		return c;
	if(config->env[0] && (c==config->env[0] || c==config->env[1] || c==config->env[2]))
		return c;
	if(c==config->sep)
		return config->sep;
	for(int i=0;i<config->count;i++)
	{
		if(config->map[i].from == c)
			return config->map[i].to;
	}
	return 0;
}

int l_escape(const void *in, char *out, int size, const L_ESCAPE_CONFIG *config)
{
	int pos=0;
	const uint8_t *p=in;
	while(*p!=0)
	{
		uint32_t c;
		if((config->flags&L_ESCAPE_GB)!=0)
		{
			if(gb_is_gbk(p))
			{
				if(pos+2>=size)
					return -1;
				out[pos++]=*p++;
				out[pos++]=*p++;
				continue;
			}
			else if(gb_is_gb18030_ext(p))
			{
				if(pos+4>=size)
					return -1;
				out[pos++]=*p++;
				out[pos++]=*p++;
				out[pos++]=*p++;
				out[pos++]=*p++;
				continue;
			}
		}
		c=*p++;
		int to=escape_item_find(config,c);
		if(to==0)
		{
			if(pos+1>=size)
				return -1;
			out[pos++]=c;
		}
		else
		{
			if(pos+2>=size)
				return -1;
			out[pos++]=config->lead;
			out[pos++]=to;
		}
	}
	out[pos]=0;
	return pos;
}


int encodeURIComponent(const char *in,char *out,int size)
{
	int i,c,pos=0;
	pos=0;
	for(i=0;(c=in[i])!=0 && pos<size-1;i++)
	{
		if((c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z') || strchr("-_.!~*'()",c))
		{
			out[pos++]=c;
		}
		else
		{
			if(pos>=size-3)
				break;
			pos+=sprintf(out+pos,"%%%02X",(unsigned char)c);
		}
	}
	out[pos]=0;
	return pos;
}
