#include "llib.h"
#include "lcall.h"
#include "yong.h"

static bool dirty=false;

#define CTX_ID		1

static void client_enable(guint id)
{
	l_call_client_call("enable",NULL,"i",id);
}

static void client_add_ic(guint id)
{
	l_call_client_call("add_ic",NULL,"i",id);
}

static gboolean client_input_key(guint id,int key,guint32 time)
{
	int ret,res;
	ret=l_call_client_call("input",&res,"iii",id,key,time);
	if(ret!=0) return 0;
	return res?TRUE:FALSE;
}

static gboolean client_input_key_async(guint id,int key,guint32 time)
{
	int ret;
	ret=l_call_client_call("input",0,"iii",id,key,time);
	if(ret!=0) return 0;
	return TRUE;
}

static void client_focus_in(guint id)
{
	l_call_client_call("focus_in",NULL,"i",id);
}

static int disp_func(const char *name,LCallBuf *buf)
{
	if(!strcmp(name,"commit"))
	{
		guint id;
		int ret;
		char text[1024];
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		if(id!=CTX_ID) return -1;
		ret=l_call_buf_get_string(buf,text,sizeof(text));
		if(ret!=0) return -1;
		dirty=true;
		printf("%s",text);
	}
	return 0;
}

void client_input_keys(const char *s)
{
	bool last_space=false;
	int i=0;
	while(isspace(s[i])) i++;
	for(;s[i]!=0;i++)
	{
		int c=s[i];
		last_space=c==YK_SPACE;
		client_input_key_async(CTX_ID,c,0);
	}
	if(!last_space)
		client_input_key_async(CTX_ID,YK_SPACE,0);
	client_input_key(CTX_ID,YK_ESC,0);
}

int main(int argc,char *argv[])
{
	if(argc!=2)
	{
		return -1;
	}
	if(!l_str_is_ascii(argv[1]))
	{
		return -1;
	}
	if(strlen(argv[1])>=59)
	{
		return -1;
	}
	l_call_client_dispatch(disp_func);
	int res=l_call_client_connect();
	if(res!=0)
		return -1;
	client_add_ic(CTX_ID);
	client_enable(CTX_ID);
	client_focus_in(CTX_ID);
	client_input_keys(argv[1]);
	l_call_client_disconnect();
	if(dirty && isatty(fileno(stdout)))
		printf("\n");
	return 0;
}
