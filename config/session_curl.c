#include <curl/curl.h>

#include "llib.h"
#include "session.h"

struct _HttpSession{
	int port;
	char host[68];
	int timeout;
	volatile int abort;
	LString str;
	CURLM *curlm;
};

HttpSession *http_session_new(void)
{
	HttpSession *ss=l_new0(HttpSession);
	ss->timeout=30000;
	ss->curlm=curl_multi_init();
	return ss;
}

void http_session_free(HttpSession *ss)
{
	if(!ss)
		return;
	curl_multi_cleanup(ss->curlm);
	free(ss->str.str);
	l_free(ss);
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

static size_t write_callback(char *ptr, size_t size, size_t nmemb,  HttpSession *ss)
{
	// printf("write %d\n",nmemb);
	if(ss->abort)
		return CURL_WRITEFUNC_ERROR;
	l_string_append(&ss->str,ptr,nmemb);
	return nmemb;
}

char *http_session_get(HttpSession *ss,const char *path,int *len,const char *post,int post_len)
{
	char url[256];
	const char *proto=ss->port==443?"https":"http";
	snprintf(url,sizeof(url),"%s://%s:%d%s",proto,ss->host,ss->port,path);
	CURL *curl=curl_easy_init();
	curl_multi_add_handle(ss->curlm,curl);
	curl_easy_setopt(curl,CURLOPT_URL,url);
	if(post && post_len>0)
	{
		curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
		curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,post_len);
	}
	curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,ss->timeout);
	curl_easy_setopt(curl,CURLOPT_ACCEPTTIMEOUT_MS,1000);
	curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,1000);
	l_string_init(&ss->str,16);
	if(!strcmp(proto,"https"))
	{
		curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
		curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0);
	}
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,(void*)write_callback);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,ss);
	struct curl_slist *header = NULL;
	header = curl_slist_append(header, "Cache-Control: no-cache");
	header = curl_slist_append(header, "Content-Type: text/plain");
	curl_easy_setopt(curl,CURLOPT_HTTPHEADER,header);
	CURLMcode mcode;
	int still_running;
	do{
		mcode=curl_multi_perform(ss->curlm,&still_running);
	}while(mcode==CURLM_OK && still_running);
	curl_slist_free_all(header);
	curl_multi_remove_handle(ss->curlm,curl);
	curl_easy_cleanup(curl);
	if(mcode!=CURLM_OK || still_running || ss->abort)
	{
		l_string_clear(&ss->str);
		// printf("curl result code=%d still_running=%d abort=%d\n",mcode,still_running,ss->abort);
		// printf("\turl=%s\n",url);
		return NULL;
	}
	if(len)
		*len=ss->str.len;
	return l_string_steal(&ss->str);
}

int http_session_abort(HttpSession *ss)
{
	if(!ss)
		return -1;
	ss->abort=1;
	curl_multi_wakeup(ss->curlm);
	return 0;
}

