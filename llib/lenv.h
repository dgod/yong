#pragma once

char *l_getenv(const char *name,char *buf,int size);
char *l_getenv_gb(const char *name,char *buf,int size);
#ifdef _WIN32
int l_setenv(const char *name,const char *value,int overwrite);
#else
#define l_setenv(name,value,overwrite) setenv(name,value,overwrite)
#endif
