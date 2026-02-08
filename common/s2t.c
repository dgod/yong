#if !defined(CFG_NO_S2T)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "yong.h"
#include "select.h"
#include "im.h"
#include "common.h"
#include "xim.h"

#include "s2t_char.c"
#include "s2t_phrase.c"
#include "t2s_char.c"
#include "t2s_phrase.c"

static int s2t_open;
static int s2t_bd;
static int s2t_multi;

static int s2t_get_enable(void)
{
	CONNECT_ID *id=y_xim_get_connect();
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

static unsigned hash_func(const char *in,int len)
{
	unsigned int ha=0;
	for(int i=0;i<len && *in;i++)
		ha=ha*33+*(unsigned char*)in++;
	return ha;
}

static int adjust_phrase_s2t_ext(char *in,int len)
{
	int pos;
	char *p;
	if(len<=3)
		return 0;
	unsigned int ha=hash_func(in,len);
	for(int i=ha%S2T_PHRASE_NUM;(pos=s2t_phrase[i])!=0xffff;i=(i+1)%S2T_PHRASE_NUM)
	{
		p=(char*)s2t_data+pos;
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
	int pos;
	char *p;
	if(len<=3)
		return 0;
	unsigned int ha=hash_func(in,len);
	for(int i=ha%T2S_PHRASE_NUM;(pos=t2s_phrase[i])!=0xffff;i=(i+1)%T2S_PHRASE_NUM)
	{
		p=(char*)t2s_data+pos;
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
	int pos=0;
	
	s2t_open=s2t_get_enable();

	if(!s2t_open) return s;
	if(!s) return 0;
	if(im.SelectMode) return s;

	while(true)
	{
		uint32_t code=l_gb_to_char(s);
		bool is_biaodian=gb_is_biaodian(s);
		s=l_gb_next_char(s);
		if(s==NULL)
			break;
		if(s2t_open==1)
		{
			if(!is_biaodian || s2t_bd)
			{
				if(code<0x10000)
				{
					const uint16_t *res=bsearch(&code,s2t,s2t_num,4,l_uint16_equal);
					if(res) code=res[1];
					else goto S2T_EXT;
				}
				else
				{
S2T_EXT:
					const uint32_t *res=bsearch(&code,s2te,s2te_num,4,l_uint32_equal);
					if(res) code=res[1];
				}
			}
		}
		else
		{
			if(!is_biaodian || s2t_bd)
			{
				if(code<0x10000)
				{
					const uint16_t *res=bsearch(&code,t2s,t2s_num,4,l_uint16_equal);
					if(res) code=res[1];
					else goto T2S_EXT;
				}
				else
				{
T2S_EXT:
					const uint32_t *res=bsearch(&code,s2te,s2te_num,4,l_uint32_equal);
					if(res) code=res[1];
				}
			}
		}
		pos+=l_char_to_gb(code,t+pos);
	}
	t[pos]=0;
	if(s2t_open==1)
		adjust_phrase_s2t_ext(t,pos);
	else
		adjust_phrase_t2s_ext(t,pos);
	return t;
}

int s2t_select(const char *s)
{
	if(im.SelectMode)
		return 0;
	if(s2t_is_enable()==0)
		return 0;
	if(!s2t_multi)
		return 0;
	if(!gb_is_gbk((const uint8_t*)s))
		return 0;
	if(l_gb_strlen(s,-1)!=1)
		return 0;

	EXTRA_IM *eim=y_select_eim();
	uint32_t code=l_gb_to_char(s);
	int i;
	if(s2t_open==1)
	{
		for(i=0;i<5;i++)
		{
			const uint16_t *res=bsearch(&code,s2t_m[i],s2t_num_m[i],4,l_uint16_equal);
			if(!res) break;
			l_char_to_gb0(res[1],eim->CandTable[i]);
			eim->CodeTips[i][0]=0;
		}
	}
	else
	{
		for(i=0;i<2;i++)
		{
			const uint16_t *res=bsearch(&code,t2s_m[i],t2s_num_m[i],4,l_uint16_equal);
			if(!res) break;
			l_char_to_gb0(res[1],eim->CandTable[i]);
			eim->CodeTips[i][0]=0;
		}
	}
	if(i<=1) return 0;
	eim->SelectIndex=0;
	eim->CandWordCount=i;
	eim->CurCandPage=0;
	eim->CandPageCount=1;
	strcpy(eim->StringGet,s2t_open==1?"¼ò×ª·±£º":"·±×ª¼ò£º");
	y_im_str_encode(eim->StringGet,im.StringGet,0);
	l_char_to_gb0(code,eim->CodeInput);
	eim->CodeLen=strlen(eim->CodeInput);
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

const char *s2t_conv(const char *s)
{
	return s;
}

int s2t_select(const char *s)
{
	return 0;
}

#endif
