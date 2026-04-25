#pragma once 

typedef struct {
	int (*sleep)(unsigned ms,LUserFunc cb,void *arg);
	int (*idle)(LUserFunc cb,void *arg);
	int (*main)(LUserFunc cb,void *arg);
	int (*poll)(int fd,int events,void (*cb)(int,int,void*),void *arg);
}L_LOOP_SCHED;

extern L_LOOP_SCHED l_sched;
int l_loop_sched(const L_LOOP_SCHED *p);

typedef struct _l_co_promise L_CO_PROMISE;
typedef void (*L_CO_RESOLVE)(L_CO_PROMISE *promise);

struct _l_co_promise {
	void *arg;
	L_CO_RESOLVE resolve;
	LFreeFunc free;
	void *result;
	int error;
	void *co;
};

typedef int (*L_CO_EXECUTER)(L_CO_PROMISE *promise);
int l_co_await(L_CO_EXECUTER executer,void *arg,void **result);

int l_co_init(void);
int l_co_exit(void);
void l_co_sched(void);
int l_co_create(void (*routine)(void*),void *arg);
int l_co_sleep(int ms);
int l_co_idle(void);
int l_co_yield(void);
int l_co_self(void);

