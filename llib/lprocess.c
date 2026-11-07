#include "llib.h"

typedef struct{
	void *next;
	int id;
	bool killed;	// 进程还没起来就收到了 kill 请求
#ifdef _WIN32
	HANDLE pipe_in[2];
	HANDLE pipe_out[2];
	HANDLE proc;
#else
	int pipe_in[2];
	int pipe_out[2];
	pid_t pid;
#endif
	char **argv;
	int flags;
	LProcessReadFunc cb;
	void *user_data;
}WRAPPER_PARAM;

// 全局锁
static l_mtx_t mtx;
static int id_base=-1;
static WRAPPER_PARAM *plist;

static void plist_init(void)
{
	if(id_base>=0)
		return;
	l_mtx_init(&mtx,l_mtx_plain);
	id_base=0;
}

static int plist_next(void)
{
	int id=-1;
	uint8_t mask[8]={0};
	for(WRAPPER_PARAM *p=plist;p!=NULL;p=p->next)
	{
		l_bitmap_set(mask,p->id);
	}
	for(int i=0;i<64;i++)
	{
		int cur=id_base;
		id_base=(id_base+1)%64;
		if(!l_bitmap_get(mask,cur))
		{
			id=cur;
			break;
		}
	}
	return id;
}

static int plist_add(WRAPPER_PARAM *param)
{
	int ret=-1;
	l_mtx_lock(&mtx);
	int id=plist_next();
	if(id>=0)
	{
		plist=l_slist_prepend(plist,param);
		param->id=id;
		ret=0;
	}
	l_mtx_unlock(&mtx);
	return ret;
}

static void plist_remove(WRAPPER_PARAM *param)
{
	l_mtx_lock(&mtx);
	plist=l_slist_remove(plist,param);
	l_mtx_unlock(&mtx);
}

#ifdef __linux__

#include <sys/wait.h>
#include <signal.h>

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
		if(dup2(param->pipe_in[0], STDIN_FILENO) == -1)
			_exit(127);
		if(dup2(param->pipe_out[1], STDOUT_FILENO) == -1)
			_exit(127);
		close(param->pipe_in[0]);
		close(param->pipe_in[1]);
		close(param->pipe_out[0]);
		close(param->pipe_out[1]);
		execvp(param->argv[0],param->argv);
		_exit(127);
	}
	param->pid=pid;
	l_mtx_lock(&mtx);
	bool killed=param->killed;
	l_mtx_unlock(&mtx);
	if(killed)
		kill(pid,SIGTERM);
	close(param->pipe_in[0]);
	param->pipe_in[0]=-1;
	close(param->pipe_out[1]);
	param->pipe_out[1]=-1;
	while(1)
	{
		char temp[4096];
		int ret=read(param->pipe_out[0],temp,sizeof(temp));
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
	// 已回收，pid 可能被系统复用，先作废（回调之后才从 plist 摘除，中间不能留有可杀的目标）
	l_mtx_lock(&mtx);
	param->pid=0;
	l_mtx_unlock(&mtx);
OUT:
	if(l_sched.main)
		l_sched.main((LUserFunc)param->cb,buf);
	else
		param->cb(buf);
	plist_remove(param);
	l_strfreev(param->argv);
	if(param->pipe_in[0]!=-1)
		close(param->pipe_in[0]);
	if(param->pipe_in[1]!=-1)
		close(param->pipe_in[1]);
	if(param->pipe_out[0]!=-1)
		close(param->pipe_out[0]);
	if(param->pipe_out[1]!=-1)
		close(param->pipe_out[1]);
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
	si.hStdOutput=param->pipe_out[1];
	si.hStdError=GetStdHandle(STD_ERROR_HANDLE);;
	si.hStdInput=param->pipe_in[0];
	LPWSTR cmdline=cmdline_from_argv(param->argv);
	BOOL ret=CreateProcessW(NULL,cmdline,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi);
	l_free(cmdline);
	if(!ret)
	{
		buf->status=-1;
		goto OUT1;
	}
	param->proc=pi.hProcess;
	l_mtx_lock(&mtx);
	bool killed=param->killed;
	l_mtx_unlock(&mtx);
	if(killed)
		TerminateProcess(pi.hProcess,1);
	CloseHandle(param->pipe_in[0]);
	param->pipe_in[0]=INVALID_HANDLE_VALUE;
	CloseHandle(param->pipe_out[1]);
	param->pipe_out[1]=INVALID_HANDLE_VALUE;
	while(1)
	{
		char temp[4096];
		DWORD bytes;
		BOOL ret = ReadFile(param->pipe_out[0], temp, sizeof(temp), &bytes, NULL);
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
	// 先摘掉句柄引用再关闭，避免 l_process_kill 拿到可能被复用的句柄值
	param->proc=NULL;
	CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
OUT1:
	if(l_sched.main)
		l_sched.main((LUserFunc)param->cb,buf);
	else
		param->cb(buf);
	plist_remove(param);
	l_strfreev(param->argv);
	if(param->pipe_in[0]!=INVALID_HANDLE_VALUE)
		CloseHandle(param->pipe_in[0]);
	if(param->pipe_in[1]!=INVALID_HANDLE_VALUE)
		CloseHandle(param->pipe_in[1]);
	if(param->pipe_out[0]!=INVALID_HANDLE_VALUE)
		CloseHandle(param->pipe_out[0]);
	if(param->pipe_out[1]!=INVALID_HANDLE_VALUE)
		CloseHandle(param->pipe_out[1]);
	l_free(param);
	return 0;
}
#endif

// 向进程的 pipe_in 写入数据，返回写入字节数，失败返回 -1
// 被调用时 param 必须在 plist 中（调用方持有 mtx），保证 param 不会被回收
static int process_write(WRAPPER_PARAM *param,const void *buf,int size)
{
	int total=0;
#ifdef _WIN32
	if(param->pipe_in[1]==INVALID_HANDLE_VALUE)
		return -1;
	while(total<size)
	{
		DWORD bytes=0;
		if(!WriteFile(param->pipe_in[1],(const char*)buf+total,size-total,&bytes,NULL))
			break;
		if(bytes==0)
			break;
		total+=(int)bytes;
	}
#else
	if(param->pipe_in[1]<0)
		return -1;
	// SIGPIPE 由宿主忽略（见 common/main.c），这里只管 EPIPE
	while(total<size)
	{
		int ret=write(param->pipe_in[1],(const char*)buf+total,size-total);
		if(ret<0 && errno==EINTR)
			continue;
		if(ret<=0)
			break;
		total+=ret;
	}
#endif
	return total>0?total:-1;
}

// 关闭 pipe_in 的写入端，让进程读到 EOF，成功返回 0，已关闭过返回 -1
// 被调用时 param 必须在 plist 中（调用方持有 mtx）
static int process_close_in(WRAPPER_PARAM *param)
{
#ifdef _WIN32
	if(param->pipe_in[1]==INVALID_HANDLE_VALUE)
		return -1;
	CloseHandle(param->pipe_in[1]);
	param->pipe_in[1]=INVALID_HANDLE_VALUE;
#else
	if(param->pipe_in[1]<0)
		return -1;
	close(param->pipe_in[1]);
	param->pipe_in[1]=-1;
#endif
	return 0;
}

// 杀掉进程，成功返回 0，失败返回 -1
// 被调用时 param 必须在 plist 中（调用方持有 mtx）
static int process_kill(WRAPPER_PARAM *param)
{
#ifdef _WIN32
	if(param->proc==NULL)
	{
		param->killed=true;		// 进程还没创建，交给 wrapper 线程启动后杀
		return 0;
	}
	return TerminateProcess(param->proc,1)?0:-1;
#else
	if(param->pid<=0)
	{
		param->killed=true;		// 进程还没起来，交给 wrapper 线程 fork 后杀
		return 0;
	}
	return kill(param->pid,SIGTERM)?-1:0;
#endif
}

#ifdef __EMSCRIPTEN__
int l_pread(char **argv,int flags,LProcessReadFunc cb,void *user_data)
{
	return -1;
}
int l_process_write(int fd,const void *buf,int size)
{
	return -1;
}
int l_process_kill(int id)
{
	return -1;
}
#else
int l_pread(char **argv,int flags,LProcessReadFunc cb,void *user_data)
{
	plist_init();
	WRAPPER_PARAM *param=l_new(WRAPPER_PARAM);
#ifdef _WIN32
	SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
	if(TRUE!=CreatePipe(param->pipe_in+0,param->pipe_in+1,&saAttr,0))
	{
		l_free(param);
		return -1;
	}
	if(TRUE!=CreatePipe(param->pipe_out+0,param->pipe_out+1,&saAttr,0))
	{
		CloseHandle(param->pipe_in[0]);
		CloseHandle(param->pipe_in[1]);
		l_free(param);
		return -1;
	}
	SetHandleInformation(param->pipe_in[1], HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(param->pipe_out[0], HANDLE_FLAG_INHERIT, 0);
#else
	if(0!=pipe(param->pipe_in))
	{
		l_free(param);
		return -1;
	}
	if(0!=pipe(param->pipe_out))
	{
		close(param->pipe_in[0]);
		close(param->pipe_in[1]);
		l_free(param);
		return -1;
	}
#endif
	param->argv=l_strdupv(argv);
	param->flags=flags;
	param->cb=cb;
	param->user_data=user_data;
	param->killed=false;
#ifdef _WIN32
	param->proc=NULL;
#else
	param->pid=0;
#endif
	if(0!=plist_add(param))
	{
#ifdef _WIN32
		CloseHandle(param->pipe_in[0]);
		CloseHandle(param->pipe_in[1]);
		CloseHandle(param->pipe_out[0]);
		CloseHandle(param->pipe_out[1]);
#else
		close(param->pipe_in[0]);
		close(param->pipe_in[1]);
		close(param->pipe_out[0]);
		close(param->pipe_out[1]);
#endif
		l_strfreev(param->argv);
		l_free(param);
		return -1;
	}
	int id=param->id;
	l_thrd_t thr;
	int ret=l_thrd_create(&thr,(l_thrd_start_t)wrapper_thread,param);
	if(ret!=0)
	{
		plist_remove(param);
#ifdef _WIN32
		CloseHandle(param->pipe_in[0]);
		CloseHandle(param->pipe_in[1]);
		CloseHandle(param->pipe_out[0]);
		CloseHandle(param->pipe_out[1]);
#else
		close(param->pipe_in[0]);
		close(param->pipe_in[1]);
		close(param->pipe_out[0]);
		close(param->pipe_out[1]);
#endif
		l_strfreev(param->argv);
		l_free(param);
		return ret;
	}
	l_thrd_detach(thr);
	return id;
}

int l_process_write(int fd,const void *buf,int size)
{
	int ret=-1;
	if(fd<0)
		return -1;
	l_mtx_lock(&mtx);
	for(WRAPPER_PARAM *p=plist;p!=NULL;p=p->next)
	{
		if(p->id!=fd)
			continue;
		if(buf==NULL)
		{
			// 写入 NULL：关闭写入端，让进程读到 EOF
			ret=process_close_in(p);
		}
		else if(size>0)
		{
			ret=process_write(p,buf,size);
		}
		break;
	}
	l_mtx_unlock(&mtx);
	return ret;
}

int l_process_kill(int id)
{
	int ret=-1;
	if(id<0)
		return -1;
	l_mtx_lock(&mtx);
	for(WRAPPER_PARAM *p=plist;p!=NULL;p=p->next)
	{
		if(p->id!=id)
			continue;
		ret=process_kill(p);
		break;
	}
	l_mtx_unlock(&mtx);
	return ret;
}
#endif

