#include "llib.h"

typedef struct{
	LSList;
	const char *name;
	int flags;
	union {
		const char *string;
		struct{
			char *(*cb)(void*);
			void *arg;
		};
	};
}PSEUDO_ENV;
static PSEUDO_ENV *pseudo_envs;

static char *pseudo_env_get(const char *name,int flags,char *buf,int size)
{
	PSEUDO_ENV *p=l_slist_find_by(pseudo_envs,name,strcmp,name);
	if(!p)
		return NULL;
	int type=p->flags&0x7f;
	int encoding=p->flags&0x80;
	if(type==L_PSEUDO_ENV_STRING)
	{
		if(flags==encoding)
		{
			if(buf)
			{
				l_strcpy(buf,size,p->string);
				return buf;
			}
			return l_strdup(p->string);
		}
		size=size>0?size:(int)strlen(p->string)*2+4;
		char *temp=buf?buf:l_alloca(size);
		if(encoding==L_PSEUDO_ENV_STRING)
			l_gb_to_utf8(p->string,temp,size);
		else
			l_utf8_to_gb(p->string,temp,size);
		if(buf)
		{
			return buf;
		}
		return l_strdup(temp);
	}
	else if(type==L_PSEUDO_ENV_FUNC)
	{
		char *s=p->cb(p->arg);
		if(!s)
			return NULL;
		if(flags==encoding)
		{
			if(buf)
			{
				l_strcpy(buf,size,s);
				l_free(s);
				return buf;
			}
			return s;
		}
		size=size?size:(int)strlen(s)*2+4;
		char *temp=buf?buf:l_alloca(size);
		l_free(s);
		if(encoding==L_PSEUDO_ENV_STRING)
			l_gb_to_utf8(p->string,temp,size);
		else
			l_utf8_to_gb(p->string,temp,size);
		if(buf)
		{
			return buf;
		}
		return l_strdup(temp);
	}
	return NULL;
}

#if defined(__linux__) || defined(__EMSCRIPTEN__)
char *l_getenv(const char *name,char *buf,int size)
{
	char *t;
	t=pseudo_env_get(name,L_PSEUDO_ENV_UTF8,buf,size);
	if(t)
		return t;
	t=getenv(name);
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
	char *t;
	t=pseudo_env_get(name,0,buf,size);
	if(t)
		return t;
	t=getenv(name);
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
	char *e=pseudo_env_get(name,L_PSEUDO_ENV_UTF8,buf,size);
	if(e)
		return e;
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
	char *e=pseudo_env_get(name,0,buf,size);
	if(e)
		return e;
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

int l_setenv_pseudo(const char *name,int flags,...)
{
	int type=flags&0x7f;
	PSEUDO_ENV *p=l_slist_find_by(pseudo_envs,name,strcmp,name);
	if(type==L_PSEUDO_ENV_NONE)
	{
		if(p)
		{
			pseudo_envs=l_slist_remove(pseudo_envs,p);
			l_free(p);
		}
		return 0;
	}
	if(type!=L_PSEUDO_ENV_STRING && type!=L_PSEUDO_ENV_FUNC)
		return -1;
	if(!p)
	{
		p=l_new(PSEUDO_ENV);
		p->name=name;
		pseudo_envs=l_slist_prepend(pseudo_envs,p);
	}
	p->flags=flags;
		va_list ap;
	va_start(ap,flags);
	if(type==L_PSEUDO_ENV_STRING)
	{
		p->string=va_arg(ap,const char*);
	}
	else
	{
		p->cb=va_arg(ap,void*);
		p->arg=va_arg(ap,void*);
	}
	va_end(ap);
	return 0;
}
