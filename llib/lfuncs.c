#include "llib.h"

void l_noop(void)
{
}

#if !L_USE_C11_THREADS

#include "lthreads.h"

#ifdef _WIN32

DWORD WINAPI l_winthread_wrapper(void *param)
{
	void **arr=(void**)param;
	l_thrd_start_t func = (l_thrd_start_t)arr[0];
	void *arg = ((void**)arr)[1];
	l_free(param);
	return (DWORD)func(arg);
}

#else

void *l_pthread_wrapper(void *param)
{
	void **arr=(void**)param;
	l_thrd_start_t func = (l_thrd_start_t)arr[0];
	void *arg = ((void**)arr)[1];
	l_free(param);
	return (void*)(size_t)func(arg);
}

#endif
#endif

int l_int_equal(const void *p1,const void *p2)
{
	int v1=*(const int*)p1;
	int v2=*(const int*)p2;
	if(v1>v2)
		return 1;
	else if(v1==v2)
		return 0;
	else
		return -1;
}

int l_int_equal_r(const void *p1,const void *p2)
{
	int v1=*(const int*)p1;
	int v2=*(const int*)p2;
	if(v2>v1)
		return 1;
	else if(v1==v2)
		return 0;
	else
		return -1;
}

int l_uint64_equal(const void *p1,const void *p2)
{
	uint64_t v1=*(const uint64_t*)p1;
	uint64_t v2=*(const uint64_t*)p2;
	if(v1>v2)
		return 1;
	else if(v1==v2)
		return 0;
	else
		return -1;
}



