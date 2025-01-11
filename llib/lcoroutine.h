#pragma once 

typedef struct {
	int (*sleep)(unsigned ms,void (*cb)(void*),void *arg);
}L_CO_EVENTS;

typedef struct {
	int stack_size;
	L_CO_EVENTS events;
}L_CO_INIT;

int l_co_init(L_CO_INIT *init);
int l_co_exit(void);
void l_co_sched(void);
int l_co_create(void (*routine)(void*),void *arg);
int l_co_sleep(int ms);
int l_co_yield(void);
int l_co_self(void);

