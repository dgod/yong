#pragma once

char *l_getenv(const char *name,char *buf,int size);
char *l_getenv_gb(const char *name,char *buf,int size);
#ifdef _WIN32
int l_setenv(const char *name,const char *value,int overwrite);
#else
#define l_setenv(name,value,overwrite) setenv(name,value,overwrite)
#endif

#define L_PSEUDO_ENV_NONE			0x00
#define L_PSEUDO_ENV_STRING			0x01
#define L_PSEUDO_ENV_FUNC			0x02
#define L_PSEUDO_ENV_UTF8			0x80
int l_setenv_pseudo(const char *name,int flags,...);
