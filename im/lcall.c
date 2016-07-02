#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
	
#include "lcall.h"
#include "ltricky.h"

//#define memcpy(a,b,c) memmove(a,b,c)

int l_call_buf_put_data(LCallBuf *buf,const void *data,size_t size,int align)
{
	uint16_t pos=buf->pos;
	if(align>1)
	{
		int next,i;
		next=(pos+align-1)&~(align-1);
		// pad0 to let valgrind happy
		for(i=pos;i<next;i++)
			buf->data[i]=0;
		pos=next;
	}
	if(pos+size>L_CALL_BUF_SIZE)
		return -1;
	memcpy(buf->data+pos,data,size);
	buf->pos=pos+size;
	return 0;
}

int l_call_buf_get_data(LCallBuf *buf,void *data,size_t size,int align)
{
	uint16_t pos=buf->pos;
	if(align>1)
		pos=(pos+align-1)&~(align-1);
	if(pos+size>buf->len)
		return -1;
	memcpy(data,buf->data+pos,size);
	buf->pos=pos+size;
	return 0;
}

int l_call_buf_put_string(LCallBuf *buf,const char *s)
{
	return l_call_buf_put_data(buf,s,strlen(s)+1,1);
}

int l_call_buf_get_string(LCallBuf *buf,char *data,size_t size)
{
	int base=buf->pos,i;
	for(i=0;i<size && i+base<buf->size;i++)
	{
		data[i]=(char)buf->data[i+base];
		if(!data[i]) break;
	}
	return data[i]==0?0:-1;
}

int l_call_buf_reset(LCallBuf *buf)
{
	//memset(buf->data,0,sizeof(buf->data));
	buf->pos=0;
	buf->size=0;
	return 0;
}

int l_call_buf_start(LCallBuf *buf,uint16_t seq,const char *name)
{
	int ret;
	l_call_buf_reset(buf);
	buf->magic=L_CALL_MAGIC;
	buf->seq=seq;
	buf->len=0;
	buf->flag=0;
	buf->pos=8;
	ret=l_call_buf_put_string(buf,name);
	return ret;
}

int l_call_buf_stop(LCallBuf *buf)
{
	buf->size=buf->pos;
	buf->pos=0;
	buf->len=buf->size;
	return 0;
}

int l_call_buf_ready(LCallBuf *buf)
{
	int i;
	if(buf->size<2)
		return 0;
	if(buf->magic!=L_CALL_MAGIC)
	{
		l_call_buf_reset(buf);
		return 0;
	}
	if(buf->size<4)
		return 0;
	if(buf->len>L_CALL_BUF_SIZE || buf->size<10)
	{
		l_call_buf_reset(buf);
		return 0;
	}
	if(buf->size<6)
		return 0;
	if(buf->len>buf->size)
		return 0;
	for(i=8;i<buf->len;i++)
	{
		 if(buf->data[i]==0 && i!=8)
		 {
		 	 buf->pos=i+1;
		 	 return 1;
		 }
	}
	l_call_buf_reset(buf);
	return 0;
}

int l_call_buf_next(LCallBuf *buf)
{
	int left;
	left=buf->size-buf->len;
	if(left<=0)
	{
		l_call_buf_reset(buf);
		return 0;
	}
	memmove(buf->data,buf->data+buf->len,left);
	buf->size=left;
	buf->pos=0;
	return 0;
}

const char *l_call_buf_name(LCallBuf *buf)
{
	return (const char*)buf->data+8;
}

int l_call_buf_write(LCallBuf *buf,const void *data,size_t size)
{
	if(buf->pos+size>L_CALL_BUF_SIZE)
		return -1;
	memcpy(buf->data+buf->size,data,size);
	buf->size+=size;
	return 0;
}


#if defined(L_CALL_GLIB_SERVER) || defined(L_CALL_GLIB_CLIENT)

#include <glib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>

void l_call_conn_free(LCallConn *conn)
{
	if(!conn)
		return;
	conn->user->free(conn);
	if(conn->serv)
		conn->serv->conn=g_slist_remove(conn->serv->conn,conn);
	if(conn->id)
		g_source_remove(conn->id);
	g_io_channel_shutdown(conn->ch,TRUE,NULL);
	g_io_channel_unref(conn->ch);
	g_free(conn);
}

static void l_call_build_path(char *path)
{
	char *p;
	p=getenv("DISPLAY");
	if(!p)
	{
		strcpy(path,"/tmp/yong-:0");
	}
	else
	{
		sprintf(path,"/tmp/yong-%s",p);
		p=strchr(path,'.');
		if(p) *p=0;
	}
}

static int conn_read_data(LCallConn *conn)
{
	gchar *buf=(gchar*)conn->buf.data+conn->buf.size;
	gsize count=L_CALL_BUF_SIZE-conn->buf.pos;
	ssize_t bytes_read;
	int fd=g_io_channel_unix_get_fd(conn->ch);
	bytes_read=recv(fd,buf,count,0);
	if(bytes_read<=0)
	{
		return -1;
	}
	conn->buf.size+=bytes_read;
	return 0;
}

static int conn_deal_data(LCallConn *conn)
{
	LCallBuf *buf=&conn->buf;
	int (*dispatch)(LCallConn *,const char *,LCallBuf *);
	dispatch=conn->user->dispatch;
	while(l_call_buf_ready(buf))
	{
		const char *name=l_call_buf_name(buf);
		int ret;
		ret=dispatch(conn,name,buf);
		if(ret!=0)
			return -1;
		l_call_buf_next(buf);
	}
	if(conn->buf.size>=L_CALL_BUF_SIZE-1)
	{
		return -1;
	}
	return 0;
}

static LCallBuf *conn_wait_result(LCallConn *conn,uint16_t seq)
{
	LCallBuf *buf=&conn->buf;
	const char *name;
	int ret;
	int (*dispatch)(LCallConn *,const char *,LCallBuf *);
	
	conn->rseq=seq;
	conn->res=TRUE;
	dispatch=conn->user->dispatch;

	while(conn->id!=0)
	{
		if(conn_read_data(conn)!=0)
		{
			break;
		}
		if(conn->id==0)
			break;
		while(l_call_buf_ready(buf))
		{
			name=l_call_buf_name(buf);
			if(!strcmp(name,"return") && buf->seq==seq)
			{
				conn->res=FALSE;
				return buf;
			}
			ret=dispatch(conn,name,buf);
			if(ret!=0)
			{
				conn->res=FALSE;
				return NULL;
			}
			l_call_buf_next(buf);
		}
	}
	conn->res=FALSE;
	return NULL;
}

int l_call_conn_vcall(LCallConn *conn,const char *name,int *res,const char *param,va_list ap)
{
	LCallBuf buf;
	int i;
	//gsize bytes_written;
	//GIOStatus status;
	uint16_t seq;
		
	seq=++conn->seq;
	l_call_buf_start(&buf,seq,name);
	if(res) buf.flag|=L_CALL_FLAG_SYNC;
	for(i=0;param[i]!=0;i++)
	{
		switch(param[i]){
		case 'i':
		{
			int t=va_arg(ap,int);
			l_call_buf_put_val(&buf,int,t);
			break;
		}
		case 's':
		{
			char *s=va_arg(ap,char*);
			l_call_buf_put_string(&buf,s);
			break;
		}
		default:
			break;
		}
	}
	l_call_buf_stop(&buf);
	int fd=g_io_channel_unix_get_fd(conn->ch);
	if(send(fd,buf.data,buf.size,MSG_NOSIGNAL)!=buf.size)
	{
		return -1;
	}
	/*status=g_io_channel_write_chars(conn->ch,(gchar*)buf.data,buf.size,&bytes_written,NULL);
	if(status!=G_IO_STATUS_NORMAL)
	{
		return -1;
	}*/
	if(res!=NULL)
	{
		LCallBuf *p;
		p=conn_wait_result(conn,seq);
		if(!p) return -1;
		i=l_call_buf_get_ptr(p,res);
		if(i!=0)
		{
			return -1;
		}
		l_call_buf_next(p);
		return conn_deal_data(conn);
	}
	return 0;
}

int l_call_conn_call(LCallConn *conn,const char *name,int *res,const char *param,...)
{
	int ret;
	va_list ap;
	va_start(ap,param);
	ret=l_call_conn_vcall(conn,name,res,param,ap);
	va_end(ap);
	return ret;
}

int l_call_conn_return(LCallConn *conn,uint16_t seq,int res)
{
	LCallBuf buf;
	//GIOStatus status;
	//gsize bytes_written;
	l_call_buf_start(&buf,seq,"return");
	l_call_buf_put_val(&buf,int,res);
	l_call_buf_stop(&buf);
	/*status=g_io_channel_write_chars(conn->ch,(gchar*)buf.data,buf.size,&bytes_written,NULL);
	if(status!=G_IO_STATUS_NORMAL)
	{
		return -1;
	}*/
	int fd=g_io_channel_unix_get_fd(conn->ch);
	if(send(fd,buf.data,buf.size,MSG_NOSIGNAL)!=buf.size)
	{
		return -1;
	}
	return 0;
}

/*
static gboolean conn_event(GIOChannel *channel,GIOCondition condition,LCallConn *conn);
static gboolean conn_event_idle(LCallConn *conn)
{
	if(0!=conn_deal_data(conn))
	{
		l_call_conn_free(conn);
	}
	conn->id=g_io_add_watch(conn->ch,G_IO_ERR|G_IO_IN|G_IO_HUP,
			(GIOFunc)conn_event,conn);
	return FALSE;
}
*/

static gboolean conn_event(GIOChannel *channel,GIOCondition condition,LCallConn *conn)
{
	if(condition!=G_IO_IN)
	{
		conn->id=0;
		if(!conn->res)
		{
			l_call_conn_free(conn);
		}
		return FALSE;
	}
	if(0!=conn_read_data(conn))
	{
		conn->id=0;
		if(!conn->res)
			l_call_conn_free(conn);
		return FALSE;
	}
	/*g_idle_add((GSourceFunc)conn_event_idle,conn);*/
	if(!conn->res && 0!=conn_deal_data(conn))
	{
		conn->id=0;
		l_call_conn_free(conn);
		return FALSE;
	}
	return TRUE;
	/*
	conn->id=0;
	return FALSE;
	*/
}

LCallConn *l_call_conn_new(GIOChannel *channel,LCallUser *user)
{
	LCallConn *conn;
	conn=g_new0(LCallConn,1);
	conn->user=user;
	conn->ch=channel;
	l_call_buf_reset(&conn->buf);
	conn->id=g_io_add_watch(channel,G_IO_ERR|G_IO_IN|G_IO_HUP,
			(GIOFunc)conn_event,conn);
	user->init(conn);
	return conn;
}

#endif

#ifdef L_CALL_GLIB_SERVER
#include "lfile.h"
GIOChannel *l_call_server_new(void)
{
	struct sockaddr_un sa;
	int s;
	GIOChannel *serv;

	s=socket(AF_UNIX,SOCK_STREAM,0);
	memset(&sa,0,sizeof(sa));
	sa.sun_family=AF_UNIX;
	l_call_build_path(sa.sun_path);
	if(l_file_exists(sa.sun_path))
	{
		unlink(sa.sun_path);
	}
	
	if(0!=bind(s,(struct sockaddr*)&sa,sizeof(sa)))
	{
		perror("bind");
		close(s);
		return NULL;
	}
	if(0!=listen(s,1))
	{
		perror("listen");
		close(s);
		return NULL;
	}
	serv=g_io_channel_unix_new(s);
	g_io_channel_set_encoding(serv,NULL,NULL);
	g_io_channel_set_buffered(serv,FALSE);
	return serv;
}

static gboolean serv_accept(GIOChannel *channel,GIOCondition condition,LCallServ *serv)
{
	int s,as;
	GIOChannel *client;
	LCallConn *conn;
	struct timeval timeo;
	
	timeo.tv_sec=1;
	timeo.tv_usec=0;
	
	if(condition!=G_IO_IN)
		return TRUE;
	
	s=g_io_channel_unix_get_fd(channel);
	as=accept(s,NULL,NULL);
	if(as==-1)
		return TRUE;
	setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeo,sizeof(timeo));
	setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&timeo,sizeof(timeo));
	client=g_io_channel_unix_new(as);
	g_io_channel_set_encoding(client,NULL,NULL);
	g_io_channel_set_buffered(client,FALSE);
	conn=l_call_conn_new(client,serv->user);
	serv->conn=g_slist_prepend(serv->conn,conn);
	
	return TRUE;
}

LCallServ *l_call_serv_new(void *arg,LCallUser *user)
{
	LCallServ *serv;
	
	serv=g_new0(LCallServ,1);
	serv->arg=arg;
	serv->user=user;
	serv->ch=l_call_server_new();
	serv->id=g_io_add_watch(serv->ch,G_IO_IN,(GIOFunc)serv_accept,serv);
	
	return serv;
}

void l_call_serv_free(LCallServ *serv)
{
	GSList *p;
	if(!serv)
		return;
	g_source_remove(serv->id);
	g_io_channel_shutdown(serv->ch,TRUE,NULL);
	g_io_channel_unref(serv->ch);
	for(p=serv->conn;p!=NULL;p=p->next)
		l_call_conn_free(p->data);
	g_slist_free(serv->conn);
	g_free(serv);
}

#endif/*L_CALL_GLIB_SERVER*/


#ifdef L_CALL_GLIB_CLIENT

GIOChannel *l_call_client_new(void)
{
	struct sockaddr_un sa;
	int s;
	GIOChannel *client;
	struct timeval timeo;
	
	timeo.tv_sec=1;
	timeo.tv_usec=0;

	s=socket(AF_UNIX,SOCK_STREAM,0);
	memset(&sa,0,sizeof(sa));
	sa.sun_family=AF_UNIX;
	l_call_build_path(sa.sun_path);
	
	if(0!=connect(s,(struct sockaddr*)&sa,sizeof(sa)))
	{
		//printf("client conn fail\n");
		close(s);
		return NULL;
	}
	setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeo,sizeof(timeo));
	setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&timeo,sizeof(timeo));
	client=g_io_channel_unix_new(s);
	g_io_channel_set_encoding(client,NULL,NULL);
	g_io_channel_set_buffered(client,FALSE);
	return client;
}

static LCallConn *_conn;
static void (*_connect)(void);
static int (*_dispatch)(const char *name,LCallBuf *buf);
static void client_init(LCallConn *conn)
{
	_conn=conn;
	if(_connect) _connect();
};
static void client_free(LCallConn *conn){_conn=NULL;};
static int client_dispatch(LCallConn *conn,const char *name,LCallBuf *buf)
{
	return _dispatch(name,buf);
}

static LCallUser _user={
	.init=client_init,
	.free=client_free,
	.dispatch=client_dispatch
};

void l_call_client_dispatch(int (*dispatch)(const char *,LCallBuf *))
{
	_dispatch=dispatch;
}

void l_call_client_set_connect(void (*cb)(void))
{
	_connect=cb;
}

void l_call_client_connect(void)
{
	GIOChannel *ch;
	if(_conn)
		return;
	ch=l_call_client_new();
	if(!ch) return;
	l_call_conn_new(ch,&_user);
}

void l_call_client_disconnect(void)
{
	if(!_conn) return;
	l_call_conn_free(_conn);
}

int l_call_client_call(const char *name,int *res,const char *param,...)
{
	va_list ap;
	int ret;
	
	if(!_conn && (!strcmp(name,"enable") || !strcmp(name,"focus_in")))
		l_call_client_connect();
	if(!_conn) return -1;
	
	va_start(ap,param);
	ret=l_call_conn_vcall(_conn,name,res,param,ap);
	va_end(ap);
	if(ret!=0)
		l_call_client_disconnect();

	return ret;
}

#endif/*L_CALL_GLIB_CLIENT*/

#ifdef L_CALL_GLIB_CLIENT_TEST

static int disp_func(const char *name,LCallBuf *buf)
{
	printf("callback %s\n",name);
	return 0;
}
int main(int arc,char *arg[])
{
	int res;
	l_call_client_dispatch(disp_func);
	l_call_client_connect();
	if(!l_call_client_call("test",&res,"is",1234,"hello world!"))
		printf("res %d\n",res);
	l_call_client_disconnect();
	return 0;
}

#endif/*L_CALL_GLIB_CLIENT_TEST*/

#ifdef L_CALL_GLIB_SERVER_TEST

static GMainLoop *loop;

static void serv_init(LCallConn *conn)
{
	printf("add %p\n",conn);
}
static void serv_free(LCallConn *conn)
{
	printf("del %p\n",conn);
}
static int serv_dispatch(LCallConn *conn,const char *name,LCallBuf *buf)
{
	printf("call %s\n",name);
	if(!strcmp(name,"test"))
	{
		int i;
		char s[64];
		l_call_buf_get_val(buf,i);
		l_call_buf_get_string(buf,s,sizeof(s));
		printf("\t%d %s\n",i,s);
		//l_call_conn_return(conn,buf->seq,4321);
	}
	else if(!strcmp(name,"cursor"))
	{
	}
	else if(!strcmp(name,"input"))
	{
		l_call_conn_return(conn,buf->seq,0);
	}
	else if(!strcmp(name,"focus_in"))
	{
	}
	else if(!strcmp(name,"focus_out"))
	{
	}
	else if(!strcmp(name,"enable"))
	{
	}
	else if(!strcmp(name,"add_ic"))
	{
	}
	else if(!strcmp(name,"del_ic"))
	{
	}
	return 0;
}
static LCallUser serv_user={
	serv_init,serv_free,serv_dispatch
};

int main(int arc,char *arg[])
{
	LCallServ *serv;
	loop=g_main_loop_new(NULL,0);
	serv=l_call_serv_new(0,&serv_user);
	g_main_loop_run(loop);
	g_main_loop_unref(loop);
	l_call_serv_free(serv);
	g_main_context_unref(g_main_context_default());
	return 0;
}

#endif/*L_CALL_GLIB_SERVER_TEST*/
