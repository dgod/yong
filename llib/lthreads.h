#pragma once

#include <time.h>
#include "lmem.h"
#include "lfuncs.h"

#ifndef L_USE_C11_THREADS
#define L_USE_C11_THREADS 0
#else
#if !__has_include("threads.h")
#undef L_USE_C11_THREADS
#define L_USE_C11_THREADS 0
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
typedef struct{
	HANDLE event;
	bool has_waiter;
}l_cnd_t;

DWORD WINAPI l_winthread_wrapper(void *param);

int l_thrd_create(l_thrd_t *thr,l_thrd_start_t func,void *arg);
int l_thrd_join(l_thrd_t thr,int *res);
int l_thrd_detach(l_thrd_t thr);
#define l_thrd_exit(res) ExitThread((DWORD)(res))
int l_thrd_sleep(const struct timespec *time_point,struct timespec *remain);
int l_mtx_init(l_mtx_t *mtx,int type);
int l_mtx_destroy(l_mtx_t *mtx);
int l_mtx_lock(l_mtx_t *mtx);
int l_mtx_trylock(l_mtx_t *mtx);
int l_mtx_unlock(l_mtx_t *mtx);
int l_mtx_timedlock(l_mtx_t *mtx, const struct timespec *ts);
int l_cnd_init(l_cnd_t *cond);
void l_cnd_destroy(l_cnd_t *cond);
int l_cnd_signal(l_cnd_t *cond);
int l_cnd_wait(l_cnd_t *cond,l_mtx_t *mtx);
int l_cnd_timedwait(l_cnd_t *cond,l_mtx_t *mtx,const struct timespec *ts);

#else

#include <pthread.h>
#include <unistd.h>

#define l_thread_local __thread

typedef pthread_t l_thrd_t;
typedef pthread_mutex_t l_mtx_t;
typedef pthread_cond_t l_cnd_t;

void *l_pthread_wrapper(void *param);
int l_thrd_create(l_thrd_t *thr,l_thrd_start_t func,void *arg);
int l_thrd_join(l_thrd_t thr,int *res);
int l_thrd_detach(l_thrd_t thr);
void l_thrd_exit(int res);
#define l_thrd_exit(res) pthread_exit((void*)(size_t)(res))
#define l_thrd_sleep(t,r) nanosleep(t,r)
int l_mtx_init(l_mtx_t *mtx,int type);
int l_mtx_destroy(l_mtx_t *mtx);
int l_mtx_lock(l_mtx_t *mtx);
int l_mtx_trylock(l_mtx_t *mtx);
int l_mtx_unlock(l_mtx_t *mtx);
int l_mtx_timedlock(l_mtx_t *mtx, const struct timespec *ts);
int l_cnd_init(l_cnd_t *cnd);
int l_cnd_signal(l_cnd_t *cnd);
#define l_cnd_destroy(cnd) pthread_cond_destroy(cnd)
int l_cnd_wait(l_cnd_t *cnd,l_mtx_t *mtx);
int l_cnd_timedwait(l_cnd_t *cnd,l_mtx_t *mtx,const struct timespec *ts);

#endif // _WIN32
#endif // L_USE_C11_THREADS

#ifdef _WIN32
#define l_thrd_sleep_ms(ms) Sleep(ms)
#else
#define l_thrd_sleep_ms(ms) usleep((ms)*1000)
#endif

int l_cnd_timedwait_ms(l_cnd_t *cnd,l_mtx_t *mtx,int ms);

