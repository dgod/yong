#include "llib.h"

typedef struct{
#ifdef _WIN32
	HANDLE pipefd[2];
#else
	int pipefd[2];
#endif
	char **argv;
	int flags;
	LProcessReadFunc cb;
	void *user_data;
}WRAPPER_PARAM;

#ifdef __linux__

#include <sys/wait.h>

static int wrapper_thread(WRAPPER_PARAM *param)
{
	LProcessBuffer *buf=l_new0_with(LProcessBuffer,param->user_data);
	pid_t pid=fork();
	if(pid<0)
	{
		buf->status=-1;
		goto OUT;
	}
	if(pid==0)
	{
		close(param->pipefd[0]);
		if(dup2(param->pipefd[1], STDOUT_FILENO) == -1)
			_exit(127);
		close(param->pipefd[1]);
		execvp(param->argv[0],param->argv);
		_exit(127);
	}
	close(param->pipefd[1]);
	param->pipefd[1]=-1;
	while(1)
	{
		char temp[4096];
		int ret=read(param->pipefd[0],temp,sizeof(temp));
		if(ret<=0)
			break;
		l_buffer_append((LBuffer*)buf,temp,ret);
		if((param->flags&L_PREAD_STREAM))
		{
			if(l_sched.main)
				l_sched.main((LUserFunc)param->cb,buf);
			else
				param->cb(buf);
			buf=l_new0_with(LProcessBuffer,param->user_data);
		}
	}
	waitpid(pid,&buf->status,0);
OUT:
	if(l_sched.main)
		l_sched.main((LUserFunc)param->cb,buf);
	else
		param->cb(buf);
	l_strfreev(param->argv);
	close(param->pipefd[0]);
	if(param->pipefd[1]!=-1)
		close(param->pipefd[1]);
	l_free(param);
	return 0;
}
#endif

#ifdef _WIN32

static LPWSTR cmdline_from_argv(char **argv)
{
	LBuffer buf=L_BUFFER_INIT;
	const char *p;
	uint32_t c;
	uint16_t temp[8];
	int len;
	for(int i=0;(p=argv[i])!=NULL;i++)
	{
		len=0;
		if(i!=0)
			len=l_unichar_to_utf16(' ',temp);
		bool space=strchr(p,' ')?true:false;
		if(space)
			len+=l_unichar_to_utf16('"',temp+len/2);
		if(len>0)
			l_buffer_append(&buf,temp,len);
		while(p!=NULL)
		{
			c=l_utf8_to_unichar((const uint8_t*)p);
			if(c==0)
				break;
			p=(const char*)l_utf8_next_char((const uint8_t*)p);
			if(i==0 && c=='/')
				c='\\';
			if(c=='"')
			{
				len=l_unichar_to_utf16('\\',temp);
				len+=l_unichar_to_utf16(c,temp+len/2);
			}
			else if(i!=0 && space && c=='\\' && !l_str_has_prefix(argv[0],"cmd"))
			{
				len=l_unichar_to_utf16('\\',temp);
				len+=l_unichar_to_utf16('\\',temp+len/2);
			}
			else
			{
				len=l_unichar_to_utf16(c,temp);
			}
			l_buffer_append(&buf,temp,len);
		}
		if(space)
		{
			len=l_unichar_to_utf16('"',temp);
			l_buffer_append(&buf,temp,len);
		}
	}
	temp[0]=0;
	l_buffer_append(&buf,temp,2);
	return (LPWSTR)buf.data;
}

static int wrapper_thread(WRAPPER_PARAM *param)
{
	LProcessBuffer *buf=l_new0_with(LProcessBuffer,param->user_data);
	STARTUPINFO si={0};
	PROCESS_INFORMATION pi={0};
	SECURITY_ATTRIBUTES sa={.nLength=sizeof(sa),.bInheritHandle=TRUE};
	si.cb=sizeof(si);
	if((param->flags&L_PREAD_SHOW))
	{
		si.dwFlags=STARTF_USESTDHANDLES;
	}
	else
	{
		si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
		si.wShowWindow=SW_HIDE;
	}
	si.hStdOutput=param->pipefd[1];
	si.hStdError=GetStdHandle(STD_ERROR_HANDLE);;
	si.hStdInput=INVALID_HANDLE_VALUE;
	LPWSTR cmdline=cmdline_from_argv(param->argv);
	BOOL ret=CreateProcessW(NULL,cmdline,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi);
	l_free(cmdline);
	if(!ret)
	{
		buf->status=-1;
		goto OUT1;
	}
	CloseHandle(param->pipefd[1]);
	param->pipefd[1]=INVALID_HANDLE_VALUE;
	while(1)
	{
		char temp[4096];
		DWORD bytes;
		BOOL ret = ReadFile(param->pipefd[0], temp, sizeof(temp), &bytes, NULL);
		if(!ret || bytes==0)
			break;
		l_buffer_append((LBuffer*)buf,temp,(int)bytes);
		if((param->flags&L_PREAD_STREAM))
		{
			if(l_sched.main)
				l_sched.main((LUserFunc)param->cb,buf);
			else
				param->cb(buf);
			buf=l_new0_with(LProcessBuffer,param->user_data);
		}
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exitCode;
	GetExitCodeProcess(pi.hProcess,&exitCode);
	buf->status=(int)exitCode;
	CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
OUT1:
	if(l_sched.main)
		l_sched.main((LUserFunc)param->cb,buf);
	else
		param->cb(buf);
	l_strfreev(param->argv);
	CloseHandle(param->pipefd[0]);
	if(param->pipefd[1]!=INVALID_HANDLE_VALUE)
		CloseHandle(param->pipefd[1]);
	l_free(param);
	return 0;
}
#endif

int l_pread(char **argv,int flags,LProcessReadFunc cb,void *user_data)
{
	WRAPPER_PARAM *param=l_new(WRAPPER_PARAM);
#ifdef _WIN32
	SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
	if(TRUE!=CreatePipe(param->pipefd+0,param->pipefd+1,&saAttr,0))
	{
		l_free(param);
		return -1;
	}
	SetHandleInformation(param->pipefd[0], HANDLE_FLAG_INHERIT, 0);
#else
	if(0!=pipe(param->pipefd))
	{
		l_free(param);
		return -1;
	}
#endif
	param->argv=l_strdupv(argv);
	param->flags=flags;
	param->cb=cb;
	param->user_data=user_data;
	l_thrd_t thr;
	int ret=l_thrd_create(&thr,(l_thrd_start_t)wrapper_thread,param);
	if(ret!=0)
	{
#ifdef _WIN32
		CloseHandle(param->pipefd[0]);
		CloseHandle(param->pipefd[1]);
#else
		close(param->pipefd[0]);
		close(param->pipefd[1]);
#endif
		l_strfreev(param->argv);
		l_free(param);
		return ret;
	}
	l_thrd_detach(thr);
	return 0;
}

