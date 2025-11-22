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
	l_mtx_t mut;
	l_thrd_t th;
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

#define LOCK() l_mtx_lock(&async.mut)
#define UNLOCK() l_mtx_unlock(&async.mut)

static void async_task_free(ASYNC_TASK *t)
{
	if(!t)
		return;
	if(t->free)
		t->free(t);
	l_free(t);
}

static int async_task(void *unused)
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
	l_thrd_detach(async.th);
	UNLOCK();
	return 0;
}

int y_im_async_init(void)
{
	memset(&async,0,sizeof(async));
	l_mtx_init(&async.mut,l_mtx_plain);
	async.que=l_queue_new((LFreeFunc)async_task_free);
	return 0;
}

void y_im_async_destroy(void)
{
	LOCK();
	if(async.wait || async.run)
	{
		l_thrd_join(async.th,NULL);
	}
	UNLOCK();
	l_mtx_destroy(&async.mut);
	l_queue_free(async.que);
	memset(&async,0,sizeof(async));
}

static void async_task_add(ASYNC_TASK *t)
{
	LOCK();
	if(async.wait)
	{
		l_thrd_join(async.th,NULL);
		async.wait=false;
	}
	l_queue_push_tail(async.que,t);
	if(!async.run)
	{
		async.run=true;
		l_thrd_create(&async.th,async_task,NULL);
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
		l_thrd_sleep_ms(50);
	}while(timeout>0);
	return -1;
}

#if !defined(CFG_XIM_ANDROID) && !defined(CFG_XIM_NODEJS)
static void stream_result(void (*cb)(const char *,void*),void *user,const char *str,int size)
{
	int len=l_utf8_strlen(str,size);
	while(len>0)
	{
		char temp[512];
		char gb[256];
		const char *end;
		if(len<64)
		{
			end=(const void*)l_utf8_offset((const void*)str,len);
			len=0;
		}
		else
		{
			end=(const void*)l_utf8_offset((const void*)str,63);
			len-=63;
		}
		int part=(int)(size_t)(end-str);
		l_strncpy(temp,str,part);
		str=end;
		l_utf8_to_gb(temp,gb,sizeof(gb));
#ifndef _WIN32
		cb(gb,user);
#else
		void **p=l_cnew(3,void*);
		p[0]=cb;
		p[1]=l_strdup(gb);
		p[2]=user;
		PostMessage(y_ui_main_win(),WM_USER+118,0x7377652e,(LPARAM)p);
#endif
	}
}
#endif

#if defined(__linux__) && !defined(CFG_XIM_ANDROID) && !defined(CFG_XIM_NODEJS)
typedef struct {
	int fd;
	GIOChannel *ch;
	GPid pid;
	LString *str;
	void (*cb)(const char *,void*);
	void *user;
	bool skip_prefix;
	bool stream;
}SPAWN_ARG;

static gboolean cb_stdout(GIOChannel* channel, GIOCondition condition, SPAWN_ARG *arg)
{
	char temp[32];
	gsize bytes;

	GIOStatus status=g_io_channel_read_chars(channel,temp,sizeof(temp),&bytes,NULL);
	if(status == G_IO_STATUS_NORMAL && bytes>0)
	{
		LString *str=arg->str;
		l_string_append(str,temp,bytes);
		if(arg->stream && (arg->skip_prefix || str->len>=10))
		{
			if(!arg->skip_prefix)
			{
				if(l_str_has_prefix(str->str,"yong:text "))
					l_string_erase(str,0,10);
				arg->skip_prefix=true;
				if(str->len==0)
					return TRUE;
			}
			char *end;
			l_utf8_validate(str->str,str->len,(void**)&end);
			if(end!=str->str && end[-1]=='\n')
				end--;
			if(end==str->str)
				return TRUE;
			int size=(int)(size_t)(end-str->str);
			stream_result(arg->cb,arg->user,str->str,size);
			l_string_erase(str,0,size);
		}
		return TRUE;
	}

	if(arg->str->len>8192)
	{
		arg->cb(NULL,arg->user);
	}
	else
	{
		char temp[16*1024];
		l_utf8_to_gb(arg->str->str,temp,sizeof(temp));
		if(l_str_has_prefix(temp,"yong:text "))
			memmove(temp,temp+10,strlen(temp+10)+1);
		l_str_trim_right(temp);
		arg->cb(temp,arg->user);
	}
	g_io_channel_unref(arg->ch);
	close(arg->fd);
	l_string_free(arg->str);
	l_free(arg);

	return FALSE;
}

static void child_handler(GPid pid, int status, SPAWN_ARG *arg)
{
	g_spawn_close_pid(pid);
}

int y_im_async_spawn(char **argv,void (*cb)(const char *text,void *user),void *user,bool stream)
{
	gboolean ret;
	SPAWN_ARG *arg=l_new0(SPAWN_ARG);
	GError *error=NULL;
	ret=g_spawn_async_with_pipes(NULL,
			argv,
			NULL,
			G_SPAWN_DO_NOT_REAP_CHILD|G_SPAWN_SEARCH_PATH,
			NULL,
			arg,
			&arg->pid,
			NULL,
			&arg->fd,
			NULL,
			&error);
	if(ret!=TRUE)
	{
		printf("%s\n",error->message);
		g_error_free(error);
		l_free(arg);
		return -1;
	}
	arg->cb=cb;
	arg->user=user;
	arg->str=l_string_new(1024);
	arg->ch=g_io_channel_unix_new(arg->fd);
	arg->stream=stream;
	g_io_channel_set_encoding(arg->ch,NULL,NULL);
	g_io_add_watch(arg->ch,G_IO_IN | G_IO_ERR | G_IO_HUP,(GIOFunc)cb_stdout , (gpointer)arg);
	g_child_watch_add(arg->pid , (GChildWatchFunc)child_handler, arg);
	return 0;
}
#endif

#ifdef _WIN32
#include "ui.h"

typedef struct {
	LString *str;
	void (*cb)(const char *text,void *user);
	void *user;
	bool skip_prefix;
	bool stream;
}SPAWN_ARG;

static void spawn_arg_cb(SPAWN_ARG *arg,char *text)
{
	void **p=l_cnew(3,void*);
	if(text!=NULL)
	{
		if(l_str_has_prefix(text,"yong:text "))
			memmove(text,text+10,strlen(text+10)+1);
		l_str_trim_right(text);
	}
	p[0]=arg->cb;
	p[1]=text;
	p[2]=arg->user;
	PostMessage(y_ui_main_win(),WM_USER+118,0x7377652e,(LPARAM)p);
}

static DWORD WINAPI spawn_thread(SPAWN_ARG *arg)
{
	STARTUPINFO si={0};
	PROCESS_INFORMATION pi={0};
	SECURITY_ATTRIBUTES sa={.nLength=sizeof(sa),.bInheritHandle=TRUE};
	WCHAR temp[16*1024];
	int is_utf8=l_str_has_prefix(arg->str->str,"node ");
	l_utf8_to_utf16(arg->str->str,temp,sizeof(temp));
	arg->str->str[0]=0;
	arg->str->len=0;
	si.cb=sizeof(si);
	si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
	si.wShowWindow=SW_HIDE;
	HANDLE readPipe=NULL,writePipe=NULL;
	if(!CreatePipe(&readPipe,&writePipe,&sa,20*1024))
	{
		l_string_free(arg->str);
		spawn_arg_cb(arg,NULL);
		l_free(arg);
		return 0;
	}
	si.hStdOutput=writePipe;
	si.hStdError=writePipe;
	BOOL ret=CreateProcess(NULL,temp,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi);
	CloseHandle(writePipe);
	if(ret!=TRUE)
	{
		// printf("CreateProcess: %lu\n",GetLastError());
		CloseHandle(readPipe);
		l_string_free(arg->str);
		spawn_arg_cb(arg,NULL);
		l_free(arg);
		return 0;
	}
	while(1)
	{
		char temp[1024];
		DWORD bytes;
		ret=ReadFile(readPipe,temp,sizeof(temp),&bytes,NULL);
		if(ret!=TRUE)
		{
			//printf("Read: %lu %d\n",GetLastError(),arg->str->len);
			break;
		}
		LString *str=arg->str;
		l_string_append(str,temp,bytes);
		if(is_utf8 && arg->stream && (arg->skip_prefix || str->len>=10))
		{
			if(!arg->skip_prefix)
			{
				if(l_str_has_prefix(str->str,"yong:text "))
					l_string_erase(str,0,10);
				arg->skip_prefix=true;
				if(str->len==0)
					return TRUE;
			}
			char *end;
			l_utf8_validate(str->str,str->len,(void**)&end);
			if(end!=str->str && end[-1]=='\n')
				end--;
			if(end==str->str)
				return TRUE;
			int size=(int)(size_t)(end-str->str);
			stream_result(arg->cb,arg->user,str->str,size);
			l_string_erase(str,0,size);
		}
	}
	CloseHandle(readPipe);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	if(arg->str->len>8192)
	{
		spawn_arg_cb(arg,NULL);
	}
	else
	{
		UINT acp=GetACP();
		if(is_utf8)
		{
			l_utf8_to_gb(arg->str->str,(void*)temp,sizeof(temp));
			arg->str->len=0;
			l_string_append(arg->str,(const char*)temp,-1);
		}
		else if(acp!=936 && acp!=54936)
		{
			MultiByteToWideChar(CP_ACP,0,arg->str->str,-1,temp,L_ARRAY_SIZE(temp));
			l_utf16_to_gb(temp,arg->str->str,arg->str->size);
		}
		spawn_arg_cb(arg,arg->str->str);
		arg->str->str=NULL;
	}
	l_string_free(arg->str);
	l_free(arg);
	return 0;
}

// windows下针对\，空格，双引号的转义规则未知
static LString *cmdline_from_argv(char **argv)
{
	LString *s=l_string_new(8192);
	char *p,c;
	for(int i=0;(p=argv[i])!=NULL;i++)
	{
		if(i!=0)
			l_string_append_c(s,' ');
		int space=strchr(p,' ')?1:0;
		if(space)
			l_string_append_c(s,'"');
		for(int j=0;(c=p[j])!=0;j++)
		{
			if(i==0 && c=='/')
				c='\\';
			if(c=='"')
				l_string_append(s,"\\\"",2);
			else if(i!=0 && space && c=='\\' && !l_str_has_prefix(argv[0],"cmd"))
				l_string_append(s,"\\\\",2);
			else
				l_string_append_c(s,c);
		}
		if(space)
			l_string_append_c(s,'"');
		if(s->len>8192)
		{
			l_string_free(s);
			return NULL;
		}
	}
	return s;
}

int y_im_async_spawn(char **argv,void (*cb)(const char *text,void *user),void *user,bool stream)
{
	SPAWN_ARG *arg=l_new0(SPAWN_ARG);
	arg->str=cmdline_from_argv(argv);
	if(!arg->str)
	{
		l_free(arg);
		return -1;
	}
	arg->user=user;
	arg->cb=cb;
	arg->stream=stream;
	HANDLE thread=CreateThread(NULL,0,(void*)spawn_thread,arg,0,NULL);
	if(thread==NULL)
	{
		l_string_free(arg->str);
		l_free(arg);
		return -2;
	}
	return 0;
}
#endif

