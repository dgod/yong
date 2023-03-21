#include "llib.h"
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "common.h"

typedef struct{
#ifdef _WIN32
	HANDLE mut;
	HANDLE th;
#else
	pthread_mutex_t mut;
	pthread_t th;
#endif
	LQueue *que;
	bool run;
	bool wait;
}YONG_ASYNC;

typedef struct{
	LList;
	void (*run)(void*);
	LFreeFunc free;
}ASYNC_TASK;

typedef struct{
	ASYNC_TASK;
	char *file;
	LString *data;
	bool backup;
}ASYNC_TASK_WRITE_FILE;

static YONG_ASYNC async;

#ifdef _WIN32
#define LOCK() WaitForSingleObject(async.mut,INFINITE)
#define UNLOCK() ReleaseMutex(async.mut)
#else
#define LOCK() pthread_mutex_lock(&async.mut)
#define UNLOCK() pthread_mutex_unlock(&async.mut)
#endif

static void async_task_free(ASYNC_TASK *t)
{
	if(!t)
		return;
	if(t->free)
		t->free(t);
	l_free(t);
}

static void async_task(void *unused)
{
	ASYNC_TASK *t;
	while(1)
	{
		LOCK();
		t=l_queue_pop_head(async.que);
		if(!t)
			break;
		UNLOCK();
		t->run(t);
		async_task_free(t);
	}
	async.run=false;
	async.wait=false;
#ifdef _WIN32
	CloseHandle(async.th);
	async.th=NULL;
#else
	pthread_detach(async.th);
#endif
	UNLOCK();
}

int y_im_async_init(void)
{
	memset(&async,0,sizeof(async));
#ifdef _WIN32
	async.mut=CreateMutex(NULL,FALSE,NULL);
#else
	pthread_mutex_init(&async.mut,NULL);
#endif
	async.que=l_queue_new((LFreeFunc)async_task_free);
	return 0;
}

void y_im_async_destroy(void)
{
	LOCK();
	if(async.wait || async.run)
	{
#ifdef _WIN32
	WaitForSingleObject(async.th,INFINITE);
	CloseHandle(async.th);
#else
	pthread_join(async.th,NULL);
#endif
	}
	UNLOCK();
#ifdef _WIN32
	CloseHandle(async.mut);
#else
	pthread_mutex_destroy(&async.mut);
#endif
	l_queue_free(async.que);
	memset(&async,0,sizeof(async));
}

#ifdef _WIN32
static DWORD WINAPI async_thread(void *unused)
{
	async_task(unused);
	return 0;
}
#else
static void *async_thread(void *unused)
{
	async_task(unused);
	return NULL;
}
#endif

static void async_task_add(ASYNC_TASK *t)
{
	LOCK();
	if(async.wait)
	{
#ifdef _WIN32
		WaitForSingleObject(async.th,INFINITE);
		CloseHandle(async.th);
#else
		pthread_join(async.th,NULL);
#endif
		async.wait=false;
	}
	l_queue_push_tail(async.que,t);
	if(!async.run)
	{
		async.run=true;
#ifdef _WIN32
		DWORD tid;
		async.th=CreateThread(NULL,0,async_thread,NULL,0,&tid);
		(void)tid;
#else
		pthread_create(&async.th,NULL,async_thread,NULL);
#endif
	}
	UNLOCK();
}

static void async_task_write_file_free(ASYNC_TASK_WRITE_FILE *t)
{
	l_free(t->file);
	l_string_free(t->data);
}

static void async_task_write_file_run(ASYNC_TASK_WRITE_FILE *t)
{
	const char *fn=t->file;

	if(!fn)
		return;
	if(t->backup)
	{
		char orig[252];
		char dest[256];
		sprintf(orig,"%s/%s",y_im_get_path("HOME"),fn);
		sprintf(dest,"%s.bak",orig);
		l_remove(dest);
		rename(orig,dest);
	}
	FILE *fp=y_im_open_file(fn,"wb");
	if(!fp)
		return;
	fwrite(t->data->str,t->data->len,1,fp);
	fclose(fp);
}

int y_im_async_write_file(const char *file,LString *data,bool backup)
{
	if(!file || !data)
	{
		l_string_free(data);
		return -1;
	}
	ASYNC_TASK_WRITE_FILE *t=l_new(ASYNC_TASK_WRITE_FILE);
	t->file=l_strdup(file);
	t->data=data;
	t->backup=backup;
	t->free=(void*)async_task_write_file_free;
	t->run=(void*)async_task_write_file_run;
	async_task_add((ASYNC_TASK*)t);
	return 0;
}

int y_im_async_wait(int timeout)
{
	do{
		if(!async.run)
			return 1;
#ifdef _WIN32
		Sleep(50);
#else
		usleep(50*1000);
#endif
	}while(timeout>0);
	return -1;
}

