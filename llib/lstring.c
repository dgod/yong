#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <inttypes.h>

#include "ltypes.h"
#include "lmem.h"
#include "lstring.h"
#include "lconv.h"
#include "ltricky.h"

char *l_sprintf(const char *fmt,...)
{
	va_list ap;
	int len;
	char *ret;
	va_start(ap,fmt);
#if 1
	len=vasprintf(&ret,fmt,ap);
	if(len<0) ret=NULL;
#else
	len=vsnprintf(NULL,0,fmt,ap);
#ifdef _WIN32
	/* win2000 always return -1, so we set 256 to work */
	if(len<=0) len=256;
#endif
	va_end(ap);
	ret=l_alloc(len+1);
	va_start(ap,fmt);
	vsprintf(ret,fmt,ap);
	
#endif
	va_end(ap);
	return ret;
}

char **l_strsplit(const char *str,int delimiter)
{
	char **list;
	int count;
	const char *p=str;

	count=2;
	do{
		p=strchr(p,delimiter);
		if(p!=NULL)
		{
			count++;
			p++;
			if(delimiter==0 && *p==0)
			{
				break;
			}
		}
	}while(p!=NULL);
	list=l_cnew(count,char*);
	
	for(count=0;;)
	{
		p=strchr(str,delimiter);
		if(!p)
		{
			list[count++]=l_strdup(str);
			break;
		}
		size_t len=p-str;
		list[count++]=l_strndup(str,len);
		str=p+1;
		if(delimiter==0 && *str==0)
		{
			break;
		}
	}
	list[count++]=NULL;
	return list;
}

void l_strfreev(char **list)
{
	int i;
	if(!list)
		return;
	for(i=0;list[i]!=NULL;i++)
	{
		l_free(list[i]);
	}
	l_free(list);
}

#ifndef EMSCRIPTEN
#if defined(_WIN32) || (!defined(__GLIBC__)/* && !defined(__BIONIC__)*/)
char *l_stpcpy(char *dest,const char *src)
{
	do{
		*dest++=*src;
	}while(*src++!='\0');
	return dest-1;
}
#endif
#endif

char *l_strjoinv(const char *sep,char **list)
{
	int i,len;
	char *string,*ptr;
	if(!list || !list[0])
		return l_strdup("");
	if(!sep) sep="";
	for(i=len=0;list[i]!=NULL;i++)
		len+=strlen(list[i]);
	if(i>0) len+=(i-1)*strlen(sep);
	len++;
	string=l_alloc(len);
	ptr=l_stpcpy(string,*list);
	for(i=1;list[i]!=NULL;i++)
	{
		ptr=l_stpcpy(ptr,sep);
		ptr=l_stpcpy(ptr,list[i]);
	}
	return string;
}

int l_strv_length(char **list)
{
	int i;
	if(!list) return 0;
	for(i=0;list[i]!=NULL;i++);
	return i;
}

LString *l_string_new(int size)
{
	LString *string;
	string=l_new0(LString);
	if(size>0)
	{
		string->str=l_alloc(size);
		string->size=size;
	}
	return string;
}

void l_string_free(LString *string)
{
	if(!string) return;
	l_free(string->str);
	l_free(string);
}

void l_string_expand(LString *string,int len)
{
	int v1,v2;
	if(string->size-string->len>=len+1)
		return;
	v1=string->len+len+1;
	v2=string->size*5/4;
	string->size=v1>v2?v1:v2;
	string->str=l_realloc(string->str,string->size);
}

void l_string_append(LString *string,const char *val,int len)
{
	if(len<0) len=strlen(val);
	l_string_expand(string,len);
	if(val)
	{
		memcpy(string->str+string->len,val,len);
		string->str[string->len+len]=0;
	}
	string->len+=len;
}

void l_string_append_c(LString *string,int c)
{
	l_string_expand(string,1);
	string->str[string->len++]=c;
	string->str[string->len]=0;
}

void *l_strncpy(char *restrict dest,const char *restrict src,size_t n)
{
	int i;
	for(i=0;i<n;i++)
	{
		int c=src[i];
		if(!c)
			break;
		dest[i]=c;
	}
	dest[i]=0;
	return dest;
}

#ifdef _WIN32
#undef l_strndup
void *l_strndup(const void *p,size_t n)
{
	char *r=l_alloc(n+1);
	return l_strncpy(r,p,n);
}
#endif

void l_strup(char *s)
{
	register int c;
	while((c=*s)!='\0')
	{
		if(c>='a' && c<='z')
			*s='A'+c-'a';
		s++;
	}
}

void l_strdown(char *s)
{
	register int c;
	while((c=*s)!='\0')
	{
		if(c>='A' && c<='Z')
			*s=c-'A'+'a';
		s++;
	}
}

#if 0
#ifndef _WIN32
#include <dlfcn.h>
static int (*p_vsscanf)(const char *buf,const char *fmt,va_list ap);
#endif
int l_sscanf(const char * buf, const char * fmt, ...)
{
	va_list args;
	int i;
	
	va_start(args,fmt);
#ifndef _WIN32
	if(!p_vsscanf)
	{
		p_vsscanf=dlsym(0,"vsscanf");
#ifndef __GLIBC__
		if(!p_vsscanf) p_vsscanf=vsscanf;
#endif
	}
	i=p_vsscanf(buf,fmt,args);
#else
	i = vsscanf(buf,fmt,args);
#endif
	va_end(args);
	return i;
}

#else

int l_sscanf(const char * buf, const char * fmt, ...)
{
	va_list args;
	int i;
	va_start(args,fmt);
	i=vsscanf(buf,fmt,args);
	va_end(args);
	return i;
}

#endif

bool l_str_has_prefix(const char *str,const char *prefix)
{
	int len1=strlen(str),len2=strlen(prefix);
	if(len1<len2) return false;
	return memcmp(str,prefix,len2)?false:true;
}

bool l_str_has_suffix(const char *str,const char *suffix)
{
	int len1=strlen(str),len2=strlen(suffix);
	if(len1<len2) return false;
	return memcmp(str+len1-len2,suffix,len2)?false:true;
}

char *l_str_trim_left(char *str)
{
	int i;
	for(i=0;str[i]!=0;i++)
	{
		char c=str[i];
		if((c&0x80) || !isspace(c))
			break;
	}
	if(i!=0)
		memmove(str,str+i,strlen(str+i)+1);
	return str;
}

char *l_str_trim_right(char *str)
{
	int len=strlen(str);
	for(len--;len>=0;len--)
	{
		char c=str[len];
		if((c&0x80) || !isspace(c))
			break;
		str[len]=0;
	}
	return str;
}

char *l_str_trim(char *str)
{
	return l_str_trim_left(l_str_trim_right(str));
}

int l_strcpy(char *dest,int dest_size,const char *src)
{
	int i;
	if(!dest || !src || dest_size<=0)
	{
		return -1;
	}
	for(i=0;i<dest_size-1;i++)
	{
		int c=src[i];
		if(!c)
			break;
		dest[i]=c;
	}
	dest[i]=0;
	return i;
}

#if defined(_GNU_SOURCE) && defined(__linux__)
void *l_memmem(const void *haystack,int haystacklen,const void *needle,int needlelen)
{
	return memmem(haystack,haystacklen,needle,needlelen);
}
#else
void *l_memmem(const void *haystack,int haystacklen,const void *needle,int needlelen)
{
	if(needlelen==0)
		return (void*)haystack;
	if(haystacklen<needlelen)
		return NULL;
	const char *begin;
	const char *last_possible=(const char*)haystack+haystacklen-needlelen;
	for(begin=haystack;begin<last_possible;begin++)
	{
		if(begin[0]==((const char*)needle)[0] && !memcmp(begin,needle,needlelen))
			return (void*)begin;
	}
	return NULL;
}
#endif

int l_mempos(const void *haystack,int haystacklen,const void *needle,int needlelen)
{
	const void *r=l_memmem(haystack,haystacklen,needle,needlelen);
	if(!r)
		return -1;
	return (int)(size_t)((const char*)r-(const char*)haystack);
}

int l_strpos(const char *haystack,const char *needle)
{
	const char *p=strstr(haystack,needle);
	if(!p)
		return -1;
	return (int)(size_t)(p-haystack);
}

int l_str_replace(char *s,int from,int to)
{
	int i=0;
	if(to==0)
	{
		for(int j=0;s[j]!=0;j++)
		{
			if(s[j]!=from)
				s[i++]=s[j];
		}
		s[i]=0;
		return i;
	}
	else
	{
		for(i=0;s[i]!=0;i++)
		{
			if(s[i]==from)
				s[i]=to;
		}
		return i;
	}
}

#undef L_INT2STR_UTF8
#undef L_INT2STR_INDIRECT

#define L_INT2STR_UTF8			0x02
#define L_INT2STR_INDIRECT		0x10

int l_int_to_str(int64_t n,const char *format,int flags,char *out)
{
	if(format)
	{
		if(!strcmp(format,"%d"))
			format="%"PRId64;
		if(!strcmp(format,"%02d"))
			format="%02"PRId64;
	}
	if((flags&L_INT2STR_MINSEC)!=0)
	{
		if(n<0 || n>59)
			return -1;
	}
	if((flags&(L_INT2STR_MINSEC|L_INT2STR_HZ)) == (L_INT2STR_MINSEC|L_INT2STR_HZ))
	{
		flags&=~L_INT2STR_MINSEC;
		if(n<10)
		{
			format="%02"PRId64;
		}
		else
		{
			flags|=L_INT2STR_INDIRECT;
		}
		
	}
	if(!(flags&L_INT2STR_HZ))
	{
		return sprintf(out,format?format:"%"PRId64,n);
	}
	const char *ch0=flags&L_INT2STR_ZERO0?"〇":"零";
	const char *ch=flags&L_INT2STR_BIG?"壹贰叁肆伍陆柒捌玖":"一二三四五六七八九";
	const char *dw=NULL;
	if((flags&L_INT2STR_INDIRECT)!=0)
	{
		if((flags&L_INT2STR_BIG)!=0)
			dw=flags&L_INT2STR_TRAD?"拾佰仟萬億":"拾佰仟万亿";
		else
			dw=flags&L_INT2STR_TRAD?"十百千萬億":"十百千万亿";
	}
	int pos=0;
	bool is_utf8=(flags&L_INT2STR_UTF8)!=0;
	flags&=~L_INT2STR_UTF8;
	char *p=is_utf8?l_alloca(64):out;
	if(!(flags&L_INT2STR_INDIRECT))
	{
		char t[32];
		int len=sprintf(t,format?format:"%"PRId64,n);
		for(int i=0;i<len;i++)
		{
			int c=t[i];
			if(c=='0')
			{
				p[pos++]=ch0[0];
				p[pos++]=ch0[1];
			}
			else
			{
				p[pos++]=ch[(c-'1')*2+0];
				p[pos++]=ch[(c-'1')*2+1];
			}
		}
	}
	else
	{
		int strip10=!(flags&L_INT2STR_KEEP10) && n<20 && n>=10;	// 是否忽略十前面的一
		int have_value=0;			// 之前是否有值
		int prev_value=0;			// 上一位是否有值
		// 亿
		if(n/100000000)
		{
			pos+=l_int_to_str(n/100000000,format,flags|L_INT2STR_NEST,p+pos);
			p[pos++]=dw[8];
			p[pos++]=dw[9];
			have_value=1;
			prev_value=n/100000000%10;
			n%=100000000;
		}
		if(have_value && !prev_value && n)
		{
			p[pos++]=ch0[0];
			p[pos++]=ch0[1];
			have_value=0;
		}
		// 万
		if(n/10000)
		{
			pos+=l_int_to_str(n/10000,format,flags|L_INT2STR_NEST,p+pos);
			p[pos++]=dw[6];
			p[pos++]=dw[7];
			have_value=1;
			prev_value=n/10000%10;
			n%=10000;
		}
		else
		{
			prev_value=0;
		}
		if(have_value && !prev_value && n)
		{
			p[pos++]=ch0[0];
			p[pos++]=ch0[1];
			have_value=0;
		}
		// 千
		if(n/1000)
		{
			pos+=l_int_to_str(n/1000,format,flags|L_INT2STR_NEST,p+pos);
			p[pos++]=dw[4];
			p[pos++]=dw[5];
			have_value=1;
			prev_value=n/1000%10;
			n%=1000;
		}
		else
		{
			prev_value=0;
		}
		if(have_value && !prev_value && n)
		{
			p[pos++]=ch0[0];
			p[pos++]=ch0[1];
			have_value=0;
		}
		// 百
		if(n/100)
		{
			pos+=l_int_to_str(n/100,format,flags|L_INT2STR_NEST,p+pos);
			p[pos++]=dw[2];
			p[pos++]=dw[3];
			have_value=1;
			prev_value=n/100%10;
			n%=100;
		}
		else
		{
			prev_value=0;
		}
		if(have_value && !prev_value && n)
		{
			p[pos++]=ch0[0];
			p[pos++]=ch0[1];
			have_value=0;
		}
		// 十
		if(n/10)
		{
			if(n/10!=1 || !strip10)
				pos+=l_int_to_str(n/10,format,flags|L_INT2STR_NEST,p+pos);
			p[pos++]=dw[0];
			p[pos++]=dw[1];
			have_value=1;
			prev_value=n/10%10;
			n%=10;
		}
		else
		{
			prev_value=0;
		}
		if(have_value && !prev_value && n)
		{
			p[pos++]=ch0[0];
			p[pos++]=ch0[1];
			have_value=0;
		}
		if(n || (pos==0 && !(flags&L_INT2STR_NEST)))
		{
			if(n==0)
			{
				p[pos++]=ch0[0];
				p[pos++]=ch0[1];
			}
			else
			{
				p[pos++]=ch[(n-1)*2+0];
				p[pos++]=ch[(n-1)*2+1];
			}
		}
	}
	p[pos]=0;
	if(is_utf8)
	{
		l_gb_to_utf8(p,out,64);
		pos=strlen(out);
	}
	return pos;
}

