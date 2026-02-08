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

int l_uint16_equal(const void *p1,const void *p2)
{
	uint16_t v1=*(const uint16_t*)p1;
	uint16_t v2=*(const uint16_t*)p2;
	if(v1>v2)
		return 1;
	else if(v1==v2)
		return 0;
	else
		return -1;
}

int l_uint32_equal(const void *p1,const void *p2)
{
	uint32_t v1=*(const uint32_t*)p1;
	uint32_t v2=*(const uint32_t*)p2;
	if(v1>v2)
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

int l_rand(int min,int max)
{
    static bool init=false;
    if(min>max || min<0)
        return -1;
    if(min==max)
        return min;
#ifdef __ANDROID__
    if(!init)
    {
        srand48((long int)time(NULL));
        init=true;
    }
    if(min==0 && max==INT_MAX)
        return lrand48();
    return min+lrand48()%(max-min+1);
#else
    if(!init)
    {
        srand((unsigned int)time(NULL));
        init=true;
    }
#if RAND_MAX==INT_MAX
    if(min==0 && max==INT_MAX)
        return rand();
    return min+rand()%(max-min+1);
#elif RAND_MAX==0x7fff
    if(max-min+1<=0x7fff)
        return min+rand()%(max-min+1);
    if(max-min+1<=0x3fffffff)
    {
        int r=(rand()<<15)|rand();
        return min+r%(max-min+1);
    }
    else
    {
        int r0=rand(),r1=rand(),r2=rand();
        int r=(r0<<16)|(r1<<1)|(r2&1);
        if(min==0 && max==INT_MAX)
            return r;
        return min+r%(max-min+1);
    }
#else
    #error "RAND_MAX is not supported"
#endif
#endif
}

