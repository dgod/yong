#include "llib.h"

#define STACK_SIZE		0x10000

#if !L_USE_C11_THREADS

#ifdef _WIN32

int l_thrd_create(l_thrd_t *thr,l_thrd_start_t func,void *arg)
{
	void **param=l_cnew(2,void*);
	param[0]=func;
	param[1]=arg;
	*thr = CreateThread(NULL,STACK_SIZE,l_winthread_wrapper,param,0,NULL);
	if(*thr != NULL)
		return l_thrd_success;
	l_free(param);
	return l_thrd_error;
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
	mtx->type=type;
	if(type==l_mtx_timed)
	{
		mtx->timed=CreateMutex(NULL,FALSE,NULL);
		if(mtx->timed==NULL)
			return l_thrd_error;
	}
	else
	{
		InitializeCriticalSection(&mtx->plain);
	}
	return l_thrd_success;
}

int l_mtx_destroy(l_mtx_t *mtx)
{
	if(mtx->type==l_mtx_timed)
	{
		BOOL ret=CloseHandle(mtx->timed);
		return ret?l_thrd_success:l_thrd_error;
	}
	else
	{
		DeleteCriticalSection(&mtx->plain);
		return l_thrd_success;
	}
}

int l_mtx_lock(l_mtx_t *mtx)
{
	if(mtx->type==l_mtx_timed)
		return WaitForSingleObject(mtx->timed,INFINITE)==WAIT_OBJECT_0?l_thrd_success:l_thrd_busy;
	EnterCriticalSection(&mtx->plain);
	return l_thrd_success;
}

int l_mtx_trylock(l_mtx_t *mtx)
{
	if(mtx->type==l_mtx_timed)
		return WaitForSingleObject(mtx->timed,0)==WAIT_OBJECT_0?l_thrd_success:l_thrd_busy;
	return TryEnterCriticalSection(&mtx->plain)?l_thrd_success:l_thrd_busy;
}

int l_mtx_unlock(l_mtx_t *mtx)
{
	if(mtx->type==l_mtx_timed)
		return ReleaseMutex(mtx->timed)?l_thrd_success:l_thrd_error;
	LeaveCriticalSection(&mtx->plain);
	return l_thrd_success;
}

int l_mtx_timedlock(l_mtx_t *mtx, const struct timespec *ts)
{
	if(mtx->type!=l_mtx_timed)
		return l_thrd_error;
	DWORD ret=WaitForSingleObject(mtx->timed,ts?ts->tv_sec*1000+ts->tv_nsec/1000000:INFINITE);
	if(ret==WAIT_OBJECT_0)
		return l_thrd_success;
	if(ret==WAIT_TIMEOUT)
		return l_thrd_timedout;
	return l_thrd_error;
}

int l_cnd_init(l_cnd_t *cond)
{
#ifdef _WIN64
	InitializeConditionVariable(cond);
#else
	cond->event=CreateEvent(NULL,FALSE,FALSE,NULL);
	if(!cond->event)
		return l_thrd_error;
	cond->has_waiter=false;
#endif
	return l_thrd_success;
}

void l_cnd_destroy(l_cnd_t *cond)
{
#ifdef _WIN64
	(void)cond;
#else
	if(cond && cond->event)
	{
		CloseHandle(cond->event);
		cond->event=NULL;
	}
#endif
}

int l_cnd_signal(l_cnd_t *cond)
{
#ifdef _WIN64
	WakeConditionVariable(cond);
#else
	if(cond->has_waiter)
	{
		cond->has_waiter=false;
		return SetEvent(cond->event)?l_thrd_success:l_thrd_error;
	}
#endif
	return l_thrd_success;
}

int l_cnd_wait(l_cnd_t *cond,l_mtx_t *mtx)
{
#if _WIN64
	if(mtx->type==l_mtx_timed)
		return l_thrd_error;
	BOOL ret=SleepConditionVariableCS(cond,&mtx->plain,INFINITE);
	return ret?l_thrd_success:l_thrd_error;
#else
	cond->has_waiter=true;
	if(l_mtx_unlock(mtx)!=l_thrd_success)
		return l_thrd_error;
	DWORD ret=WaitForSingleObject(cond->event,INFINITE);
	if(ret!=WAIT_OBJECT_0)
	{
		l_mtx_lock(mtx);
		return l_thrd_error;
	}
	if(l_mtx_lock(mtx)!=l_thrd_success)
		return l_thrd_error;
	return l_thrd_success;
#endif
}

int l_cnd_timedwait(l_cnd_t *cond,l_mtx_t *mtx,const struct timespec *ts)
{
	DWORD dwMilliseconds=ts?ts->tv_sec*1000+ts->tv_nsec/1000000:INFINITE;
#ifdef _WIN64
	if(!ts)
		return l_cnd_wait(cond,mtx);
	if(mtx->type==l_mtx_timed)
		return l_thrd_error;
	BOOL ret=SleepConditionVariableCS(cond,&mtx->plain,dwMilliseconds);
	if(ret==TRUE)
		return l_thrd_success;
	if(GetLastError()==ERROR_TIMEOUT)
		return l_thrd_timedout;
	return l_thrd_error;
#else
	cond->has_waiter=true;
	if(l_mtx_unlock(mtx)!=l_thrd_success)
		return l_thrd_error;
	DWORD ret=WaitForSingleObject(cond->event,dwMilliseconds);
	if(ret==WAIT_TIMEOUT)
	{
		l_mtx_lock(mtx);
		return l_thrd_timedout;
	}
	if(ret!=WAIT_OBJECT_0)
	{
		l_mtx_lock(mtx);
		return l_thrd_error;
	}
	if(l_mtx_lock(mtx)!=l_thrd_success)
		return l_thrd_error;
	return l_thrd_success;
#endif
}
#else

int l_thrd_create(l_thrd_t *thr,l_thrd_start_t func,void *arg)
{
	void **param=l_cnew(2,void*);
	param[0]=func;
	param[1]=arg;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr,STACK_SIZE);
	int ret=pthread_create(thr,&attr,l_pthread_wrapper,param);
	pthread_attr_destroy(&attr);
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
		return ret==0?l_thrd_success:l_thrd_error;
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
		if(ret==0)
			return l_thrd_success;
		if(errno==ETIMEDOUT)
			return l_thrd_timedout;
		return l_thrd_error;
	}
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
		if(ret==0)
			return l_thrd_success;
		if(errno==ETIMEDOUT)
			return l_thrd_timedout;
		return l_thrd_error;
	}
	else
	{
		ret=pthread_cond_wait(cnd,mtx);
		return ret==0?l_thrd_success:l_thrd_error;
	}
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

typedef struct{
	void *next;
	LUserFunc func;
	void *arg;
	bool run;
	bool exclusive;
	atomic_bool done;
}THRDP_ITEM;

typedef struct{
	void *next;
	l_cnd_t cnd;
	THRDP_ITEM *item;
	bool exit;
}THRDP_THREAD;

static struct{
	int num;
	THRDP_THREAD *threads;
	THRDP_ITEM *items;
	l_mtx_t mtx;
	l_cnd_t cnd;
}tp;

int l_thrdp_init(int num)
{
	tp.num=num?num:4;
	l_mtx_init(&tp.mtx,l_mtx_plain);
	l_cnd_init(&tp.cnd);
	return 0;
}

static int thrdp_runner(THRDP_THREAD *self);
static int thrdp_get_idle(THRDP_ITEM *item)
{
	THRDP_THREAD *t=tp.threads;
	if(item->exclusive)
	{
		for(t=tp.threads;t!=NULL;t=t->next)
		{
			if(t->item && t->item->func == item->func)
				return 1;
		}
	}
	for(t=tp.threads;t!=NULL;t=t->next)
	{
		if(t->item)
			continue;
		t->item=item;
		item->run=true;
		l_cnd_signal(&t->cnd);
		return 0;
	}
	if(l_slist_length(tp.threads)>=tp.num)
		return -1;
	t=l_new(THRDP_THREAD);
	t->item=item;
	t->exit=false;
	l_cnd_init(&t->cnd);
	l_thrd_t thr;
	if(l_thrd_success!=l_thrd_create(&thr,(l_thrd_start_t)thrdp_runner,t))
	{
		l_cnd_destroy(&t->cnd);
		l_free(t);
		return -1;
	}
	item->run=true;
	l_thrd_detach(thr);
	tp.threads=l_slist_prepend(tp.threads,t);
	return 0;
}
static void thrdp_sched(THRDP_ITEM *item)
{
	l_mtx_lock(&tp.mtx);
	int threads=0,idle=0,items=0,done=0;
	for(THRDP_THREAD *t=tp.threads,*n;t!=NULL;t=n)
	{
		n=t->next;
		if(t->exit)
		{
			tp.threads=l_slist_remove(tp.threads,t);
			l_cnd_destroy(&t->cnd);
			l_free(t);
		}
		else
		{
			threads++;
			if(!t->item)
				idle++;
		}
	}
	if(item)
		tp.items=l_slist_append(tp.items,item);
	for(THRDP_ITEM *it=tp.items,*n;it!=NULL;it=n)
	{
		n=it->next;
		if(atomic_load(&it->done))
		{
			tp.items=l_slist_remove(tp.items,it);
			l_free(it);
			done++;
		}
		else
		{
			items++;
		}
	}
	if(done)
	{
		l_cnd_signal(&tp.cnd);
	}
	if(items==0 || (idle==0 && threads>=tp.num))
	{
		l_mtx_unlock(&tp.mtx);
		return;
	}
	for(THRDP_ITEM *it=tp.items;it!=NULL;it=it->next)
	{
		if(it->run)
			continue;
		if(thrdp_get_idle(it)<0)
			break;
	}
	l_mtx_unlock(&tp.mtx);
}

static int thrdp_runner(THRDP_THREAD *self)
{
	while(1)
	{
		if(self->item)
		{
			THRDP_ITEM *it=self->item;
			it->func(it->arg);
			atomic_store(&it->done,true);
			self->item=NULL;
		}
		thrdp_sched(NULL);
		l_mtx_lock(&tp.mtx);
		if(!self->item)
		{
			int ret=l_cnd_timedwait_ms(&self->cnd,&tp.mtx,5000);
			if(ret!=l_thrd_success && !self->item)
			{
				l_mtx_unlock(&tp.mtx);
				break;
			}
		}
		l_mtx_unlock(&tp.mtx);
	}
	self->exit=true;
	thrdp_sched(NULL);
	return 0;
}

int l_thrdp_run(LUserFunc func,void *arg,bool exclusive)
{
	if(tp.num<=0)
		return -1;
	THRDP_ITEM *it=l_new(THRDP_ITEM);
	it->func=func;
	it->arg=arg;
	it->run=false;
	it->exclusive=exclusive;
	atomic_init(&it->done,false);
	thrdp_sched(it);
	return 0;
}

int l_thrdp_wait(LUserFunc func,int timeout)
{
	uint64_t end_time=l_ticks()+timeout;
	int result=0;
	l_mtx_lock(&tp.mtx);
	do{
		bool done=true;
		for(THRDP_ITEM *it=tp.items;it!=NULL;it=it->next)
		{
			if(it->func==func && !it->done)
			{
				done=false;
				break;
			}
		}
		if(done)
		{
			break;
		}
		if(l_ticks()>=end_time)
		{
			result=-1;
			break;
		}
		int ret=l_cnd_timedwait_ms(&tp.cnd,&tp.mtx,50);
		if(ret==l_thrd_error)
			break;
	}while(1);
	l_mtx_unlock(&tp.mtx);
	return result;
}

