#include "ltypes.h"
#include "lgb.h"

#include <string.h>

#define UTF8_LENGTH(c)				\
  ((c) < 0x80 ? 1 :					\
   ((c) < 0x800 ? 2 :				\
    ((c) < 0x10000 ? 3 :			\
     ((c) < 0x200000 ? 4 :			\
      ((c) < 0x4000000 ? 5 : 6)))))

#define UTF16_LENGTH(c)				\
  ((c) < 0x10000 ?2 : 4)

int l_unichar_to_utf8 (uint32_t c,uint8_t *outbuf)
{
	int len = 0;
	int first;
	int i;

	if (c < 0x80)
	{
		first = 0;
		len = 1;
	}
	else if (c < 0x800)
	{
		first = 0xc0;
		len = 2;
	}
	else if (c < 0x10000)
	{
		first = 0xe0;
		len = 3;
	}
	else if (c < 0x200000)
	{
		first = 0xf0;
		len = 4;
	}
	else if (c < 0x4000000)
	{
		first = 0xf8;
		len = 5;
	}
	else
	{
		first = 0xfc;
		len = 6;
	}

	if(outbuf)
	{
		for (i = len - 1; i > 0; --i)
		{
			outbuf[i] = (c & 0x3f) | 0x80;
			c >>= 6;
		}
		outbuf[0] = c | first;
	}

	return len;
}

uint32_t l_utf8_to_unichar(const uint8_t *s)
{
	uint8_t c=s[0];
	int i,len;
	uint32_t res;
	
	if(c<0x80)
	{
		return c;
	}
	else if((c&0xe0)==0xc0)
	{
		len=2;
		res=c&0x1f;
	}
	else if((c&0xf0)==0xe0)
	{
		len=3;
		res=c&0x0f;
	}
	else if((c&0xf8)==0xf0)
	{
		len=4;
		res=c&0x07;
	}
	else if((c&0xfc)==0xf8)
	{
		len=5;
		res=c&0x03;
	}
	else if((c&0xfe)==0xfc)
	{
		len=6;
		res=c&0x01;
	}
	else
	{
		return 0;
	}
	for(i=1;i<len;i++)
	{
		c=s[i];
		if((c&0xc0)!=0x80)
			return 0;
		res<<=6;
		res|=c&0x3f;
	}
	return res;
}

const uint8_t *l_utf8_next_char(const uint8_t *s)
{
	uint8_t c=s[0];
	if(c==0)
	{
		return 0;
	}
	else if(c<0x80)
	{
		return s+1;
	}
	else if((c&0xe0)==0xc0)
	{
		return s+2;
	}
	else if((c&0xf0)==0xe0)
	{
		return s+3;
	}
	else if((c&0xf8)==0xf0)
	{
		return s+4;
	}
	else if((c&0xfc)==0xf8)
	{
		return s+5;
	}
	else if((c&0xfe)==0xfc)
	{
		return s+6;
	}
	else
	{
		return 0;
	}
}

bool l_utf8_validate(const void *s,int len,void **end)
{
	const uint8_t *p=s;
	if(len<0)
		len=(int)strlen(s);
	while(len>0)
	{
		uint8_t c=*p;
		if(!c)
			break;
		if(c<=0x7f)
		{
			p++;
			len--;
		}
		else if(c>=0xc0 && c<=0xdf)
		{
			if(len<2)
				break;
			if((p[1]&0xc0)!=0x80)
				break;
			len-=2;
			p+=2;
		}
		else if(c>=0xe0 && c<=0xef)
		{
			if(len<3)
				break;
			if((p[1]&0xc0)!=0x80)
				break;
			if((p[2]&0xc0)!=0x80)
				break;
			len-=3;
			p+=3;
		}
		else if(c>=0xf0 && c<=0xf7)
		{
			if(len<=4)
				break;
			if((p[1]&0xc0)!=0x80)
				break;
			if((p[2]&0xc0)!=0x80)
				break;
			if((p[3]&0xc0)!=0x80)
				break;
			len-=4;
			p+=4;
		}
		else
		{
			break;
		}
			
	}
	if(end)
		*end=(void*)p;
	return len==0;
}

uint8_t *l_utf8_strncpy(uint8_t *dst,const uint8_t *src,size_t n)
{
	uint8_t *orig=dst;
	uint32_t uc;
	int i;
	int len;
	for(i=0;i<n;i++)
	{
		uc=l_utf8_to_unichar(src);
		len=l_unichar_to_utf8(uc,dst);
		dst+=len;src+=len;
	}
	*dst=0;
	return orig;
}

const uint8_t *l_utf8_offset(const uint8_t *s,int offset)
{
	int i;
	for(i=0;i<offset;i++)
	{
		s=l_utf8_next_char(s);
		if(s==NULL)
			return NULL;
	}
	return s;
}

const uint16_t *l_utf16_next_char(const uint16_t *s)
{
	uint16_t c=s[0];
	if(c==0)
		return 0;
	else if(c>=0xd800 && c<0xdc00)
		return s+2;
	else return s+1;
}

int l_unichar_to_utf16 (uint32_t c,uint16_t *outbuf)
{
	if(c<0x10000)
	{
		outbuf[0]=(uint16_t)c;
		return 2;
	}
	else
	{
		c-=0x10000;
		outbuf[0]=c / 0x400 + 0xd800;
		outbuf[1]=c % 0x400 + 0xdc00;
		return 4;
	}
}

uint32_t l_utf16_to_unichar(const uint16_t *s)
{
	uint16_t c=s[0];
	if(c>=0xd800 && c<0xdc00)
	{
		uint32_t res=0x10000+(c-0xd800)*0x400;
		c=s[1];
		if(c>=0xdc00 && c<0xe000)
		{
			return res+=(c-0xdc00);
		}
		return 0;
	}
	else
	{
		return c;
	}
}

void *l_utf8_to_utf16(const char *s,void *out,int size)
{
	uint16_t *res=out;
	uint32_t t;
	
	while(s && (t=l_utf8_to_unichar((const uint8_t*)s))!=0)
	{
		int space=UTF16_LENGTH(t);
		if(space+2>size) break;
		l_unichar_to_utf16(t,res);
		res+=space>>1;
		size-=space;
		s=(const char*)l_utf8_next_char((const uint8_t*)s);
	}
	res[0]=0;
	return out;
}

char *l_utf16_to_utf8(const void *s,char *out,int size)
{
	uint8_t *res=(uint8_t*)out;
	uint32_t t;
	
	while(s && (t=l_utf16_to_unichar(s))!=0)
	{
		int space=UTF8_LENGTH(t);
		if(space+2>size) break;
		l_unichar_to_utf8(t,res);
		res+=space;
		size-=space;
		s=l_utf16_next_char(s);
	}
	res[0]=0;
	return out;
}

#ifndef USE_SYSTEM_ICONV

char *l_gb_to_utf8(const char *s,char *out,int size)
{
	uint8_t *res=(uint8_t*)out;
	uint32_t t;
	
	while(s && (t=l_gb_to_unichar((const uint8_t*)s))!=0)
	{
		int space=UTF8_LENGTH(t);
		if(space+1>size) break;
		l_unichar_to_utf8(t,res);
		res+=space;
		size-=space;
		s=(const char*)l_gb_next_char((const uint8_t*)s);
	}
	res[0]=0;
	return out;
}

void *l_gb_to_utf16(const char *s,void *out,int size)
{
	uint16_t *res=out;
	uint32_t t;
	
	while(s && (t=l_gb_to_unichar((const uint8_t*)s))!=0)
	{
		int space=UTF16_LENGTH(t);
		if(space+2>size) break;
		l_unichar_to_utf16(t,res);
		res+=space>>1;
		size-=space;
		s=(const char*)l_gb_next_char((const uint8_t*)s);
	}
	res[0]=0;
	return out;
}

char *l_utf8_to_gb(const char *s,char *out,int size)
{
	uint8_t *res=(uint8_t*)out;
	uint32_t t;
	
	while(s && (t=l_utf8_to_unichar((const uint8_t*)s))!=0)
	{
		int space=4;
		if(space+1>size)
		{
			uint8_t temp[5];
			int i;
			space=l_unichar_to_gb(t,temp);
			if(space+1>size) break;
			for(i=0;i<space;i++) res[i]=(char)temp[i];
		}
		else
		{
			space=l_unichar_to_gb(t,res);
		}
		res+=space;
		size-=space;
		s=(const char*)l_utf8_next_char((const uint8_t*)s);
	}
	res[0]=0;
	return out;
}

char *l_utf16_to_gb(const void *s,char *out,int size)
{
	uint8_t *res=(uint8_t*)out;
	uint32_t t;
	
	while(s && (t=l_utf16_to_unichar(s))!=0)
	{
		int space=4;
		if(space+1>size)
		{
			uint8_t temp[5];
			int i;
			space=l_unichar_to_gb(t,temp);
			if(space+1>size) break;
			for(i=0;i<space;i++) res[i]=(char)temp[i];
		}
		else
		{
			space=l_unichar_to_gb(t,res);
		}
		res+=space;
		size-=space;
		s=l_utf16_next_char(s);
	}
	res[0]=0;
	return out;
}
#endif

int l_utf8_strlen(const void *p,int size)
{
	const uint8_t *s=p;
	const uint8_t *end=size<0?LINT_TO_PTR(-1):s+size;
	int count=0;
	while(s<end)
	{
		s=l_utf8_next_char(s);
		if(s==NULL)
			break;
		count++;
	}
	return count;
}

int l_utf16_strlen(const void *p,int size)
{
	const uint16_t *s=p;
	const uint16_t *end=size<0?LINT_TO_PTR(-1):s+(size>>1);
	int count=0;
	while(s<end)
	{
		s=l_utf16_next_char(s);
		if(s==NULL)
			break;
		count++;
	}
	return count;
}

const void *l_utf16_offset(const void *p,int offset)
{
	const uint16_t *s=p;
	int i;
	for(i=0;i<offset;i++)
	{
		s=l_utf16_next_char(s);
		if(s==NULL)
			return NULL;
	}
	return s;
}

