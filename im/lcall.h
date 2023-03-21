#ifndef _LCALL_H_
#define _LCALL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
	
#define L_CALL_BUF_SIZE		1024
#define L_CALL_MAGIC		0x4321

#define L_CALL_FLAG_SYNC	(1<<0)

typedef struct{
	uint16_t size;
	uint16_t pos;
	union{
		uint8_t data[L_CALL_BUF_SIZE];
		struct{
			uint16_t magic;
			uint16_t seq;
			uint16_t len;
			uint16_t flag;
		};
	};
}LCallBuf;

int l_call_buf_reset(LCallBuf *buf);
int l_call_buf_start(LCallBuf *buf,uint16_t seq,const char *name);
int l_call_buf_stop(LCallBuf *buf);
int l_call_buf_ready(LCallBuf *buf);
int l_call_buf_next(LCallBuf *buf);
const char *l_call_buf_name(LCallBuf *buf);
int l_call_buf_write(LCallBuf *buf,const void *data,size_t size);

int l_call_buf_put_data(LCallBuf *buf,const void *data,size_t size,int align);
int l_call_buf_get_data(LCallBuf *buf,void *data,size_t size,int align);
int l_call_buf_get_string(LCallBuf *buf,char *data,size_t size);
int l_call_buf_put_string(LCallBuf *buf,const char *s);

#define l_call_buf_put_val(buf,type,val) \
	do { \
		type _t=(type)val; \
		l_call_buf_put_data((buf),&(_t),sizeof(_t),sizeof(_t)); \
	}while(0)

#define l_call_buf_get_val(buf,val) \
	l_call_buf_get_data((buf),&(val),sizeof(val),sizeof(val))
	
#define l_call_buf_get_ptr(buf,ptr) \
	l_call_buf_get_data(buf,ptr,sizeof(*ptr),sizeof(*ptr))

#if defined(L_CALL_GLIB_SERVER) || defined(L_CALL_GLIB_CLIENT)
#include <glib.h>

struct _LCallConn;
typedef struct _LCallConn LCallConn;
struct _LCallServ;
typedef struct _LCallServ LCallServ;
struct _LCallUser;
typedef struct _LCallUser LCallUser;

struct _LCallConn{
	LCallServ *serv;
	LCallUser *user;
	GIOChannel *ch;
	LCallBuf buf;
	uint16_t seq;
	uint16_t rseq;
	gboolean res;
	guint id;
	void *arg;
};

struct _LCallServ{
	GIOChannel *ch;
	guint id;
	GSList *conn;
	void *arg;
	LCallUser *user;
};

struct _LCallUser{
	void (*init)(LCallConn *);
	void (*free)(LCallConn *);
	int (*dispatch)(LCallConn *,const char *,LCallBuf *);
};

LCallConn *l_call_conn_new(GIOChannel *channel,LCallUser *user);
int l_call_conn_return(LCallConn *conn,uint16_t seq,int res);
int l_call_conn_call(LCallConn *conn,const char *name,int *res,const char *param,...);
int l_call_conn_vcall(LCallConn *conn,const char *name,int *res,const char *param,va_list ap);
void l_call_conn_free(LCallConn *conn);
int l_call_conn_peer_pid(LCallConn *conn);

#endif

#ifdef L_CALL_GLIB_SERVER

GIOChannel *l_call_server_new(void);
LCallServ *l_call_serv_new(void *arg,LCallUser *user);
void l_call_serv_free(LCallServ *serv);

#endif

#ifdef L_CALL_GLIB_CLIENT
GIOChannel *l_call_client_new(void);
void l_call_client_connect(void);
void l_call_client_disconnect(void);
void l_call_client_dispatch(int (*_dispatch)(const char *,LCallBuf *));
void l_call_client_set_connect(void (*cb)(void));
int l_call_client_call(const char *name,int *res,const char *param,...);
#endif

#ifdef __cplusplus
}
#endif

#endif/*_LCALL_H_*/
