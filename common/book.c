#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "aes.h"
#include "lbase64.h"
#include "md5.h"
#include "english.h"
#include "llib.h"
#include "common.h"
#include "translate.h"

static int BookSet(const char *s);
static int BookGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
ENGLISH_IM eim_book={BookSet,BookGet};

static uint8_t key[32]="hkjk45t!(90?cp|";
static LKeyFile *book;

int y_im_book_encrypt(const char *s,char *out)
{
    uint8_t data[17]={0},edata[64];
    int len,i,count;
    aes_set_key(key,256);
    len=strlen(s);
    if(len>64)
		return -1;
	count=len/16+(len%16?1:0);
    for(i=0;i<count;i++)
    {
		int part=len-i*16;
		if(part>=16)
		{
			part=16;
			memcpy(data,s+i*16,16);
		}
		else
		{
			memset(data,0,16);
			strcpy((char*)data,s+i*16);
			data[15]=part;
		}
		aes_encrypt(data,edata+i*16);
	}
    l_base64_encode(out,edata,16*count);
    return 0;
}

int y_im_book_decrypt(const char *s,char *out)
{
    uint8_t edata[128];
    int len;
    int i;
    int count;
    aes_set_key(key,256);
    if(strlen(s)>128)
    {
        return -1;
	}
    len=l_base64_decode(edata,s);
    if((len%16)!=0 || len==0 || len>64)
    {
        return -1;
	}
    count=len/16;
    for(i=0;i<count;i++)
    {
		aes_decrypt(edata+i*16,(uint8_t*)out+i*16);
		if(i<count-1)
		{
			continue;
		}
		len=(int)(uint8_t)out[i*16+15];
		if(len<16)
		{
			len=i*16+len;
		}
		else
		{
			len=i*16+16;
		}
		break;
	}
	out[len]=0;
    for(i=0;i<len;i++)
    {
		uint8_t c=(uint8_t)out[i];
        if(c<0x20 || c==0x7f || c==0x80 || c==0xff)
        {
			out[0]=0;
            return -1;
		}
    }
    return 0;
}

void y_im_free_book(void)
{
	if(!book)
		return;
	l_key_file_free(book);
	book=NULL;
}

void y_im_load_book(void)
{
	y_im_free_book();
	book=l_key_file_open("book.ini",0,
				y_im_get_path("HOME"),
				y_im_get_path("DATA"),NULL);
}

int y_im_book_key(const char *s)
{
	char temp[32];
	MD5_CTX ctx;
	if(!s ||!s[0])
	{
		memset(key+16,0,16);
		aes_set_key(key,256);
		return 0;
	}
	int len=l_base64_decode(NULL,s);
	if(len<=0 || len>16)
		return -1;
	l_base64_decode((uint8_t*)temp,s);
	MD5Init(&ctx);
	MD5Update(&ctx,(const uint8_t*)temp,len);
	MD5Update(&ctx,key,strlen((char*)key));
	MD5Final(&ctx);
	memcpy(key+16,ctx.digest,16);
	return 0;
}

static int BookSet(const char *s)
{
	ENGLISH_IM *e=&eim_book;
	int i;
	if(!strncasecmp(s,"key ",4) && strlen(s+4)<=16)
	{
		e->Count=1;
		e->Priv1=1;
		e->Priv2=(uintptr_t)s+4;
		return 1;
	}
	if(!strncasecmp(s,"miyao ",6) && strlen(s+6)<=16)
	{
		e->Count=1;
		e->Priv1=1;
		e->Priv2=(uintptr_t)s+6;
		return 1;
	}
	if(!strncasecmp(s,"encrypt ",8) && strlen(s+8)<=64)
	{
		e->Count=1;
		e->Priv1=2;
		e->Priv2=(uintptr_t)s+8;
		return 1;
	}
	if(!strncasecmp(s,"jiami ",6) && strlen(s+6)<=64)
	{
		e->Count=1;
		e->Priv1=2;
		e->Priv2=(uintptr_t)s+6;
		return 1;
	}
	if(!book)
		return 0;
	i=strcspn(s,",");
	if(s[i]!=',' && l_key_file_has_group(book,s))
	{
		char **keys=l_key_file_get_keys(book,s);
		if(!keys)
		{
			e->Count=0;
		}
		else
		{
			e->Count=l_strv_length(keys);
			l_strfreev(keys);
			if(e->Count>0)
			{
				e->Priv1=3;
				e->Priv2=(uintptr_t)s;
				return e->Count;
			}
		}
	}
	if(s[i]==',')
	{
		char grp[64];
		memcpy(grp,s,i);
		grp[i]=0;
		if(!l_key_file_get_data(book,grp,s+i+1))
		{
			e->Count=0;
		}
		else
		{
			e->Count=1;
			e->Priv1=4;
			e->Priv2=(uintptr_t)s;
			return 1;
		}
	}
	e->Count=0;
	e->Priv1=0;
	return 0;
}

static int BookGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_book;
	if(!e->Priv1 || !e->Count || !count)
		return 0;
	if(e->Priv1==1)
	{
		char *p=cand[pos];
		char *s=(char*)e->Priv2;
		int ret=sprintf(p,"$[");
		ret+=sprintf(p+ret,"%s",YT("设置秘钥"));
		ret+=sprintf(p+ret,"]$KEY(");
		l_base64_encode(p+ret,(const uint8_t*)s,strlen(s));
		strcat(p,")");
	}
	else if(e->Priv1==2)
	{
		char *p=cand[pos];
		char *s=(char*)e->Priv2;
		if(s[0]==0)
		{
			int ret=sprintf(p,"$[");
			ret+=sprintf(p+ret,"%s",YT("加密剪贴板内容"));
			ret+=sprintf(p+ret,"]$ENCRYPT()");
		}
		else
		{
			y_im_book_encrypt(s,p);
		}
	}
	else if(e->Priv1==3)
	{
		char *s=(char*)e->Priv2;
		assert(book!=NULL);
		char **keys=l_key_file_get_keys(book,s);
		int i;
		for(i=pos;i<pos+count && i<e->Count;i++)
		{
			char *t=l_key_file_get_string(book,s,keys[i]);
			char *p=cand[i];
			if(strlen(t)>MAX_CAND_LEN)
			{
				sprintf(p,"$[%s]",YT("太长了，不支持"));
			}
			else
			{
				l_utf8_to_gb(t,p,MAX_CAND_LEN);
			}
			l_free(t);			
		}
	}
	else if(e->Priv1==4)
	{
		char *p=cand[pos];
		char *s=(char*)e->Priv2;
		int i=strcspn(s,",");
		char grp[64];
		char *t;
		memcpy(grp,s,i);
		grp[i]=0;
		assert(book!=NULL);
		t=l_key_file_get_string(book,grp,s+i+1);
		if(strlen(t)>MAX_CAND_LEN)
		{
			sprintf(p,"$[%s]",YT("太长了，不支持"));
		}
		else
		{
			l_utf8_to_gb(t,p,MAX_CAND_LEN);
		}
		l_free(t);
	}
	return 0;
}
