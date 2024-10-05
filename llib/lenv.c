#include "llib.h"

#ifdef __linux__
char *l_getenv(const char *name,char *buf,int size)
{
	char *t=getenv(name);
	if(!t)
		return NULL;
	if(buf)
	{
		if(strlen(t)>=size)
			return NULL;
		strcpy(buf,t);
		return buf;
	}
	return l_strdup(t);
}

char *l_getenv_gb(const char *name,char *buf,int size)
{
	char *t=getenv(name);
	if(!t)
		return NULL;
	char temp[256];
	l_utf8_to_gb(t,temp,sizeof(temp));
	t=temp;
	if(buf)
	{
		if(strlen(t)>=size)
			return NULL;
		strcpy(buf,t);
		return buf;
	}
	return l_strdup(t);
}
#else
char *l_getenv(const char *name,char *buf,int size)
{
	wchar_t temp[256];
	char temp2[256];
	l_utf8_to_utf16(name,temp,sizeof(temp));
	wchar_t *t=_wgetenv(temp);
	if(!t)
		return NULL;
	if(buf)
	{
		l_utf16_to_utf8(t,buf,size);
		return buf;
	}
	l_utf16_to_utf8(t,temp2,sizeof(temp2));
	return l_strdup(temp2);
}

char *l_getenv_gb(const char *name,char *buf,int size)
{
	wchar_t temp[256];
	char temp2[256];
	l_utf8_to_utf16(name,temp,sizeof(temp));
	wchar_t *t=_wgetenv(temp);
	if(!t)
		return NULL;
	if(buf)
	{
		l_utf16_to_gb(t,buf,size);
		return buf;
	}
	l_utf16_to_gb(t,temp2,sizeof(temp2));
	return l_strdup(temp2);
}
#endif

#ifdef _WIN32
int l_setenv(const char *name,const char *value,int overwrite)
{
	char temp[256];
	if(!overwrite)
	{
		if(l_getenv(name,temp,sizeof(temp)))
			return 0;
	}
	snprintf(temp,sizeof(temp),"%s=%s",name,value);
	wchar_t temp2[256];
	l_utf8_to_utf16(temp,temp2,sizeof(temp2));
	return _wputenv(temp2);
}
#endif

