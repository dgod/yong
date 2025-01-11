#pragma once

#include <time.h>
#include "lmem.h"
#include "lfuncs.h"

#ifndef L_USE_C11_THREADS
#define L_USE_C11_THREADS 0
#else
#if !__has_include("threads.h")
#undef L_USE_C11_THREADS
#define L_USE_C11_THREADS 1
#endif
#endif

#if L_USE_C11_THREADS

#include <threads.h>

#define l_thread_local thread_local

#define l_thrd_success thrd_success
#define l_thrd_busy thrd_busy
#define l_thrd_error thrd_error
#define l_thrd_nomem thrd_nomem
#define l_thrd_timedout thrd_timedout

#define l_mtx_plain mtx_plain
#define l_mtx_recursive mtx_recursive
#define l_mtx_timed mtx_timed

#define l_thrd_t thrd_t
#define l_mtx_t mtx_t
#define l_cnd_t cnd_t

#define l_thrd_create(thr,func,arg) thrd_create(thr,func,arg)
#define l_thrd_join(thr,res) thrd_join(thr,res)
#define l_thrd_detach(thr) thrd_detach(thr)
#define l_thrd_exit(res) thrd_exit(res)
#define l_thrd_sleep(t,r) thrd_sleep(t,r)

#define l_mtx_init(mtx,type) mtx_init(mtx,type)
#define l_mtx_destroy(mtx) mtx_destroy(mtx)
#define l_mtx_lock(mtx) mtx_lock(mtx)
#define l_mtx_trylock(mtx) mtx_trylock(mtx)
#define l_mtx_unlock(mtx) mtx_unlock(mtx)
#define l_mtx_timedlock(mtx,ts) mtx_timedlock(mtx,ts)

#define l_cnd_init(cond) cnd_init(cond)
#define l_cnd_destroy(cond) cnd_destroy(cond)
#define l_cnd_signal(cond) cnd_signal(cond)
#define l_cnd_wait(cond,mtx) cnd_wait(cond,mtx)
#define l_cnd_timedwait(cond,mtx,ts) cnd_timedwait(cond,mtx,ts)

#else

typedef int (*l_thrd_start_t)(void *);

enum
{
	l_thrd_success  = 0,
	l_thrd_busy     = 1,
	l_thrd_error    = 2,
	l_thrd_nomem    = 3,
	l_thrd_timedout = 4
};

enum
{
	l_mtx_plain     = 0,
	l_mtx_recursive = 1,
	l_mtx_timed     = 2
};


#ifdef _WIN32
#include <windows.h>

#define l_thread_local __declspec(thread)
typedef HANDLE l_thrd_t;
typedef HANDLE l_mtx_t;
typedef HANDLE l_cnd_t;

DWORD WINAPI l_winthread_wrapper(void *param);

static inline int l_thrd_create(l_thrd_t *thr,l_thrd_start_t func,void *arg)
{
	void **param=l_cnew(2,void*);
	param[0]=func;
	param[1]=arg;
	*thr = CreateThread(NULL,0,l_winthread_wrapper,param,0,NULL);
	if(*thr != NULL)
		return l_thrd_success;
	l_free(param);
	return l_thrd_busy;
}

static inline int l_thrd_join(l_thrd_t thr,int *res)
{
	if(WAIT_OBJECT_0!=WaitForSingleObject(thr,INFINITE))
		return l_thrd_error;
	if(res!=NULL)
	{
		GetExitCodeThread(thr,(LPDWORD)res);
	}
	CloseHandle(thr);
	return l_thrd_success;
}

static inline int l_thrd_detach(l_thrd_t thr)
{
	return CloseHandle(thr)?l_thrd_success:l_thrd_busy;
}

static inline void l_thrd_exit(int res)
{
	ExitThread((DWORD)res);
}

static inline int l_thrd_sleep(const struct timespec *time_point,struct timespec *remain)
{
	Sleep(time_point->tv_sec*1000+time_point->tv_nsec/1000000);
	if(remain)
		remain->tv_sec=remain->tv_nsec=0;
	return 0;
}

static inline int l_mtx_init(l_mtx_t *mtx,int type)
{
	(void)type;
	*mtx=CreateMutex(NULL,FALSE,NULL);
	return *mtx==NULL?l_thrd_success:l_thrd_busy;
}

static inline int l_mtx_destroy(l_mtx_t *mtx)
{
	BOOL ret=CloseHandle(*mtx);
	*mtx=NULL;
	return ret?l_thrd_success:l_thrd_busy;
}

static inline int l_mtx_lock(l_mtx_t *mtx)
{
	return WaitForSingleObject(*mtx,INFINITE)==WAIT_OBJECT_0?l_thrd_success:l_thrd_busy;
}

static inline int l_mtx_trylock(l_mtx_t *mtx)
{
	return WaitForSingleObject(*mtx,0)==WAIT_OBJECT_0?l_thrd_success:l_thrd_busy;
}

static inline int l_mtx_unlock(l_mtx_t *mtx)
{
	return ReleaseMutex(*mtx)?l_thrd_success:l_thrd_busy;
}

static inline int l_mtx_timedlock(l_mtx_t *mtx, const struct timespec *ts)
{
	return WaitForSingleObject(*mtx,ts->tv_sec*1000+ts->tv_nsec/1000000)>=WAIT_OBJECT_0?l_thrd_success:l_thrd_busy;
}

static inline int l_cnd_init(l_cnd_t *cond)
{
	*cond=CreateEvent(NULL,FALSE,FALSE,NULL);
	if(!*cond)
		return l_thrd_error;
	return l_thrd_success;
}

static inline int l_cnd_destroy(l_cnd_t *cond)
{
	return CloseHandle(*cond)?l_thrd_success:l_thrd_error;
}

static inline int l_cnd_signal(l_cnd_t *cond)
{
	return SetEvent(*cond)?l_thrd_success:l_thrd_error;
}

static inline int l_cnd_wait(l_cnd_t *cond,l_mtx_t *mtx)
{
	if(TRUE!=ReleaseMutex(*mtx))
		return l_thrd_error;
	DWORD ret=WaitForSingleObject(*cond,INFINITE);
	if(ret!=WAIT_OBJECT_0)
	{
		WaitForSingleObject(*mtx,INFINITE);
		return l_thrd_error;
	}
	ret=WaitForSingleObject(*mtx,INFINITE);
	if(ret!=WAIT_OBJECT_0)
		return l_thrd_error;
	return l_thrd_success;
}

static inline int l_cnd_timedwait(l_cnd_t *cond,l_mtx_t *mtx,const struct timespec *ts)
{
	if(TRUE!=ReleaseMutex(*mtx))
		return l_thrd_error;
	DWORD ret=WaitForSingleObject(*cond,ts->tv_sec*1000+ts->tv_nsec/1000000);
	if(ret==WAIT_TIMEOUT)
	{
		WaitForSingleObject(*mtx,INFINITE);
		return l_thrd_timedout;
	}
	if(ret!=WAIT_OBJECT_0)
	{
		WaitForSingleObject(*mtx,INFINITE);
		return l_thrd_error;
	}
	ret=WaitForSingleObject(*mtx,INFINITE);
	if(ret!=WAIT_OBJECT_0)
		return l_thrd_error;
	return l_thrd_success;
}

#else

#include <pthread.h>
#include <unistd.h>

#define l_thread_local __thread

typedef pthread_t l_thrd_t;
typedef pthread_mutex_t l_mtx_t;
typedef pthread_cond_t l_cnd_t;

void *l_pthread_wrapper(void *param);

static inline int l_thrd_create(l_thrd_t *thr,l_thrd_start_t func,void *arg)
{
	void **param=l_cnew(2,void*);
	param[0]=func;
	param[1]=arg;
	int ret=pthread_create(thr,NULL,l_pthread_wrapper,param);
	if(ret==0)
		return l_thrd_success;
	l_free(param);
	return l_thrd_error;
}

static inline int l_thrd_join(l_thrd_t thr,int *res)
{
	void *temp;
	int ret=pthread_join(thr,&temp);
	if(ret==0 && res)
		*res=(int)(size_t)temp;
	return ret==0?l_thrd_success:l_thrd_error;
}

static inline int l_thrd_detach(l_thrd_t thr)
{
	int ret=pthread_detach(thr);
	return ret==0?l_thrd_success:l_thrd_error;
}

static inline void l_thrd_exit(int res)
{
	pthread_exit((void*)(size_t)res);
}

#define l_thrd_sleep(t,r) nanosleep(t,r)

static inline int l_mtx_init(l_mtx_t *mtx,int type)
{
	(void)type;
	int ret=pthread_mutex_init(mtx,NULL);
	return ret==0?l_thrd_success:l_thrd_error;
}

static inline int l_mtx_destroy(l_mtx_t *mtx)
{
	int ret=pthread_mutex_destroy(mtx);
	return ret==0?l_thrd_success:l_thrd_error;
}

static inline int l_mtx_lock(l_mtx_t *mtx)
{
	int ret=pthread_mutex_lock(mtx);
	return ret==0?l_thrd_success:l_thrd_error;
}

static inline int l_mtx_trylock(l_mtx_t *mtx)
{
	int ret=pthread_mutex_trylock(mtx);
	return ret==0?l_thrd_success:l_thrd_error;
}

static inline int l_mtx_unlock(l_mtx_t *mtx)
{
	int ret=pthread_mutex_unlock(mtx);
	return ret==0?l_thrd_success:l_thrd_error;
}

static inline int l_mtx_timedlock(l_mtx_t *mtx, const struct timespec *ts)
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME,&t);
	t.tv_sec+=ts->tv_sec;
	t.tv_nsec+=ts->tv_nsec;
	if(t.tv_nsec>=1000000000)
	{
		t.tv_nsec-=1000000000;
		t.tv_sec++;
	}
	int ret=pthread_mutex_timedlock(mtx,&t);
	return ret==0?l_thrd_success:l_thrd_error;
}

static inline int l_cnd_init(l_cnd_t *cnd)
{
	return pthread_cond_init(cnd,NULL)==0?l_thrd_success:l_thrd_error;
}

static inline int l_cnd_signal(l_cnd_t *cnd)
{
	return pthread_cond_signal(cnd)==0?l_thrd_success:l_thrd_error;
}

static inline int l_cnd_wait(l_cnd_t *cnd,l_mtx_t *mtx)
{
	return pthread_cond_wait(cnd,mtx)==0?l_thrd_success:l_thrd_error;
}

static inline int l_cnd_timedwait(l_cnd_t *cnd,l_mtx_t *mtx,const struct timespec *ts)
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME,&t);
	t.tv_sec+=ts->tv_sec;
	t.tv_nsec+=ts->tv_nsec;
	if(t.tv_nsec>=1000000000)
	{
		t.tv_nsec-=1000000000;
		t.tv_sec++;
	}
	return pthread_cond_timedwait(cnd,mtx,&t)==0?l_thrd_success:l_thrd_error;
}

#endif

#endif

#ifdef _WIN32
#define l_thrd_sleep_ms(ms) Sleep(ms)
#else
#define l_thrd_sleep_ms(ms) usleep((ms)*1000)
#endif

