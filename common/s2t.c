#if !defined(CFG_NO_S2T)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "gbk.h"
#include "yong.h"
#include "select.h"
#include "im.h"
#include "common.h"
#include "xim.h"

#include "s2t_char.c"
#include "s2t_phrase.c"
#include "t2s_char.c"
#include "t2s_phrase.c"

//#define memcpy(a,b,c) memmove(a,b,c)

static int s2t_open;
static int s2t_bd;
static int s2t_multi;

static int s2t_get_enable(void)
{
	CONNECT_ID *id;
	id=y_xim_get_connect();
	if(!id) return 0;
	if(id->trad==im.Trad)
		return 0;
	return im.Trad?2:1;
}

void s2t_enable(int b)
{
	s2t_open=b;
}

void s2t_biaodian(int b)
{
	s2t_bd=b;
}

void s2t_multiple(int b)
{
	s2t_multi=b;
}

int s2t_is_enable(void)
{
	return s2t_open;
}
static unsigned hash_func(char *in,int len)
{
	unsigned int ha=0;
	int i;
	for(i=0;i<len && *in;i++)
		ha=ha*33+*(unsigned char*)in++;
	return ha;
}

static int s2t_compar(const void *a,const void *b)
{
	return (int)(*(uint16_t*)a)-(int)(*(uint16_t*)b);	
}

#if 0
static char *adjust_phrase(char *in,int len)
{
	unsigned int ha;
	unsigned short magic;
	int i;
	int pos;
	char *p;
	if(len<=3) return in;
	ha=hash_func(in,len);
	magic=ha>>16;
	for(i=ha%S2T_PHRASE_NUM;(pos=s2t_phrase[i])!=0xffff;i=(i+1)%S2T_PHRASE_NUM)
	{
		p=(char*)s2t_data+pos;
		if(*(unsigned short*)p!=magic) continue;
		p+=2;
		if(*p!=(char)len) continue;
		p+=1;
		if(memcmp(in,p,len)) continue;
		return p+len;
	}
	return in;
}

static char *adjust_phrase_t2s(char *in,int len)
{
	unsigned int ha;
	unsigned short magic;
	int i;
	int pos;
	char *p;
	if(len<=3) return in;
	ha=hash_func(in,len);
	magic=ha>>16;
	for(i=ha%T2S_PHRASE_NUM;(pos=t2s_phrase[i])!=0xffff;i=(i+1)%T2S_PHRASE_NUM)
	{
		p=(char*)t2s_data+pos;
		if(*(unsigned short*)p!=magic) continue;
		p+=2;
		if(*p!=(char)len) continue;
		p+=1;
		if(memcmp(in,p,len)) continue;
		return p+len;
	}
	return in;
}
#endif

static int adjust_phrase_s2t_ext(char *in,int len)
{
	unsigned int ha;
	unsigned short magic;
	int i;
	int pos;
	char *p;
	if(len<=3)
		return 0;
	ha=hash_func(in,len);
	magic=ha>>16;
	for(i=ha%S2T_PHRASE_NUM;(pos=s2t_phrase[i])!=0xffff;i=(i+1)%S2T_PHRASE_NUM)
	{
		p=(char*)s2t_data+pos;
		if(*(unsigned short*)p!=magic) continue;
		p+=2;
		if(*p!=(char)len) continue;
		p+=1;
		if(memcmp(in,p,len)) continue;
		memcpy(in,p+len,len);
		return 1;
	}
	if(len>4)
	{
		if(adjust_phrase_s2t_ext(in,4))
			adjust_phrase_s2t_ext(in+4,len-4);
		else
			adjust_phrase_s2t_ext(in+2,len-2);
	}
	return 0;
}

static int adjust_phrase_t2s_ext(char *in,int len)
{
	unsigned int ha;
	unsigned short magic;
	int i;
	int pos;
	char *p;
	if(len<=3)
		return 0;
	ha=hash_func(in,len);
	magic=ha>>16;
	for(i=ha%T2S_PHRASE_NUM;(pos=t2s_phrase[i])!=0xffff;i=(i+1)%T2S_PHRASE_NUM)
	{
		p=(char*)t2s_data+pos;
		if(*(unsigned short*)p!=magic) continue;
		p+=2;
		if(*p!=(char)len) continue;
		p+=1;
		if(memcmp(in,p,len)) continue;
		memcpy(in,p+len,len);
		return 1;
	}
	if(len>4)
	{
		if(adjust_phrase_t2s_ext(in,4))
			adjust_phrase_t2s_ext(in+4,len-4);
		else
			adjust_phrase_t2s_ext(in+2,len-2);
	}
	return 0;
}

const char *s2t_conv(const char *s)
{
	static char t[256];
	uint16_t code;
	int len;
	int pos=0;
	uint16_t *res;
	
	s2t_open=s2t_get_enable();

	if(!s2t_open) return s;
	if(!s) return 0;
	if(im.SelectMode) return s;

	len=strlen(s);
	if(len<=1/* || (len&0x01)*/)
		return s;
	while(len>=1)
	{
		if(!(s[0]&0x80))
		{
			t[pos++]=*s++;
			len--;
			continue;
		}
		code=GBK_MAKE_CODE(s[0],s[1]);

		if(!GBK_IS_VALID(code))
		{
			t[pos++]=*s++;
			t[pos++]=*s++;
			len-=2;
			continue;
		}
		s+=2;
		len-=2;
		if(s2t_open==1)
		{
			if(!GB2312_IS_BIAODIAN(code) || s2t_bd)
				res=bsearch(&code,s2t,s2t_num,4,s2t_compar);
			else
				res=NULL;
		}
		else
		{
			if(!GB2312_IS_BIAODIAN(code) || s2t_bd)
				res=bsearch(&code,t2s,t2s_num,4,s2t_compar);
			else
				res=NULL;
		}
		if(res) code=res[1];
		GBK_MAKE_STRING(code,t+pos);	
		pos+=2;
	}
	t[pos]=0;
	if(s2t_open==1)
	{
		//return adjust_phrase(t,pos);
		adjust_phrase_s2t_ext(t,pos);
		return t;
	}
	else
	{
		//return adjust_phrase_t2s(t,pos);
		adjust_phrase_t2s_ext(t,pos);
		return t;
	}
}

int s2t_select(const char *s)
{
	EXTRA_IM *eim;
	int i;
	uint16_t code;
	uint16_t *res;

	if(im.SelectMode)
		return 0;
	if(s2t_is_enable()==0)
		return 0;
	if(!s2t_multi)
		return 0;
	if(!gb_is_gbk((const uint8_t*)s))
		return 0;
	if(gb_strlen((const uint8_t*)s)!=1)
		return 0;

	eim=y_select_eim();
	code=GBK_MAKE_CODE(s[0],s[1]);
	if(s2t_open==1)
	{
		for(i=0;i<5;i++)
		{
			res=bsearch(&code,s2t_m[i],s2t_num_m[i],4,s2t_compar);
			if(!res) break;
			GBK_MAKE_STRING(res[1],eim->CandTable[i]);
			eim->CodeTips[i][0]=0;
		}
	}
	else
	{
		for(i=0;i<2;i++)
		{
			res=bsearch(&code,t2s_m[i],t2s_num_m[i],4,s2t_compar);
			if(!res) break;
			GBK_MAKE_STRING(res[1],eim->CandTable[i]);
			eim->CodeTips[i][0]=0;
		}
	}
	if(i<=1) return 0;
	eim->SelectIndex=0;
	eim->CandWordCount=i;
	eim->CurCandPage=0;
	eim->CandPageCount=1;
	eim->CodeLen=0;
	strcpy(eim->StringGet,s2t_open==1?"¼ò×ª·±£º":"·±×ª¼ò£º");
	y_im_str_encode(eim->StringGet,im.StringGet,0);
	GBK_MAKE_STRING(code,eim->CodeInput);
	y_im_str_encode(eim->CodeInput,im.CodeInput,DONT_ESCAPE);
	im.SelectMode=1;
	return i;
}

#else

void s2t_enable(int b)
{
}

void s2t_biaodian(int b)
{
}

void s2t_multiple(int b)
{
}

int s2t_is_enable(void)
{
	return 0;
}

char *s2t_conv(char *s)
{
	return s;
}

int s2t_select(const char *s)
{
	return 0;
}

#endif
