#ifndef EMSCRIPTEN

#ifdef __linux__

#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>

#define WAKEUP_SIGNAL		SIGUSR2

typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define closesocket(s) close(s)
#define WSAEALREADY EALREADY
#define WSAEINPROGRESS EINPROGRESS
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAGetLastError() errno

static void setnonblock(SOCKET s)
{
	int f_old;
	f_old = fcntl(s,F_GETFL,0);  
	if( f_old < 0 ) return;
	f_old |= O_NONBLOCK;
	fcntl(s,F_SETFL,f_old);
}

#endif

#ifdef _WIN32
#include <winsock2.h>

static void setnonblock(SOCKET s)
{
	u_long b=1;
	ioctlsocket(s,FIONBIO,&b);
}

#endif

#include "llib.h"

static l_mtx_t l_mut;
static l_cnd_t l_cnd;
static void sg_lock(bool b)
{
	if(b)
		l_mtx_lock(&l_mut);
	else
		l_mtx_unlock(&l_mut);
}

static void WaitSignal(int ms)
{
	l_mtx_lock(&l_mut);
	l_cnd_timedwait_ms(&l_cnd,&l_mut,ms);
	l_mtx_unlock(&l_mut);
}

static void SetSignal(void)
{
	l_cnd_signal(&l_cnd);
}

#include "cloud.h"
#include "yong.h"

extern EXTRA_IM EIM;

static int url_get_port(char *url)
{
	int port=80;
	char *p;
	if(!strncmp(url,"http://",7))
		url+=7;
	p=strchr(url,'@');
	if(p) url=p+1;
	p=strchr(url,':');
	if(p)
	{
		port=atoi(p+1);
	}
	return port;
}

static char *url_get_host(char *url)
{
	char *p;
	if(!strncmp(url,"http://",7))
		url+=7;
	p=strchr(url,'@');
	if(p) url=p+1;
	p=strdup(url);
	url=p;
	p=strchr(p,':');
	if(p) *p=0;
	return url;
}

static struct sockaddr_in sa;
static void sg_addr_init(const char *proxy,const char *host)
{
	struct hostent *he;
	if(sa.sin_addr.s_addr!=INADDR_NONE &&
			sa.sin_addr.s_addr!=INADDR_ANY)
	{
		return;
	}
	sa.sin_family=AF_INET;
	if(!proxy)
	{
		he=gethostbyname(host);
		if(!he)
		{
			return;
		}
		sa.sin_port=htons(80);
		memcpy(&sa.sin_addr.s_addr,he->h_addr,he->h_length);
	}
	else
	{
		char *host=url_get_host((char*)proxy);
		int port=url_get_port((char*)proxy);
		sa.sin_addr.s_addr=inet_addr(host);
		if(sa.sin_addr.s_addr==INADDR_NONE)
		{
			he=gethostbyname(host);
			if(!he)
			{
				free(host);
				return;
			}
		}
		free(host);
		sa.sin_port=htons(port);
	}
}

static SOCKET sg_conn_init(sg_cache_t *c,const char *host)
{
	SOCKET s;
	int ret;
	fd_set wfds,efds;
	struct timeval tv;
	int i;
	sg_addr_init(c->proxy,host);
	s=socket(AF_INET,SOCK_STREAM,0);
	if(s==INVALID_SOCKET)
	{
		return s;
	}
	setnonblock(s);
	ret=connect(s,(struct sockaddr*)&sa,sizeof(sa));
	if(ret!=0 && WSAGetLastError()!=WSAEINPROGRESS &&
			WSAGetLastError()!=WSAEALREADY && 
			WSAGetLastError()!=WSAEWOULDBLOCK)
	{
		closesocket(s);
		return INVALID_SOCKET;
	}
	for(i=0;i<50;i++)
	{
		FD_ZERO(&wfds);FD_SET(s,&wfds);
		FD_ZERO(&efds);FD_SET(s,&efds);
		tv.tv_sec=0,tv.tv_usec=100*1000;
		ret=select(s+1,0,&wfds,&efds,&tv);
		if(ret==0)
		{
			if(c->abort)
			{
				closesocket(s);
				return INVALID_SOCKET;
			}
			continue;
		}
		if(ret<0 || FD_ISSET(s,&efds))
		{
			closesocket(s);
			return INVALID_SOCKET;
		}
		if(ret==1 && FD_ISSET(s,&wfds))
			break;
	}
	if(i==50)
	{
		closesocket(s);
		return INVALID_SOCKET;
	}
	return s;
}

static int sg_conn_req(sg_cache_t *c,SOCKET s,const char *req)
{
	char data[2048];
	int ret,len;
	
	if(!c->format)
	{
		c->format=l_alloc(1024);
		len=sprintf(c->format,"%s ",sg_cur_api->method?sg_cur_api->method:"GET");
		if(c->proxy)
			len+=sprintf(c->format+len,"http://%s%s%s%s HTTP/1.1\r\n",
					sg_cur_api->host,
					sg_cur_api->query_res,
					c->option?"&":"",
					c->option?c->option:"");
		else
			len+=sprintf(c->format+len,"%s%s%s HTTP/1.1\r\n",
					sg_cur_api->query_res,
					c->option?"&":"",
					c->option?c->option:"");
		if(c->proxy_auth)
			len+=sprintf(c->format+len,"Proxy-Authenticate: Basic %s\r\n",c->proxy_auth);
		len+=sprintf(c->format+len,"UserAgent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:146.0) Gecko/20100101 Firefox/146.0\r\n");
		len+=sprintf(c->format+len,"Host: %s\r\n",sg_cur_api->host);
	}
	
	if(!c->key || !c->key[0])
	{
		len=sprintf(data,c->format,req);
	}
	else
	{
		len=sprintf(data,c->format,c->key,req);
	}
	if(c->cookie)
	{
		sg_cookie_t *t;
		len+=sprintf(data+len,"Cookie:");
		for(t=(sg_cookie_t*)c->cookie;t!=NULL;t=t->n)
		{
			len+=sprintf(data+len," %s=%s;",t->name,t->value);
		}
		len+=sprintf(data+len,"\r\n");
	}
	if(sg_cur_api->post_res)
	{
		int clen=sprintf(data+len,sg_cur_api->post_res,req);
		len+=sprintf(data+len,"Content-Length: %d\r\n",clen);
	}
	len+=sprintf(data+len,"\r\n");
	if(sg_cur_api->post_res)
		len+=sprintf(data+len,sg_cur_api->post_res,req);
	ret=send(s,data,len,0);
	if(ret!=len)
		return -1;
	return 0;
}

static int sg_conn_size(char *res)
{
	char *p;
	int size;
	int hdr;
	p=strstr(res,"\r\n\r\n");
	if(!p) return 0;
	hdr=(int)(size_t)(p-res)+4;
	p=strstr(res,"Content-Length: ");
	if(!p) return -1;
	size=atoi(p+16);
	return size+hdr;
}

static void sg_conn_set_cookie(sg_cache_t *c,const char *s,int len)
{
	int pos=0;
	
	while(pos<len)
	{
		char temp[1024];
		int i;
		char **list,*p;
		for(i=0;i<sizeof(temp)-1 && pos<len && s[pos]!='\r';pos++,i++)
		{
			temp[i]=s[pos];
		}
		temp[i]=0;pos+=2;
		if(strncmp(temp,"Set-Cookie: ",12))
			continue;
		list=l_strsplit(temp+12,';');
		if(!list) break;
		for(i=0;(p=list[i])!=NULL;i++)
		{
			int namelen;
			while(*p==' ') p++;
			namelen=strcspn(p,"=");
			if(namelen<=0) break;
			if(p[namelen]==0) continue;
			p[namelen]=0;
			if(!strcmp(p,"expires") || !strcmp(p,"path") || !strcmp(p,"domain"))
				continue;
			//printf("%s=%s\n",p,p+namelen+1);
			if(p[namelen+1]=='\0')
			{
				sg_cookie_t *t;
				for(t=(sg_cookie_t*)c->cookie;t!=NULL;t=t->n)
				{
					if(!strcmp(t->name,p)) break;
				}
				if(t)
				{
					c->cookie=l_slist_remove(c->cookie,t);
					sg_cookie_free(t);
				}
			}
			else
			{
				sg_cookie_t *t;
				for(t=(sg_cookie_t*)c->cookie;t!=NULL;t=t->n)
				{
					if(!strcmp(t->name,p)) break;
				}
				if(t)
				{
					l_free(t->value);
					t->value=l_strdup(p+namelen+1);
				}
				else
				{
					t=l_new(sg_cookie_t);
					t->name=l_strdup(p);
					t->value=l_strdup(p+namelen+1);
					c->cookie=l_slist_append(c->cookie,t);
				}
			}
		}
		l_strfreev(list);
	}
}

static char *sg_conn_res(sg_cache_t *c,SOCKET s)
{
	fd_set fds;
	int ret;
	struct timeval tv;
	char temp[4096];
	char *p=0;
	int i;
	int retry;
	int size=0;
	for(retry=0;retry<16;retry++)
	{
		int max;
		if(!size)
			max=c->proxy?80:50;
		else
			max=5;
		for(i=0;i<max;i++)
		{
			FD_ZERO(&fds);
			FD_SET(s,&fds);
			tv.tv_sec=0;tv.tv_usec=100*1000;
			ret=select(s+1,&fds,0,0,&tv);
			if(c->abort)
				return NULL;
			if(ret<0)
			{
				i--;
				continue;
			}
			if(ret==1) break;
		}
		if(ret==max) return NULL;
		ret=recv(s,temp+size,4096-size-1,0);
		if(ret==0 && size>0)
			break;
		if(ret<=0)
			return 0;
		size+=ret;
		temp[size]=0;
		if(strncmp("HTTP/1.",temp,7) || !isdigit(temp[7]) ||
				strncmp(temp+8," 200 OK\r\n",9))
		{
			//printf("%s\n",temp+9);
			return 0;
		}
		p=strstr(temp,"\r\n\r\n");
		if(p)
		{
			//printf("%d %d\n",sg_conn_size(temp),size);
			if(size>4000 || sg_conn_size(temp)<=size)
				break;
		}
		if(size>7 && !memcmp(temp+size-7,"\r\n0\r\n\r\n",7))
			break;
	}
	p=strstr(temp,"\r\n\r\n");
	if(!p) return NULL;
	sg_conn_set_cookie(c,temp,(int)(size_t)(p-temp));
	p=strdup(p+4);
	return p;
}

static void sg_conn_key(sg_cache_t *c)
{
	SOCKET s=sg_conn_init(c,sg_cur_api->host);
	int ret;
	char req[1024];
	char *res;
	
	if(!sg_cur_api->query_key)
		return;
	if(s==INVALID_SOCKET) return;
	if(c->proxy)
	{
		int len;
		len=snprintf(req,sizeof(req),"GET http://%s%s HTTP/1.1\r\n"
									"Host: %s\r\n",
									sg_cur_api->host,
									sg_cur_api->query_key,
									sg_cur_api->host);
		if(c->proxy_auth)
			len+=sprintf(req+len,"Proxy-Authenticate: Basic %s\r\n",c->proxy_auth);
		/*len+=*/sprintf(req+len,"%s","\r\n");
	}
	else
	{
		snprintf(req,sizeof(req),"GET %s HTTP/1.1\r\n"
									"Host: %s\r\n"
									"\r\n",
									sg_cur_api->query_key,
									sg_cur_api->host);
	}
			
	ret=send(s,req,strlen(req),0);
	if(ret<=0)
	{
		closesocket(s);
		return;
	}
	res=sg_conn_res(c,s);
	closesocket(s);
	if(!res) return;
	if(c->key) free(c->key);
	c->key=sg_cur_api->key_parse(c,res);
	free(res);
}

static int sg_thread(sg_cache_t *c)
{
	SOCKET s=INVALID_SOCKET;
	c->ready=1;
	while(!c->abort)
	{
		sg_res_t *r;
		int refresh=0;
		if(!c->key && sg_cur_api->query_key)
		{
			sg_conn_key(c);
		}
		if(!c->req[0]) WaitSignal(2000);
		if(c->abort) break;
		if(!c->req[0]) continue;
		sg_lock(1);
		r=sg_cache_get(c,c->req);
		if(r)
		{
			c->req[0]=0;
			refresh=1;
			//printf("cache %s\n",r->q);
		}
		else
		{
			r=sg_local(0,c->req,true);
			if(r)
			{
				r=sg_local(r,c->req,false);
				sg_recc(c,2);
				sg_cache_add(c,r);
				c->req[0]=0;
				refresh=1;
			}
		}
		sg_lock(0);
		if(!refresh && !c->key && sg_cur_api->query_key)
		{
			if(!c->abort) WaitSignal(2000);
			continue;
		}
		if(!r)
		{
			char req[128];
			int ret;
			if(s==INVALID_SOCKET)
			{
				s=sg_conn_init(c,sg_cur_api->host);
				if(s==INVALID_SOCKET)
				{
					// printf("connect fail %s\n",sg_cur_api->host);
					WaitSignal(1000);
					continue;
				}
			}
			sg_lock(1);
			strcpy(req,c->req);
			sg_lock(0);
			if(!req[0])
			{
				closesocket(s);
				s=INVALID_SOCKET;
				continue;
			}
			ret=sg_conn_req(c,s,req);
			if(ret!=0)
			{
				closesocket(s);
				s=INVALID_SOCKET;
				continue;
			}
			char *res=sg_conn_res(c,s);
			if(res)
			{
				r=sg_parse(c,res);
				free(res);
				sg_lock(1);
				/* 这里我们同时检查两次，返回的可能不是我们请求的 */
				if(!strcmp(c->req,req) && (!r || !strcmp(r->q,req)))
				{
					c->req[0]=0;
					refresh=1;
				}
				sg_lock(0);
			}
			else
			{
				closesocket(s);
				s=INVALID_SOCKET;
			}
		}
		if(r && refresh)
		{
			sg_lock(1);
			c->l_old=c->l;
			c->l=r;
			sg_lock(0);
			if(EIM.Request)
			{
				EIM.Request(1);
			}
		}
	}
	if(s!=INVALID_SOCKET)
		closesocket(s);
	return 0;
}

static l_thrd_t l_th;

void CloudInit(void)
{
	l_mtx_init(&l_mut,l_mtx_plain);
	l_cnd_init(&l_cnd);
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(0x0202,&wsaData);
#endif

	l_thrd_create(&l_th,(l_thrd_start_t)sg_thread,l_cache);
#ifdef __linux__
	pthread_setname_np(l_th,"cloud");
#endif
}

void CloudCleanup(void)
{
	l_cache->abort=1;
	SetSignal();
	l_thrd_join(l_th,NULL);
	l_th=0;

	l_mtx_destroy(&l_mut);
	l_cnd_destroy(&l_cnd);
#ifdef _WIN32
	WSACleanup();
#endif
}

void CloudWaitReady(void)
{
	while(!l_cache->ready)
		l_thrd_sleep_ms(10);
}

void CloudSetSignal(void)
{
	SetSignal();
}

void CloudLock(void)
{
	sg_lock(1);
}

void CloudUnlock(void)
{
	sg_lock(0);
}

#else

L_EXPORT(void webim_on_ajax_data(char *res,const char *req))
{
	sg_cache_t *c=l_cache;
	if(!strcmp(req,"key"))
	{
		if(c->key) free(c->key);
		c->key=sg_cur_api->key_parse(c,res);
		//printf("key %s\n",c->key);
		CloudSetSignal();
	}
	else if(!strncmp(req,"req ",4))
	{
		sg_res_t *r;
		req+=4;
		r=sg_parse(c,res);
		if(r && !strcmp(c->req,req) && !strcmp(r->q,req))
		{
			c->req[0]=0;
			if(EIM.Request)
			{
				EIM.Request(1);
			}
		}
	}
}

extern void webim_ajax(const char *url,const char *arg);

static int ensure_conn_key(void)
{
	char url[256];
	if(l_cache->key || !sg_cur_api->query_key)
		return 1;
	snprintf(url,sizeof(url),"http://%s%s",
			sg_cur_api->host,sg_cur_api->query_key);
	webim_ajax(url,"key");
	return 0;
}

static int get_cand_list(void)
{
	sg_cache_t *c=l_cache;
	char format[256];
	char url[384];
	char arg[128];
	snprintf(format,sizeof(format),"http://%s%s",
		sg_cur_api->host,sg_cur_api->query_res);
	if(sg_cur_api->query_key)
		snprintf(url,sizeof(url),format,c->key,c->req);
	else
		snprintf(url,sizeof(url),format,c->req);
	sprintf(arg,"req %s",c->req);
	webim_ajax(url,arg);
	return 0;
}

void CloudInit(void)
{
	ensure_conn_key();
}

void CloudCleanup(void)
{
}

void CloudWaitReady(void)
{
}

void CloudSetSignal(void)
{
	if(!ensure_conn_key())
		return;
	if(!l_cache->req[0])
		return;
	get_cand_list();
}

void CloudLock(void)
{
}

void CloudUnlock(void)
{
}

#endif
