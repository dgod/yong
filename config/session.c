#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <dirent.h>
#include <stdarg.h>
#include "ltricky.h"
	
#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
typedef int SOCKET;
#define INVALID_SOCKET		(-1)
#define closesocket(s)		close(s)
#endif
	
enum{
	SESS_STATE_CLOSE=0,
	SESS_STATE_CONNECTING,
	SESS_STATE_CONNECTED,
	SESS_STATE_AUTH,
	SESS_STATE_REQUEST,
	SESS_STATE_WAIT,
	SESS_STATE_RESPONSE,
};

struct _HttpSession{
	char agent[64];
	int state;
	int socket;
	int port;
	char host[68];
	int keep_alive;
	volatile int abort;
	volatile int *p_abort;
	int timeout;
	char *header;
	char *cookie;

	int auth_type;
	char *user;
	char *pass;
	char *proxy;
};

typedef struct{
	char protocol[8];
	char auth[32];
	char host[68];
	char ip[64];
	int port;
	char path[256];
}URL;

#include "session.h"

#ifndef MAX
#define MAX(a,b) ((a)<(b)?(b):(a))
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

void *gz_extract(const void *input,int len,int *olen);

#ifdef _WIN32
static int sock_nonblock(SOCKET sock,int on)
{
	ioctlsocket(sock,FIONBIO,(u_long*)&on);
	return 0;
}
#else

#if L_WORD_SIZE==32 && defined(__i686__)
__asm__(".symver fcntl, fcntl@GLIBC_2.0");
#endif

static int sock_nonblock(SOCKET sock,int on)
{
	int flag;

	flag=fcntl (sock, F_GETFL);
	if(on)
		flag|=O_NONBLOCK;
	else
		flag&=~O_NONBLOCK;
	return fcntl (sock, F_SETFL,  flag );
}
#endif

int http_session_sockc_connect(HttpSession *ss,const char *host,int port)
{
	struct sockaddr_in sa;
	SOCKET s;
	int ret;
	int i;

	memset(&sa,0,sizeof(sa));
	sa.sin_family=AF_INET;
	sa.sin_addr.s_addr=inet_addr(host);
	if(sa.sin_addr.s_addr==INADDR_NONE)
		return -1;
	sa.sin_port=htons(port);
	s=socket(AF_INET,SOCK_STREAM,0);
	if(s==INVALID_SOCKET)
		return -1;
	sock_nonblock(s,1);
	ret=connect(s,(struct sockaddr*)&sa,sizeof(sa));
	//printf("%d %d\n",ret,WSAGetLastError());
	for(i=0;i<ss->timeout && !http_session_is_abort(ss);i++)
	{
		fd_set fdw,fde;
		struct timeval tv;
		FD_ZERO(&fdw);
		FD_SET(s,&fdw);
		FD_ZERO(&fde);
		FD_SET(s,&fde);
		tv.tv_sec=0;
		tv.tv_usec=100*1000;
		ret=select(s+1,NULL,&fdw,&fde,&tv);
		if(ret<0) break;
		if(ret==0) continue;
		if(FD_ISSET(s,&fdw))
		{
			ss->socket=s;
			return (int)s;
		}
		break;
	}
	closesocket(s);
	return -1;
}

int http_session_sockc_send(HttpSession *ss,const void *buf,size_t size)
{
	int i;
	fd_set fdw;
	struct timeval tv;
	
	for(i=0;i<ss->timeout && !http_session_is_abort(ss);i++)
	{
		int ret;
		FD_ZERO(&fdw);
		FD_SET(ss->socket,&fdw);
		tv.tv_sec=0;
		tv.tv_usec=100*1000;
		ret=select(ss->socket+1,NULL,&fdw,NULL,&tv);
		if(ret==1)
		{
			ret=send(ss->socket,buf,size,0);
			return ret;
		}
	}
	return 0;
}

int http_session_sockc_sendn(HttpSession *ss,const void *buf,size_t size)
{
	int i;
	fd_set fdw;
	struct timeval tv;
	const char *p=buf;
	
	for(i=0;i<ss->timeout && !http_session_is_abort(ss) && size>0;i++)
	{
		int ret;
		FD_ZERO(&fdw);
		FD_SET(ss->socket,&fdw);
		tv.tv_sec=0;
		tv.tv_usec=100*1000;
		ret=select(ss->socket+1,NULL,&fdw,NULL,&tv);
		if(ret==1)
		{
			ret=send(ss->socket,p,size,0);
			if(ret<=0)
				return ret;
			p+=ret;
			size-=ret;
			i=0;
		}
	}
	return 0;
}

int http_session_sockc_recv(HttpSession *ss,void *buf,size_t size)
{
	int i;
	fd_set fdr;
	struct timeval tv;
	for(i=0;i<50 && !http_session_is_abort(ss);i++)
	{
		int ret;
		FD_ZERO(&fdr);
		FD_SET(ss->socket,&fdr);
		tv.tv_sec=0;
		tv.tv_usec=100*1000;
		ret=select(ss->socket+1,&fdr,NULL,NULL,&tv);
		if(ret==1)
		{
			ret=recv(ss->socket,buf,size,0);
			return ret;
		}
	}
	return 0;
}

int http_session_sockc_recvn(HttpSession *ss,void *buf,size_t size)
{
	char *p=buf;
	while(size>0)
	{
		int ret=http_session_sockc_recv(ss,p,size);
		if(ret<=0) return -1;
		size-=ret;
		p+=ret;
	}
	return 0;
}

static int sockc_select(int s,int ms)
{
	int ret;
	fd_set fdr;
	struct timeval tv;
	FD_ZERO(&fdr);
	FD_SET(s,&fdr);
	tv.tv_sec=ms/1000;
	tv.tv_usec=(ms%1000)*1000;
	ret=select(s+1,&fdr,NULL,NULL,&tv);
	return ret;
}

void http_session_sockc_close(HttpSession *ss)
{
	if(ss->socket==-1)
		return;
	closesocket(ss->socket);
	ss->socket=-1;
}

typedef struct{
	char *data;
	int len;
	int size;
}HBUF;

static void hbuf_init(HBUF *buf,int size)
{
	memset(buf,0,sizeof(HBUF));
	if(size<4096) size=4096;
	buf->size=size;
	buf->data=malloc(size);
}

static void hbuf_append(HBUF *buf,const char *data,int size)
{
	if(size+buf->len+1>buf->size)
	{
		int m=(size+1)-(buf->size-buf->len);
		m=MAX(m,buf->size/2);
		buf->size+=m;
		buf->data=realloc(buf->data,buf->size);
	}
	memcpy(buf->data+buf->len,data,size);
	buf->len+=size;
	buf->data[buf->len]=0;
}

static char *strclip(const char *begin,const char *end)
{
	size_t len=end-begin;
	char *res=malloc(len+1);
	memcpy(res,begin,len);
	res[len]=0;
	return res;
}

static unsigned long resolv_host(const char *host,char *ip)
{
	struct hostent *he;
	union{
		unsigned long l;
		unsigned char b[4];
	}addr;
	addr.l=inet_addr(host);
	if(addr.l!=INADDR_ANY && addr.l!=INADDR_NONE)
	{
		sprintf(ip,"%d.%d.%d.%d",addr.b[0],addr.b[1],addr.b[2],addr.b[3]);
		return addr.l;
	}
	he=gethostbyname(host);
	if(!he)
		return INADDR_NONE;
	memcpy(&addr,he->h_addr,he->h_length);
	if(ip!=NULL)
		sprintf(ip,"%d.%d.%d.%d",addr.b[0],addr.b[1],addr.b[2],addr.b[3]);
	return addr.l;
}

static int url_parse2(const char *url,URL *r,int resolv)
{
	const char *p;
	size_t len;
	u_long addr;

	memset(r,0,sizeof(*r));

	p=strstr(url,"://");
	if(p==NULL)
		return -1;
	if((len=(size_t)(p-url))>7)
		return -2;
	memcpy(r->protocol,url,len);
	r->protocol[len]=0;
	url=p+3;
	p=strchr(url,'@');
	if(p!=NULL)
	{
		if((len=(size_t)(p-url))>=sizeof(r->auth))
			return -3;
		memcpy(r->auth,url,len);
		r->auth[len]=0;
		url=p+1;
	}
	p=strpbrk(url,":/");
	if(p==url)
		return -4;
	if(!p)
		p=strchr(url,'\0');
	if((len=(size_t)(p-url))>=sizeof(r->host))
		return -5;
	memcpy(r->host,url,len);
	r->host[len]=0;
	if(resolv)
	{
		addr=resolv_host(r->host,r->ip);
		if(addr==INADDR_NONE || addr==INADDR_ANY)
		{
			return -5;
		}
	}
	url=p;
	if(url[0]==':')
	{
		r->port=atoi(url+1);
		p=strchr(url,'/');
		if(!p)
			p=strchr(url,'\0');
		url=p;
	}
	else
	{
		if(!strcmp(r->protocol,"http"))
			r->port=80;
	}
	if(url[0]=='/')
		snprintf(r->path,sizeof(r->path)-1,"%s",url);
	else
		strcpy(r->path,"/");
	return 0;
}

static int url_parse(const char *url,char *host,char *ip,int *port,char *path)
{
	const char *p;
	char *temp;
	u_long addr;

	if(memcmp(url,"http://",7))
		return -1;
	url+=7;
	p=strpbrk(url,":/");
	if(!p || p==url)
		return -2;
	temp=strclip(url,p);
	if(host)
		strcpy(host,temp);
	addr=resolv_host(temp,ip);
	free(temp);
	if(addr==INADDR_NONE || addr==INADDR_ANY)
	{
		return -3;
	}
	url=p;
	if(url[0]==':')
	{
		p=strchr(url,'/');
		if(!p || p==url)
			return -1;
		temp=strclip(url+1,p);
		if(port)
			*port=atoi(temp);
		free(temp);
		url=p;
	}
	else
	{
		if(port) *port=80;
	}
	snprintf(path,256,"%s",url);
	return 0;
}

static int http_parse_header(HBUF *buf,char **h)
{
	char *p;
	int ret;
	int ver[2],code;
	int line_break_size=4;
	ret=sscanf(buf->data,"HTTP/%d.%d %d",ver+0,ver+1,&code);
	if(ret!=3) return -1;
	if(code!=200 && code!=302) return -1;
	/*if(memcmp(buf->data,"HTTP/1.1 200 OK\r\n",17) &&
			memcmp(buf->data,"HTTP/1.0 200 OK\r\n",17) &&
			memcmp(buf->data,"HTTP/1.1 302",12))
		return -1;*/
	p=strstr(buf->data,"\r\n\r\n");
	if(!p)
	{
		p=strstr(buf->data,"\n\n");
		line_break_size=2;
	}
	if(!p && buf->len>1400)
		return -1;
	if(!p)
		return 0;
	if(h)
	{
		int size=p+line_break_size-buf->data;
		if(*h!=NULL)
			free(*h);
		*h=malloc(size+1);
		memcpy(*h,buf->data,size);
		(*h)[size]=0;
	}
	return (int)(p+4-buf->data);
}

static int build_auth_string(char auth[],const char *user,const char *pass)
{
	char temp[64];
	int len,pos;
	if(!user || !pass) return 0;
	len=snprintf(temp,sizeof(temp),"%s:%s",user,pass);
	pos=sprintf(auth,"Authorization: Basic ");
	http_session_base64_encode(auth+pos,temp,len);
	strcat(auth,"\r\n");
	return strlen(auth);
}

static int check_chunked(HBUF *buf)
{
	char *p=buf->data;
	int len=buf->len;
	if(len<5)
		return 0;
	if(memcmp(p+len-5,"0\r\n\r\n",5)!=0)
		return 0;
	
	while(len>=5)
	{
		long part;
		char *end;
		int lnum;

		part=strtol(p,&end,16);
		if(part<0)
			return -1;
		lnum=(int)(size_t)(end-p);
		p=end;len-=lnum;
		if(len<2)
			return 0;
		if(p[0]!='\r' || p[1]!='\n')
			return -1;
		p+=2;len-=2;
		if(len<part)
			return 0;
		p+=part;len-=part;
		if(len<2)
			return 0;
		if(p[0]!='\r' || p[1]!='\n')
			return -1;
		p+=2;len-=2;
		if(part==0)
		{
			if(len==0)
				return 1;
			return -1;
		}
	}	
	return 0;
}

static int build_chunked(HBUF *buf)
{
	char *p=buf->data;
	int len=buf->len;
	int olen=0;
	if(len<5)
		return -1;
	if(memcmp(p+len-5,"0\r\n\r\n",5)!=0)
		return 0;
	
	while(len>=5)
	{
		long part;
		char *end;
		int lnum;
		part=strtol(p,&end,16);
		if(part<0)
			return -1;
		lnum=(int)(size_t)(end-p);
		p=end;len-=lnum;
		if(len<2)
			return 0;
		if(p[0]!='\r' || p[1]!='\n')
			return -1;
		p+=2;len-=2;
		if(len<part)
			return 0;
		memmove(buf->data+olen,p,part);
		olen+=part;
		p+=part;len-=part;
		if(len<2)
			return -1;
		if(p[0]!='\r' || p[1]!='\n')
			return -1;
		p+=2;len-=2;
		if(part==0)
		{
			if(len==0)
			{
				buf->data[olen]=0;
				buf->len=olen;
				return 0;
			}
			return -1;
		}
	}	
	return -1;
}

static char *http_session_get_internal(HttpSession *ss,const char *url,int *len,const char *method,const char *post,int post_len)
{
	char ip[64];
	int port;
	char path[256];
	int ret;
	int s;
	char data[1024];
	HBUF buf;
	char *header=NULL,*p;
	int clen=-1;
	int conn_close=0;
	int gzip=0;
	int chunked=0;
	int quit=0;
	int i;
	char*proxy_auth=NULL;

	if(ss->proxy)
	{
		URL r;
		if(0!=url_parse2(ss->proxy,&r,1))
		{
			free(ss->proxy);
			ss->proxy=NULL;
			return NULL;
		}
		strcpy(ip,r.ip);
		port=r.port;
		if(r.auth[0])
		{
			proxy_auth=alloca(sizeof(r.auth)*3/2+3);
			http_session_base64_encode(proxy_auth,r.auth,strlen(r.auth));
		}
		if(!strncmp(url,"http://",7))
		{
			if(0==url_parse2(url,&r,0))
			{
				strcpy(ss->host,r.host);
				ss->port=r.port;
			}
			strcpy(path,url);
		}
		else
		{
			sprintf(path,"http://%s:%d%s",ss->host,ss->port,url);
		}
	}
	else
	{
		if(!strncmp(url,"http://",7))
		{
			ret=url_parse(url,ss->host,ip,&port,path);
			if(ret!=0)
				return NULL;
			ss->port=port;
			http_session_clear(ss);
		}
		else if(ss->socket==-1)
		{
			if(INADDR_NONE==resolv_host(ss->host,ip))
			{
				return NULL;
			}
			port=ss->port;
			strcpy(path,url);
		}
		else
		{
			strcpy(path,url);
		}
	}
	if(ss->socket==-1)
	{
		ss->state=SESS_STATE_CONNECTING;
		ss->socket=s=http_session_sockc_connect(ss,ip,port);
		s=ss->socket;
	}
	else
	{
		s=ss->socket;
	}
	if(s==-1)
	{
		ss->state=SESS_STATE_CLOSE;
		return NULL;
	}
	ss->state=SESS_STATE_CONNECTED;
	char host[128];
	if(ss->port && ss->port!=80)
		sprintf(host,"%s:%d",ss->host,ss->port);
	else
		strcpy(host,ss->host);
	if(!post)
	{
		ret=sprintf(data,
			"GET %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Accept-Encoding: gzip\r\n"
			"Connection: keep-alive\r\n",
			path,host);
		if(ss->cookie)
			ret+=sprintf(data+ret,"Cookie: %s\r\n",ss->cookie);
		if(ss->header)
			ret+=sprintf(data+ret,"%s",ss->header);
		if(ss->auth_type==HTTP_AUTH_BASIC)
			ret+=build_auth_string(data+ret,ss->user,ss->pass);
		if(proxy_auth!=NULL)
			ret+=sprintf(data+ret,"Proxy-Authorization: Basic %s\r\n",proxy_auth);
		ret+=sprintf(data+ret,"\r\n");
	}
	else if(post_len<=0)
	{
		ret=sprintf(data,
			"%s %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: keep-alive\r\n"
			"Content-Length: %d\r\n",
			method,path,host,(int)strlen(post));
		if(ss->cookie)
			ret+=sprintf(data+ret,"Cookie: %s\r\n",ss->cookie);
		if(ss->header)
			ret+=sprintf(data+ret,"%s",ss->header);
		if(ss->auth_type==HTTP_AUTH_BASIC)
			ret+=build_auth_string(data+ret,ss->user,ss->pass);
		if(proxy_auth!=NULL)
			ret+=sprintf(data+ret,"Proxy-Authorization: Basic %s\r\n",proxy_auth);
		ret+=sprintf(data+ret,"\r\n");
		ret+=sprintf(data+ret,"%s",post);
	}
	else
	{
		ret=sprintf(data,
			"%s %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: keep-alive\r\n"
			"Content-Length: %d\r\n",
			method,path,host,post_len);
		if(ss->cookie)
			ret+=sprintf(data+ret,"Cookie: %s\r\n",ss->cookie);
		if(ss->header)
			ret+=sprintf(data+ret,"%s",ss->header);
		if(ss->auth_type==HTTP_AUTH_BASIC)
			ret+=build_auth_string(data+ret,ss->user,ss->pass);
		if(proxy_auth!=NULL)
			ret+=sprintf(data+ret,"Proxy-Authorization: Basic %s\r\n",proxy_auth);
		ret+=sprintf(data+ret,"\r\n");
	}
	assert(ret<=sizeof(data));
	ss->state=SESS_STATE_REQUEST;
	if(ret!=http_session_sockc_send(ss,data,ret))
	{
		//fprintf(stderr,"sockc_send fail\n");
		http_session_clear(ss);
		return NULL;
	}
	if(post && post_len>0)
	{
		if(0!=http_session_sockc_sendn(ss,post,post_len))
		{
			//fprintf(stderr,"sockc_sendn fail\n");
			http_session_clear(ss);
			return NULL;
		}
	}
	ss->state=SESS_STATE_WAIT;
	hbuf_init(&buf,4096);
	for(i=0;i<300;i++)						// keep connection 30s
	{
		ret=sockc_select(s,100);
		if(http_session_is_abort(ss) || ret==-1)
		{
			//fprintf(stderr,"sockc_select fail\n");
			http_session_clear(ss);
			free(buf.data);
			return NULL;
		}
		if(ret==1) break;
	}
	for(i=0;i<512 && buf.len<512*1024;i++)
	{
		ret=http_session_sockc_recv(ss,data,sizeof(data));
		if(ret<=0)
		{
			//fprintf(stderr,"recv data fail\n");
			free(buf.data);
			http_session_clear(ss);
			return NULL;
		}
		hbuf_append(&buf,data,ret);
		ret=http_parse_header(&buf,&header);
		if(ret==-1)
		{
			//fprintf(stderr,"parse header fail\n");
			free(buf.data);
			http_session_clear(ss);
			return NULL;
		}
		if(ret==0) continue;
		memmove(buf.data,buf.data+ret,buf.len+1-ret);
		buf.len-=ret;
		break;
	}
	
	if(i==512)
	{
		//fprintf(stderr,"sockc_recv i==512 fail\n");
		free(buf.data);
		http_session_clear(ss);
		return NULL;
	}
	p=strstr(header,"Content-Length:");
	if(!p) p=strstr(header,"Content-length:");
	if(!p) p=strstr(header,"content-length:");
	if(p!=NULL)
		clen=atoi(p+15);
	if(strstr(header,"Connection: close"))
		conn_close=1;
	if(strstr(header,"Content-Encoding: gzip") || strstr(header,"content-encoding: gzip"))
		gzip=1;
	if(strstr(header,"Transfer-Encoding: chunked") || strstr(header,"transfer-encoding: chunked"))
		chunked=1;

	if(strstr(header,"Set-Cookie: "))
	{
		char *temp=header;
		char cookie[1024];
		cookie[0]=0;
		do{
			char *end,*path;
			temp=strstr(temp,"Set-Cookie: ");
			if(!temp)
				break;
			temp+=12;
			end=strchr(temp,'\r');*end=0;
			path=strstr(temp," PATH=");
			if(!path) path=strstr(temp," path=");
			if(path) *path=0;
			strcat(cookie,temp);
			if(path) *path=' ';
			*end='\r';
		}while(1);
		http_session_set_cookie(ss,cookie);
		//printf("Cookie: %s\n",cookie);
	}
	free(header);
	ss->state=SESS_STATE_RESPONSE;
	for(;!quit && ((clen>0 && buf.len<clen) || (clen==-1));)
	{
		if(chunked)
		{
			ret=check_chunked(&buf);
			if(ret<0)
			{
				free(buf.data);
				http_session_clear(ss);
				return NULL;
			}
			if(ret>0)
			{
				ret=build_chunked(&buf);
				assert(ret==0);
				break;
			}
		}
		ret=http_session_sockc_recv(ss,data,sizeof(data));
		if(ret<0)
		{
			free(buf.data);
			http_session_clear(ss);
			return NULL;
		}
		if(ret==0)
		{
			conn_close=1;
			quit=1;
		}
		else
		{
			hbuf_append(&buf,data,ret);
		}
	}

	if(len) *len=buf.len;
	if(clen >0 && buf.len!=clen)
	{
		free(buf.data);
		http_session_clear(ss);
		return NULL;
	}
	if(conn_close!=0)
	{
		http_session_clear(ss);
	}
	else
	{
		ss->state=SESS_STATE_CONNECTED;
	}
	if(!gzip)
	{
		return buf.data;
	}
	else
	{
		void *res=gz_extract(buf.data,buf.len,len);
		free(buf.data);
		return res;
	}
}

char *http_session_get(HttpSession *ss,const char *url,int *len,const char *post,int post_len)
{
	const char *method=NULL;
	if(post) method="POST";
	return http_session_get_internal(ss,url,len,method,post,post_len);
}

char *http_session_put(HttpSession *ss,const char *url,int *len,const char *data,int data_len)
{
	return http_session_get_internal(ss,url,len,"PUT",data,data_len);
}

HttpSession *http_session_new(void)
{
	HttpSession *ss;
	ss=calloc(1,sizeof(*ss));
	strcpy(ss->agent,"HttpSession");
	ss->socket=-1;
	ss->port=80;
	ss->keep_alive=300;
	ss->timeout=30;
	ss->p_abort=&ss->abort;
	return ss;
}

int http_session_clear(HttpSession *ss)
{
	if(!ss) return -1;
	http_session_sockc_close(ss);
	if(ss->header)
	{
		free(ss->header);
		ss->header=NULL;
	}
	ss->state=SESS_STATE_CLOSE;
	return 0;
}

void http_session_free(HttpSession *ss)
{
	if(!ss)
		return;
	http_session_clear(ss);
	if(ss->user)
	{
		free(ss->user);
		ss->user=NULL;
	}
	if(ss->pass)
	{
		free(ss->pass);
		ss->pass=NULL;
	}
	if(ss->proxy)
	{
		free(ss->proxy);
		ss->proxy=NULL;
	}
	free(ss->cookie);
	free(ss);
}

int http_session_abort(HttpSession *ss)
{
	if(!ss)
		return -1;
	ss->abort=1;
	return 0;
}

int http_session_set_host(HttpSession *ss,const char *host,int port)
{
	if(!ss)
		return -1;
	if(port<=0 || port>=65536)
		return -1;
	snprintf(ss->host,sizeof(ss->host),"%s",host);
	ss->port=port;
	return 0;
}

const char *http_session_get_host(HttpSession *ss)
{
	return ss->host;
}

int http_session_get_port(HttpSession *ss)
{
	return ss->port;
}

int http_session_is_abort(HttpSession *ss)
{
	return *ss->p_abort;
}

int http_session_set_abort(HttpSession *ss,int *abort)
{
	if(abort)
		ss->p_abort=abort;
	else
		ss->p_abort=&ss->abort;
	return 0;
}

int http_session_set_header(HttpSession *ss,const char *header)
{
	if(!ss)
		return -1;
	if(ss->header)
	{
		free(ss->header);
		ss->header=NULL;
	}
	if(!header)
		return 0;
	ss->header=strdup(header);
	return 0;
}

int http_session_set_cookie(HttpSession *ss,const char *cookie)
{
	int len=0;
	if(!cookie || !cookie[0])
	{
		free(ss->cookie);
		ss->cookie=NULL;
		return 0;
	}
	if(ss->cookie)
		len=strlen(ss->cookie);
	ss->cookie=realloc(ss->cookie,len+1+strlen(cookie)+1);
	ss->cookie[len]=0;
	if(len>0) strcat(ss->cookie," ");
	strcat(ss->cookie,cookie);
	return 0;
}

const char *http_session_get_cookie(HttpSession *ss)
{
	return ss->cookie;
}

static FILE *open_file_p(const char *path)
{
	char *p;
	p=strrchr(path,'/');
	if(p!=NULL && strlen(path)<256)
	{
		char temp[256];
		strcpy(temp,path);
		p=strrchr(temp,'/');*p=0;
#ifdef _WIN32
		mkdir(temp);
#else
		mkdir(temp,0755);
#endif
	}
	return fopen(path,"wb");
}

int http_session_download(HttpSession *ss,const char *remote,const char *local)
{
	char ip[64];
	int port;
	char path[256];
	int ret;
	int s;
	char data[1024];
	HBUF buf;
	char *header=NULL,*p;
	int clen=-1;
	int conn_close=0;
	int i;
	FILE *fp;

	if(!strncmp(remote,"http://",7))
	{
		ret=url_parse(remote,ss->host,ip,&port,path);
		if(ret!=0)
			return -1;
		http_session_clear(ss);
	}
	else if(ss->socket==-1)
	{
		if(INADDR_NONE==resolv_host(ss->host,ip))
			return -1;
		port=ss->port;
		strcpy(path,remote);
	}
	else
	{
		strcpy(path,remote);
	}
	
	if(ss->socket==-1)
	{
		ss->state=SESS_STATE_CONNECTING;
		ss->socket=s=http_session_sockc_connect(ss,ip,port);
	}
	else
	{
		s=ss->socket;
	}
	if(s==-1)
	{
		ss->state=SESS_STATE_CLOSE;
		return -1;
	}
	ss->state=SESS_STATE_CONNECTED;

	{
		ret=snprintf(data,sizeof(data),
			"GET %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: Keep-Alive\r\n"
			"\r\n",
			path,ss->host);
		//fprintf(stderr,"%s",data);
	}

	ss->state=SESS_STATE_REQUEST;
	if(ret!=http_session_sockc_send(ss,data,ret))
	{
		//fprintf(stderr,"sockc_send fail\n");
		http_session_clear(ss);
		return -1;
	}
	ss->state=SESS_STATE_WAIT;
	hbuf_init(&buf,4096);
	for(i=0;i<300;i++)						// keep connection 30s
	{
		ret=sockc_select(s,100);
		if(ss->abort || ret==-1)
		{
			http_session_clear(ss);
			return -1;
		}
		if(ret==1) break;
	}
	for(i=0;i<100 && buf.len<256*1024;i++)
	{
		ret=http_session_sockc_recv(ss,data,sizeof(data));
		if(ret<=0)
		{
			//fprintf(stderr,"recv data fail\n");
			free(buf.data);
			http_session_clear(ss);
			return -1;
		}
		hbuf_append(&buf,data,ret);
		ret=http_parse_header(&buf,&header);
		if(ret==-1)
		{
			//fprintf(stderr,"parse header fail\n");
			free(buf.data);
			http_session_clear(ss);
			return -1;
		}
		if(ret==0) continue;
		memmove(buf.data,buf.data+ret,buf.len+1-ret);
		buf.len-=ret;
		break;
	}
	if(i==100)
	{
		free(buf.data);
		http_session_clear(ss);
		return -1;
	}
	p=strstr(header,"Content-Length:");
	if(p!=NULL)
		clen=atoi(p+15);
	if(strstr(header,"Connection: close"))
		conn_close=1;
	free(header);
	fp=open_file_p(local);
	if(!fp)
	{
		free(buf.data);
		http_session_clear(ss);
		return -1;
	}
	if(buf.len>0)
	{
		fwrite(buf.data,buf.len,1,fp);
		//buf.len=0;
	}
	free(buf.data);buf.data=NULL;
	ss->state=SESS_STATE_RESPONSE;
	for(;clen>0 && buf.len<clen;)
	{
		ret=http_session_sockc_recv(ss,data,sizeof(data));
		if(ret==0)
		{
			conn_close=1;
			break;
		}
		if(ret<0)
		{
			http_session_clear(ss);
			fclose(fp);
			remove(local);
			return -1;
		}
		fwrite(data,ret,1,fp);
		buf.len+=ret;
	}

	if(clen >0 && ftell(fp)!=clen)
	{
		http_session_clear(ss);
		fclose(fp);
		remove(local);
		return -1;
	}
	if(conn_close!=0)
	{
		http_session_clear(ss);
	}
	else
	{
		ss->state=SESS_STATE_CONNECTED;
	}
	fclose(fp);
	return 0;
}

static int url_encode(const char *in,char *out,int size)
{
	int i,c,pos=0;
	pos=0;
	for(i=0;(c=in[i])!=0;i++)
	{
		if((c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z') || strchr(".-*_",c))
		{
			out[pos++]=c;
		}
		else if(c==' ')
		{
			out[pos++]='+';
		}
		else
		{
			pos+=sprintf(out+pos,"%%%02X",(unsigned char)c);
		}
	}
	out[pos]=0;
	return pos;
}

char *http_session_post_form(HttpSession *ss,const char *path,int *len,const char *name,const char *val,...)
{
	char data[4096];
	int pos=0;
	va_list ap;
	va_start(ap,val);
	do{
		if(val==NULL) val="";
		pos+=sprintf(data+pos,"%s=",name);
		pos+=url_encode(val,data+pos,sizeof(data)-pos);
		name=va_arg(ap,const char*);
		if(name!=NULL)
		{
			data[pos++]='&';
			val=va_arg(ap,const char*);
		}
	}while(name!=NULL);
	va_end(ap);
	data[pos]=0;
	assert(pos<sizeof(data));
	http_session_set_header(ss,"Content-Type: application/x-www-form-urlencoded\r\n");
	return http_session_get(ss,path,len,data,pos);
}

int http_session_set_auth(HttpSession *ss,const char *user,const char *pass)
{
	if(ss->user)
	{
		free(ss->user);
		ss->user=NULL;
	}
	if(ss->pass)
	{
		free(ss->pass);
		ss->pass=NULL;
	}
	if(!user || !pass)
	{
		return 0;
	}
	ss->user=strdup(user);
	ss->pass=strdup(pass);
	return 0;
}

int http_session_set_auth_type(HttpSession *ss,int type)
{
	if(type!=HTTP_AUTH_NONE && type!=HTTP_AUTH_BASIC)
		return -1;
	ss->auth_type=type;
	return 0;
}

int http_session_get_auth(HttpSession *ss,char **user,char **pass)
{
	*user=ss->user;
	*pass=ss->pass;
	return 0;
}

int http_session_set_proxy(HttpSession *ss,const char *proxy)
{
	if(ss->proxy)
	{
		free(ss->proxy);
		ss->proxy=NULL;
	}
	if(!proxy)
		return 0;
	URL r;
	if(0!=url_parse2(proxy,&r,0))
		return -1;
	ss->proxy=strdup(proxy);
	return 0;
}

int http_session_set_timeout(HttpSession *ss,int seconds)
{
	if(seconds<=0)
		seconds=3;
	ss->timeout=seconds*10;
	return 0;
}

int http_session_test(HttpSession *ss)
{
	char ip[64];
	int ret=-1;
	if(INADDR_NONE==resolv_host(ss->host,ip))
		return -1;
	http_session_clear(ss);
	ss->socket=http_session_sockc_connect(ss,ip,ss->port);
	if(ss->socket!=INVALID_SOCKET)
		ret=0;
	http_session_clear(ss);
	return ret;
}

static char b64_list[] = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void http_session_base64_encode(char *out, const void *in_data, int inlen)
{
	const unsigned char *in=in_data;
	for (; inlen >= 3; inlen -= 3)
	{
		*out++ = b64_list[in[0] >> 2];
		*out++ = b64_list[((in[0] << 4) & 0x30) | (in[1] >> 4)];
		*out++ = b64_list[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
		*out++ = b64_list[in[2] & 0x3f];
		in += 3;
	}
	if (inlen > 0)
	{
		unsigned char fragment;
		*out++ = b64_list[in[0] >> 2];
		fragment = (in[0] << 4) & 0x30;
		if (inlen > 1)
		fragment |= in[1] >> 4;
		*out++ = b64_list[fragment];
		*out++ = (inlen < 2) ? '=' : b64_list[(in[1] << 2) & 0x3c];
		*out++ = '=';
	}
	*out = '\0';
}


