#include "lconfig.h"
#include "ltypes.h"
#include "lslist.h"
#include "lmem.h"
#include "lcoroutine.h"
#include "lthreads.h"
#include "ltricky.h"

#include <unistd.h>

L_LOOP_SCHED l_sched;
int l_loop_sched(const L_LOOP_SCHED *p)
{
	l_sched=*p;
	return 0;
}

enum{
	INIT,
	RUNNING,
	SUSPEND,
	EXIT,
};

#ifdef _WIN32
#define l_gettid() GetCurrentThreadId()
#else
#define l_gettid() gettid()
#endif

#define STACK_SIZE	0x10000

#if L_USE_COROUTINE==2
	#include <setjmp.h>
	#define STACK_RESERVED (512*1024)
#else
	#ifdef _WIN32
		#include <windows.h>
	#else
		#include <sys/mman.h>
		#if L_USE_COROUTINE==3
			#undef L_USE_COROUTINE
			#define L_USE_COROUTINE 1
			#include <libucontext/libucontext.h>
			typedef libucontext_ucontext_t ucontext_t;
			#define getcontext(ucp) libucontext_getcontext(ucp)
			#define makecontext(ucp,func,argc,arg) libucontext_makecontext(ucp,func,argc,arg)
			#define setcontext(ucp) libucontext_setcontext(ucp)
			#define swapcontext(oucp,ucp) libucontext_swapcontext(oucp,ucp)
		#else
			#include <ucontext.h>
		#endif
	#endif
#endif

#if L_USE_COROUTINE

typedef struct{
	void *next;
	void (*routine)(void*);
	void *arg;
	int id;
	int state;

#if L_USE_COROUTINE==2
	int stack;
	uint8_t *stack_pad;
	jmp_buf ctx;
#else
	#ifdef _WIN32
		PVOID ctx;
	#else
		ucontext_t ctx;
		uint8_t *stack;
	#endif
#endif

#ifdef __ANDROID__
	bool pending_sleep;
	unsigned sleep_time;
	bool pending_idle;
#endif
}coroutine_t;

struct{
	int id;
	int stack_size;
	coroutine_t *routines;
	coroutine_t *current;
	coroutine_t *dead;

#ifdef _WIN32
	DWORD tid;
#else
	pid_t tid;
#endif

#if L_USE_COROUTINE==2
#define MAX_COROUTINE	4
	uintptr_t stack_top;
	uint8_t stack[MAX_COROUTINE];
	jmp_buf main;
#else
	#ifdef _WIN32
		PVOID main;
	#else
		ucontext_t main;
	#endif
#endif
}sched;

static inline int get_id(void)
{
	int ret=sched.id++;
	if(sched.id==0x7fffffff)
		sched.id=1;
	return ret;
}

int l_co_init(void)
{
	sched.id=1;
	sched.stack_size=STACK_SIZE;
	sched.routines=NULL;
	sched.current=NULL;

	sched.tid=l_gettid();

#if L_USE_COROUTINE==1
#ifdef _WIN32
	sched.main=ConvertThreadToFiber(NULL);
#else
	getcontext(&sched.main);
#endif
#elif L_USE_COROUTINE==2
	int stack_top;
	sched.stack_top=(uintptr_t)&stack_top;
#endif
	return 0;
}

static void free_dead(void)
{
	while(sched.dead)
	{
		coroutine_t *co=sched.dead;
		sched.dead=co->next;
#if L_USE_COROUTINE==1
#ifdef _WIN32
		DeleteFiber(co->ctx);
#else
		munmap(co->stack,sched.stack_size);
#endif
#elif L_USE_COROUTINE==2
		sched.stack[co->stack]=0;
#endif
		l_free(co);
	}
}

int l_co_create(void (*routine)(void*),void *arg)
{
	coroutine_t *co;
	if(sched.dead!=NULL)
	{
		co=sched.dead;
		sched.dead=co->next;
#if L_USE_COROUTINE==1 && defined(_WIN32)
		DeleteFiber(co->ctx);
		co->ctx=NULL;
#endif
	}
	else
	{
		co=l_new0(coroutine_t);
#if L_USE_COROUTINE==1 && !defined(_WIN32)
		co->stack=mmap(NULL,sched.stack_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#elif L_USE_COROUTINE==2
		co->stack=array_index(sched.stack,MAX_COROUTINE,(uint8_t)0);
		if(co->stack==-1)
		{
			l_free(co);
			return -1;
		}
		sched.stack[co->stack]=1;
#endif
	}
	sched.routines=l_slist_prepend(sched.routines,co);
	co->routine=routine;
	co->arg=arg;
	co->state=INIT;
	co->id=get_id();
	return co->id;
}

int l_co_exit(void)
{
	coroutine_t *co=sched.current;
	if(co==NULL)
		return -1;
	free_dead();
	co->state=EXIT;
	sched.routines=l_slist_remove(sched.routines,co);
	sched.dead=l_slist_prepend(sched.dead,co);
	l_co_sched();
	return -1;
}

static void resume(coroutine_t *co)
{
	co->state=RUNNING;
	l_co_sched();
}

int l_co_sleep(int ms)
{
	if(!l_sched.sleep)
	{
		return -1;
	}
	coroutine_t *co=sched.current;
	if(!co)
	{
		return -1;
	}
	co->state=SUSPEND;
#if defined(__ANDROID__) && L_USE_COROUTINE==1
	co->pending_sleep=true;
	co->sleep_time=ms;
#else
	int ret=l_sched.sleep(ms,(void*)resume,co);
	if(ret!=0)
	{
		co->state=RUNNING;
		return -1;
	}
#endif
	l_co_sched();
	return 0;
}

int l_co_idle(void)
{
	if(!l_sched.idle)
		return -1;
	coroutine_t *co=sched.current;
	if(!co)
		return -1;
	co->state=SUSPEND;
#if defined(__ANDROID__) && L_USE_COROUTINE==1
	co->pending_idle=true;
#else
	int ret=l_sched.idle((void*)resume,co);
	if(ret!=0)
	{
		co->state=RUNNING;
		return -1;
	}
#endif
	l_co_sched();
	return 0;
}

int l_co_yield(void)
{
	coroutine_t *co=sched.current;
	if(co!=NULL)
	{
		sched.routines=l_slist_remove(sched.routines,co);
		sched.routines=l_slist_append(sched.routines,co);
	}
	l_co_sched();
	return 0;
}

int l_co_self(void)
{
	coroutine_t *co=sched.current;
	return co?co->id:0;
}

#if L_USE_CO_AWAIT

static void co_resolve(L_CO_PROMISE *promise)
{
	if(l_gettid()!=sched.tid)
	{
		l_sched.main((LUserFunc)co_resolve,promise);
		return;
	}
	coroutine_t *co=promise->co;
	co->state=RUNNING;
	l_co_sched();	
}

int l_co_await(L_CO_EXECUTER executer,void *arg,void **result)
{
	if(result)
		*result=NULL;
	coroutine_t *co=sched.current;
	if(!co)
	{
		return -1;
	}
	L_CO_PROMISE promise={
		.arg=arg,
		.resolve=co_resolve,
		.free=NULL,
		.co=co,
		.result=NULL,
		.error=0,
	};
	co->state=SUSPEND;
	int ret=executer(&promise);
	if(ret!=0)
	{
		co->state=RUNNING;
		return ret;
	}
	l_co_sched();
	if(result)
		*result=promise.result;
	if(promise.free)
		promise.free(arg);
	return promise.error;
}

static int thrd_wrapper(L_CO_PROMISE *promise)
{
	l_thrd_start_t func=((void**)promise->arg)[0];
	void *arg=((void**)promise->arg)[1];
	int ret=func(arg);
	promise->error=ret;
	co_resolve(promise);
	return 0;
}

static int thrd_executor(L_CO_PROMISE *promise)
{
	l_thrd_t thr;
	int ret=l_thrd_create(&thr,(l_thrd_start_t)thrd_wrapper,promise);
	if(ret!=0)
		return -1;
	l_thrd_detach(thr);
	return 0;
}

int l_co_await_thrd(l_thrd_start_t func,void *data)
{
	void *arr[2]={func,data};
	return l_co_await(thrd_executor,arr,NULL);
}

#endif

#endif

#if L_USE_COROUTINE==1

#ifdef _WIN32
static void WINAPI wrapper(coroutine_t *co)
{
	co->routine(co->arg);
	l_co_exit();
}
#else
static void wrapper(coroutine_t *co)
{
	co->routine(co->arg);
	l_co_exit();
}
#endif

void l_co_sched(void)
{
	for(coroutine_t *co=sched.routines;co!=NULL;co=co->next)
	{
		if(co->state==INIT)
		{
#ifndef _WIN32
			getcontext(&co->ctx);
			co->ctx.uc_stack.ss_sp=co->stack;
			co->ctx.uc_stack.ss_size=sched.stack_size;
			co->ctx.uc_stack.ss_flags=0;
			co->ctx.uc_link=NULL;
			makecontext(&co->ctx,(void*)wrapper,1,co);
#else
			co->ctx=CreateFiber(sched.stack_size,(void*)wrapper,co);
#endif
			co->state=RUNNING;
			goto DO_RUN;
		}
		else if(co->state==RUNNING)
		{
			if(sched.current==co)
				return;
DO_RUN:
#ifdef _WIN32
			sched.current=co;
			SwitchToFiber(co->ctx);
#else
			if(sched.current==NULL)	
			{
				sched.current=co;
				swapcontext(&sched.main,&co->ctx);
				sched.current=NULL;
				break;
			}
			ucontext_t *ctx=&sched.current->ctx;
			sched.current=co;
			swapcontext(ctx,&co->ctx);
#endif
			return;
		}
	}
	if(sched.current!=NULL)
	{
#ifdef _WIN32
		sched.current=NULL;
		SwitchToFiber(sched.main);
#else
		coroutine_t *co=sched.current;
		sched.current=NULL;
		swapcontext(&co->ctx,&sched.main);
#endif
	}
	free_dead();
#ifdef __ANDROID__
	for(coroutine_t *co=sched.routines;co!=NULL;co=co->next)
	{
		if(co->state!=SUSPEND)
			continue;
		if(co->pending_idle)
		{
			co->pending_idle=false;
			sched.events.idle((void*)resume,co);
		}
		else if(co->pending_sleep)
		{
			co->pending_sleep=false;
			sched.events.sleep(co->sleep_time,(void*)resume,co);
		}
	}
#endif
}

#elif L_USE_COROUTINE==2

static void wrapper(coroutine_t *co)
{
	if(setjmp(sched.main))
		return;
	// save to stack_pad, avoid compiler optimize the code away
	co->stack_pad=l_alloca(STACK_RESERVED-(int)(sched.stack_top-(uintptr_t)&co)-co->stack*sched.stack_size);
	co->routine(co->arg);
	l_co_exit();
}

void l_co_sched(void)
{
restart:
	for(coroutine_t *co=sched.routines;co!=NULL;co=co->next)
	{
		if(co->state==INIT)
		{
			if(sched.current!=NULL)
			{
				if(setjmp(sched.current->ctx))
					return;
				sched.current=NULL;
				longjmp(sched.main,1);
			}
			co->state=RUNNING;
			sched.current=co;
			wrapper(co);
			goto restart;
		}
		else if(co->state==RUNNING)
		{
			if(sched.current==co)
				return;
			coroutine_t *cur=sched.current;
			sched.current=co;
			if(setjmp(cur?cur->ctx:sched.main))
			{
				if(cur==NULL)
					goto PENDING;
				return;
			}
			longjmp(co->ctx,1);
			return;
		}
	}
	if(sched.current!=NULL)
	{
		coroutine_t *co=sched.current;
		sched.current=NULL;
		if(setjmp(co->ctx))
			return;
		free_dead();
		longjmp(sched.main,1);
	}
PENDING:
	free_dead();
}

#endif

