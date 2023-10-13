#pragma once

struct _HttpSession;
typedef struct _HttpSession HttpSession;

enum{
	HTTP_AUTH_NONE=0,
	HTTP_AUTH_BASIC,
	HTTP_AUTH_DIGEST,
};

HttpSession *http_session_new(void);
void http_session_free(HttpSession *ss);
int http_session_clear(HttpSession *ss);
int http_session_abort(HttpSession *ss);
int http_session_set_abort(HttpSession *ss,int *abort);
int http_session_is_abort(HttpSession *ss);
int http_session_set_host(HttpSession *ss,const char *host,int port);
const char *http_session_get_host(HttpSession *ss);
int http_session_set_header(HttpSession *ss,const char *header);
char *http_session_get(HttpSession *ss,const char *path,int *len,const char *post,int post_len);
char *http_session_put(HttpSession *ss,const char *url,int *len,const char *data,int data_len);
char *http_session_post_form(HttpSession *ss,const char *path,int *len,const char *name,const char *val,...);
int http_session_download(HttpSession *ss,const char *remote,const char *local);
int http_session_set_auth(HttpSession *ss,const char *user,const char *pass);
int http_session_set_auth_type(HttpSession *ss,int type);
int http_session_get_auth(HttpSession *ss,char **user,char **pass);
int http_session_set_cookie(HttpSession *ss,const char *cookie);
const char *http_session_get_cookie(HttpSession *ss);
int http_session_set_proxy(HttpSession *ss,const char *proxy);

int http_session_test(HttpSession *ss);
int http_session_sockc_connect(HttpSession *ss,const char *host,int port);
int http_session_sockc_send(HttpSession *ss,const void *buf,size_t size);
int http_session_sockc_sendn(HttpSession *ss,const void *buf,size_t size);
int http_session_sockc_recv(HttpSession *ss,void *buf,size_t size);
int http_session_sockc_recvn(HttpSession *ss,void *buf,size_t size);
void http_session_sockc_close(HttpSession *ss);

void http_session_base64_encode(char *out, const void *in, int inlen);
