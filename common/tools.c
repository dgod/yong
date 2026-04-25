#ifdef _WIN32

#include <windows.h>
#include <stdbool.h>

#define WM_USER_TOOL		(WM_USER+116)

int y_run_tool(int t,int p,bool wait)
{
	HWND hWnd=FindWindow(L"yong_main",L"main");
	if(!hWnd)
		return -1;
	if(!wait)
	{
		BOOL ret=PostMessage(hWnd,WM_USER_TOOL,t,p);
		return ret?0:-1;
	}
	else
	{
		DWORD temp=SendMessage(hWnd,WM_USER_TOOL,t,p);
		return (int)temp;
	}
}

#else

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <sys/time.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct{
	uint16_t magic;
	uint16_t seq;
	uint16_t len;
	uint16_t flag;
	char method[8];
	uint32_t data[2];
}LCallMsg;

static void l_call_build_path(char *path)
{
	char *p=getenv("DISPLAY");
	if(!p)
		p=":0";
	sprintf(path,"/tmp/yong-%s",p);
	p=strchr(path,'.');
	if(p) *p=0;
}

int l_call_connect(void)
{
	struct sockaddr_un sa;
	int s;
	struct timeval timeo;
	
	timeo.tv_sec=0;
	timeo.tv_usec=500*1000;

	s=socket(AF_UNIX,SOCK_STREAM,0);
	memset(&sa,0,sizeof(sa));
	sa.sun_family=AF_UNIX;
	l_call_build_path(sa.sun_path);
	
	if(0!=connect(s,(struct sockaddr*)&sa,sizeof(sa)))
	{
		perror("connect");
		close(s);
		return -1;
	}
	setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeo,sizeof(timeo));
	setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&timeo,sizeof(timeo));

	struct linger ling;
	ling.l_onoff = 1;
	ling.l_linger = 1;
	setsockopt(s, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));

	return s;
}

static LCallMsg tool_buf;

int y_run_tool(int t,int p,bool wait)
{
	int s=l_call_connect();
	if(s==-1)
	{
		return -1;
	}
	tool_buf.magic=0x4321;
	tool_buf.seq=0;
	tool_buf.len=sizeof(LCallMsg);
	strcpy(tool_buf.method,"tool");
	tool_buf.data[0]=t;
	tool_buf.data[1]=p;
	tool_buf.flag=wait?1:0;
	int ret=write(s,&tool_buf,sizeof(tool_buf));
	if(ret<=0)
	{
		close(s);
		return -1;
	}
	if(!wait)
	{
		close(s);
		return 0;
	}
	ret=read(s,&tool_buf,sizeof(tool_buf));
	close(s);
	if(ret>=(int)sizeof(tool_buf)-4)
	{
		return tool_buf.data[0];
	}

	return -1;
}

#endif

