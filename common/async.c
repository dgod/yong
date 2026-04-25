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
	char *file;
	LString *data;
	bool backup;
}ASYNC_TASK_WRITE_FILE;

static void async_task_write_file_free(ASYNC_TASK_WRITE_FILE *t)
{
	l_free(t->file);
	l_string_free(t->data);
}

static void async_task_write_file_run(ASYNC_TASK_WRITE_FILE *t)
{
	const char *fn=t->file;

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
	if(fp!=NULL)
	{
		fwrite(t->data->str,t->data->len,1,fp);
		fclose(fp);
	}
	async_task_write_file_free(t);
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
	int ret=l_thrdp_run((LUserFunc)async_task_write_file_run,t,true);
	if(ret!=0)
	{
		async_task_write_file_free(t);
		return ret;
	}
	return 0;
}

int y_im_async_wait(int timeout)
{
	return l_thrdp_wait((LUserFunc)async_task_write_file_run,timeout);
}

#if !defined(CFG_XIM_ANDROID) && !defined(CFG_XIM_NODEJS)
typedef struct{
	bool stream;
	bool skip_prefix;
	bool is_utf8;
	void *user;
	LString *str;
	void (*cb)(const char *text,void *user);
}SPAWN_ARG;
static void stream_result(LProcessBuffer *result)
{
#ifdef _WIN32
	UINT acp=GetACP();
#endif
	SPAWN_ARG *arg=l_ptr_context(result);
	LString *str=arg->str;
	if(!result->data)
		goto END;
	if(!arg->stream)
	{
		if(l_str_has_prefix(result->str,"yong:text "))
		{
			l_string_erase((LString*)result,0,10);
		}
		l_str_trim_right(result->str);
		if(result->len==0)
			goto END;
		if(!arg->is_utf8)
		{
#ifdef _WIN32
			if(acp!=936 && acp!=54936)
			{
				WCHAR temp[4*1024];
				MultiByteToWideChar(CP_ACP,0,result->str,-1,temp,countof(temp));
				l_string_erase((LString*)result,0,-1);
				l_string_expand((LString*)result,4*1024);
				l_utf16_to_gb(temp,result->str,result->size);
			}
#endif
			arg->cb(result->str,arg->user);
		}
		else
		{
			char temp[16*1024];
			l_utf8_to_gb(arg->str->str,temp,sizeof(temp));
			arg->cb(temp,arg->user);
		}
		l_buffer_free((LBuffer*)result);
		goto END;
	}
	else
	{
		l_buffer_append(str,result->str,result->len);
		l_buffer_free((LBuffer*)result);
		if(!arg->skip_prefix && str->len<10)
			return;
		if(!arg->skip_prefix)
		{
			arg->skip_prefix=true;
			if(l_str_has_prefix(str->str,"yong:text "))
			{
				l_string_erase(str,0,10);
				if(str->len==0)
					return;
			}
		}
		char *end;
		if(arg->is_utf8)
		{
			l_utf8_validate(str->str,str->len,(void**)&end);
		}
		else
		{
#ifdef _WIN32
			if(acp==936 || acp==54936)
				l_gb_validate(str->str,str->len,(void**)&end);
			else
				end=str->str+str->len;
#else
			l_gb_validate(str->str,str->len,(void**)&end);
#endif
		}
		if(end==str->str)
			return;
		int size=(int)(size_t)(end-str->str);
		if(size==str->len)
		{
			if(str->str[size-1]=='\n')
			{
				size--;
				end--;
				if(size>0 && str->str[size-1]=='\r')
				{
					size--;
					end--;
				}
			}
		}
		if(arg->is_utf8)
		{
			char c=*end;
			*end=0;
			char temp[16*1024];
			l_utf8_to_gb(str->str,temp,sizeof(temp));
			arg->cb(temp,arg->user);
			*end=c;
		}
		else
		{
			char c=*end;
			*end=0;
#ifdef _WIN32
			if(acp==936 || acp==54936)
			{
				arg->cb(str->str,arg->user);
			}
			else
			{
				WCHAR temp[4*1024];
				char temp2[4*1024];
				MultiByteToWideChar(CP_ACP,0,str->str,-1,temp,countof(temp));
				l_utf16_to_gb(temp,temp2,countof(temp2));
				arg->cb(temp2,arg->user);
			}
#else
			arg->cb(str->str,arg->user);
#endif
			*end=c;
		}
		l_string_erase(str,0,size);		
	}
	return;
END:
	l_string_free(str);
	l_free(arg);
}
int y_im_async_spawn(char **argv,void (*cb)(const char *text,void *user),void *user,bool stream)
{
	int flags=stream?L_PREAD_STREAM:0;
	SPAWN_ARG *arg=l_new(SPAWN_ARG);
	arg->str=stream?l_string_new(1024):NULL;
	arg->stream=stream;
	arg->user=user;
	arg->cb=cb;
	arg->skip_prefix=false;
#ifdef _WIN32
	arg->is_utf8=strcmp(argv[0],"node")?false:true;
#else
	arg->is_utf8=true;
#endif
	int ret=l_pread(argv,flags,stream_result,arg);
	return ret;
}
#endif

