#include <curl/curl.h>

#include "llib.h"
#include "session.h"

struct _HttpSession{
	int port;
	char host[68];
	int timeout;
};

static CURL *curl=NULL;

HttpSession *http_session_new(void)
{
	HttpSession *ss=l_new0(HttpSession);
	ss->timeout=30;
	if(!curl)
		curl=curl_easy_init();
	return ss;
}

void http_session_free(HttpSession *ss)
{
	if(!ss)
		return;
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

static size_t write_callback(char *ptr, size_t size, size_t nmemb,  LString *str)
{
	// printf("write %d\n",size);
	size_t ret=size*nmemb;
	l_string_append(str,ptr,size*nmemb);
	return ret;
}

char *http_session_get(HttpSession *ss,const char *path,int *len,const char *post,int post_len)
{
	char url[256];
	const char *proto=ss->port==443?"https":"http";
	snprintf(url,sizeof(url),"%s://%s:%d%s",proto,ss->host,ss->port,path);
	curl_easy_setopt(curl,CURLOPT_URL,url);
	if(post && post_len>0)
	{
		curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
		curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,post_len);
	}
	curl_easy_setopt(curl,CURLOPT_TIMEOUT,ss->timeout);
	LString str=L_STRING_INIT;
	l_string_expand(&str,16);
	str.str[0]=0;
	if(!strcmp(proto,"https"))
	{
		curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
		curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0);
	}
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,(void*)write_callback);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,&str);
	struct curl_slist *header = NULL;
	header = curl_slist_append(header, "Cache-Control: no-cache");
	header = curl_slist_append(header, "Content-Type: text/plain");
	curl_easy_setopt(curl,CURLOPT_HTTPHEADER,header);
	CURLcode ret=curl_easy_perform(curl);
	curl_slist_free_all(header);
	if(ret!=CURLE_OK)
	{
		l_free(str.str);
		// printf("curl result code=%d\n",ret);
		// printf("\turl=%s\n",url);
		return NULL;
	}
	if(len)
		*len=str.len;
	return str.str;
}
