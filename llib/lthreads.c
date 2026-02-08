#include "llib.h"

#if !L_USE_C11_THREADS

#ifdef _WIN32

int l_thrd_create(l_thrd_t *thr,l_thrd_start_t func,void *arg)
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

int l_thrd_join(l_thrd_t thr,int *res)
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

int l_thrd_detach(l_thrd_t thr)
{
	return CloseHandle(thr)?l_thrd_success:l_thrd_busy;
}

int l_thrd_sleep(const struct timespec *time_point,struct timespec *remain)
{
	Sleep(time_point->tv_sec*1000+time_point->tv_nsec/1000000);
	if(remain)
		remain->tv_sec=remain->tv_nsec=0;
	return 0;
}

int l_mtx_init(l_mtx_t *mtx,int type)
{
	(void)type;
	*mtx=CreateMutex(NULL,FALSE,NULL);
	return *mtx!=NULL?l_thrd_success:l_thrd_error;
}

int l_mtx_destroy(l_mtx_t *mtx)
{
	BOOL ret=CloseHandle(*mtx);
	*mtx=NULL;
	return ret?l_thrd_success:l_thrd_error;
}

int l_mtx_lock(l_mtx_t *mtx)
{
	return WaitForSingleObject(*mtx,INFINITE)==WAIT_OBJECT_0?l_thrd_success:l_thrd_busy;
}

int l_mtx_trylock(l_mtx_t *mtx)
{
	return WaitForSingleObject(*mtx,0)==WAIT_OBJECT_0?l_thrd_success:l_thrd_busy;
}

int l_mtx_unlock(l_mtx_t *mtx)
{
	return ReleaseMutex(*mtx)?l_thrd_success:l_thrd_busy;
}

int l_mtx_timedlock(l_mtx_t *mtx, const struct timespec *ts)
{
	return WaitForSingleObject(*mtx,ts?ts->tv_sec*1000+ts->tv_nsec/1000000:INFINITE)>=WAIT_OBJECT_0?l_thrd_success:l_thrd_busy;
}

int l_cnd_init(l_cnd_t *cond)
{
	cond->event=CreateEvent(NULL,FALSE,FALSE,NULL);
	if(!cond->event)
		return l_thrd_error;
	cond->has_waiter=false;
	return l_thrd_success;
}

void l_cnd_destroy(l_cnd_t *cond)
{
	if(cond && cond->event)
	{
		CloseHandle(cond->event);
		cond->event=NULL;
	}
}

int l_cnd_signal(l_cnd_t *cond)
{
	if(cond->has_waiter)
	{
		cond->has_waiter=false;
		return SetEvent(cond->event)?l_thrd_success:l_thrd_error;
	}
	return l_thrd_success;
}

int l_cnd_wait(l_cnd_t *cond,l_mtx_t *mtx)
{
	cond->has_waiter=true;
	if(TRUE!=ReleaseMutex(*mtx))
		return l_thrd_error;
	DWORD ret=WaitForSingleObject(cond->event,INFINITE);
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

int l_cnd_timedwait(l_cnd_t *cond,l_mtx_t *mtx,const struct timespec *ts)
{
	cond->has_waiter=true;
	if(TRUE!=ReleaseMutex(*mtx))
		return l_thrd_error;
	DWORD ret=WaitForSingleObject(cond->event,ts?ts->tv_sec*1000+ts->tv_nsec/1000000:INFINITE);
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

int l_thrd_create(l_thrd_t *thr,l_thrd_start_t func,void *arg)
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

int l_thrd_join(l_thrd_t thr,int *res)
{
	void *temp;
	int ret=pthread_join(thr,&temp);
	if(ret==0 && res)
		*res=(int)(size_t)temp;
	return ret==0?l_thrd_success:l_thrd_error;
}

int l_thrd_detach(l_thrd_t thr)
{
	int ret=pthread_detach(thr);
	return ret==0?l_thrd_success:l_thrd_error;
}

int l_mtx_init(l_mtx_t *mtx,int type)
{
	(void)type;
	int ret=pthread_mutex_init(mtx,NULL);
	return ret==0?l_thrd_success:l_thrd_error;
}

int l_mtx_destroy(l_mtx_t *mtx)
{
	int ret=pthread_mutex_destroy(mtx);
	return ret==0?l_thrd_success:l_thrd_error;
}

int l_mtx_lock(l_mtx_t *mtx)
{
	int ret=pthread_mutex_lock(mtx);
	return ret==0?l_thrd_success:l_thrd_error;
}

int l_mtx_trylock(l_mtx_t *mtx)
{
	int ret=pthread_mutex_trylock(mtx);
	return ret==0?l_thrd_success:l_thrd_error;
}

int l_mtx_unlock(l_mtx_t *mtx)
{
	int ret=pthread_mutex_unlock(mtx);
	return ret==0?l_thrd_success:l_thrd_error;
}

int l_mtx_timedlock(l_mtx_t *mtx, const struct timespec *ts)
{
	int ret;
	if(!ts)
	{
		ret=pthread_mutex_lock(mtx);
	}
	else
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
		ret=pthread_mutex_timedlock(mtx,&t);
	}
	return ret==0?l_thrd_success:l_thrd_error;
}

int l_cnd_init(l_cnd_t *cnd)
{
	return pthread_cond_init(cnd,NULL)==0?l_thrd_success:l_thrd_error;
}

int l_cnd_signal(l_cnd_t *cnd)
{
	return pthread_cond_signal(cnd)==0?l_thrd_success:l_thrd_error;
}

#define l_cnd_destroy(cnd) pthread_cond_destroy(cnd)

int l_cnd_wait(l_cnd_t *cnd,l_mtx_t *mtx)
{
	return pthread_cond_wait(cnd,mtx)==0?l_thrd_success:l_thrd_error;
}

int l_cnd_timedwait(l_cnd_t *cnd,l_mtx_t *mtx,const struct timespec *ts)
{
	int ret;
	if(ts)
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
		ret=pthread_cond_timedwait(cnd,mtx,&t);
	}
	else
	{
		ret=pthread_cond_wait(cnd,mtx);
	}
	return ret==0?l_thrd_success:l_thrd_error;
}
#endif

#endif // !L_USE_C11_THREADS

int l_cnd_timedwait_ms(l_cnd_t *cnd,l_mtx_t *mtx,int ms)
{
	if(ms<0)
		return l_cnd_wait(cnd,mtx);
	struct timespec ts={
		.tv_sec=ms/1000,
		.tv_nsec=(ms%1000)*1000000
	};
	return l_cnd_timedwait(cnd,mtx,&ts);
}

