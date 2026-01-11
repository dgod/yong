#pragma once

#include "llib.h"

#define SG_CACHE_SIZE		13771

typedef struct{
	char *s;
	int l;
}sg_cand_t;

typedef struct{
	void *n;
	char *q;
	sg_cand_t *cs;
	unsigned short l;
	unsigned short c;
}sg_res_t;

typedef struct{
	void *n;
	char *name;
	char *value;
}sg_cookie_t;

typedef struct{
	LHashTable *t;
	sg_res_t *l,*l_old;
	volatile int abort;
	int ready;
	char req[128];
	
	char *format;
	char *proxy;
	char *proxy_auth;
	char *key;
	char *option;
	
	LSList *cookie;
}sg_cache_t;

extern sg_cache_t *l_cache;

struct cloud_api{
	const char *name;
	const char *method;
	const char *host;
	const char *query_key;
	const char *query_res;
	const char *post_res;
	char* (*key_parse)(sg_cache_t *C,char *s);
	sg_res_t* (*res_parse)(sg_cache_t *c,char *s);
};
extern struct cloud_api sg_apis[];
extern struct cloud_api *sg_cur_api;

void sg_cookie_free(sg_cookie_t *cookie);
void sg_recc(sg_cache_t *c,int rec);
void sg_cache_add(sg_cache_t *c,sg_res_t *r);
sg_res_t *sg_cache_get(sg_cache_t *c,const char *s);
sg_res_t *sg_local(sg_res_t *r,const char *p,bool strict);
sg_res_t *sg_parse(sg_cache_t *c,char *s);

void CloudInit(void);
void CloudCleanup(void);
void CloudWaitReady(void);
void CloudSetSignal(void);
void CloudLock(void);
void CloudUnlock(void);
