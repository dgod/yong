#pragma once

void l_noop(void);

#ifdef _WIN32
#include <windows.h>
DWORD WINAPI l_winthread_wrapper(void *param);
#else
void *l_pthread_wrapper(void *param);
#endif

int l_int_equal(const void *v1,const void *v2);
int l_int_equal_r(const void *v1,const void *v2);
int l_uint16_equal(const void *p1,const void *p2);
int l_uint32_equal(const void *p1,const void *p2);
int l_uint64_equal(const void *v1,const void *v2);

int l_rand(int min,int max);
