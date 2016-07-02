#include "ltypes.h"
#include "lconv.h"
#include "lmem.h"

#ifdef USE_SYSTEM_ICONV

#ifdef _WIN32
#include <windows.h>

static int gb_codepage;

static void detect_codepage(void)
{
	if(MultiByteToWideChar(54936,0," ",-1,0,0)>0)
		gb_codepage=54936;
	else
		gb_codepage=936;
}

void *l_gb_to_utf16(const char *s,void *out,int size)
{
	if(!gb_codepage) detect_codepage();
	MultiByteToWideChar(gb_codepage,0,s,-1,out,size/sizeof(WCHAR));
	return out;
}

char *l_gb_to_utf8(const char *s,char *out,int size)
{
	int len=MultiByteToWideChar(gb_codepage,0,s,-1,NULL,0);
	WCHAR temp[len+1];
	l_gb_to_utf16(s,temp,sizeof(temp));
	l_utf16_to_utf8(temp,out,size);
	return out;
}

char *l_utf8_to_gb(const char *s,char *out,int size)
{
	int len=MultiByteToWideChar(CP_UTF8,0,s,-1,NULL,0);
	WCHAR temp[len+1];
	l_utf8_to_utf16(s,temp,sizeof(temp));
	l_utf16_to_gb(temp,out,size);
	return out;
}

char *l_utf16_to_gb(const void *s,char *out,int size)
{
	if(!gb_codepage) detect_codepage();
	WideCharToMultiByte(gb_codepage,0,s,-1,out,size,NULL,FALSE);
	return out;
}

#else
#include <iconv.h>

static iconv_t gb_utf8=(iconv_t)-1;
static iconv_t gb_utf16=(iconv_t)-1;
static iconv_t utf16_gb=(iconv_t)-1;
static iconv_t utf8_gb=(iconv_t)-1;

char *l_gb_to_utf8(const char *s,char *out,int size)
{
	size_t l1=strlen(s),l2=size-1;
	char *inbuf=(char*)s,*outbuf=(char*)out;

	if(gb_utf8==(iconv_t)-1)
	{
		gb_utf8=iconv_open("UTF8","GB18030");
		if(gb_utf8==(iconv_t)-1)
		{
			gb_utf8=iconv_open("UTF8","GBK");
			if(gb_utf8==(iconv_t)-1)
				return 0;
		}
	}
	
	iconv(gb_utf8,&inbuf,&l1,&outbuf,&l2);
	outbuf[0]=0;
	
	return out;
}

void *l_gb_to_utf16(const char *s,void *out,int size)
{
	size_t l1=strlen(s),l2=size-2;
	char *inbuf=(char*)s,*outbuf=(char*)out;
	
	if(gb_utf16==(iconv_t)-1)
	{
		gb_utf16=iconv_open("UTF16","GB18030");
		if(gb_utf16==(iconv_t)-1)
		{
			gb_utf16=iconv_open("UTF16","GBK");
			if(gb_utf16==(iconv_t)-1)
				return 0;
		}
	}
	iconv(gb_utf16,&inbuf,&l1,&outbuf,&l2);
	outbuf[0]=outbuf[1]=0;
	
	return out;
}

char *l_utf8_to_gb(const char *s,char *out,int size)
{
	size_t l1=strlen(s),l2=size-1;
	char *inbuf=(char*)s,*outbuf=(char*)out;
	
	if(utf8_gb==(iconv_t)-1)
	{
		utf8_gb=iconv_open("GB18030","UTF8");
		if(utf8_gb==(iconv_t)-1)
		{
			utf8_gb=iconv_open("GBK","UTF8");
			if(utf8_gb==(iconv_t)-1)
				return 0;
		}
	}
	iconv(utf8_gb,&inbuf,&l1,&outbuf,&l2);
	outbuf[0]=0;
	return out;
}

static int utf16_size(const void *s)
{
	const uint16_t *p=s;
	int i;
	for(i=0;p[i]!=0;i++);
	return i<<1;
}

char *l_utf16_to_gb(const void *s,char *out,int size)
{
	size_t l1=utf16_size(s),l2=size-1;
	char *inbuf=(char*)s,*outbuf=(char*)out;
	if(utf16_gb==(iconv_t)-1)
	{
		utf16_gb=iconv_open("GB18030","UTF16");
		if(utf16_gb==(iconv_t)-1)
		{
			utf16_gb=iconv_open("GBK","UTF16");
			if(utf16_gb==(iconv_t)-1)
				return 0;
		}
	}
	iconv(utf16_gb,&inbuf,&l1,&outbuf,&l2);
	outbuf[0]=0;
	return out;
}

#endif

#endif
