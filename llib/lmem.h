#ifndef _LMEM_H_
#define _LMEM_H_

#include "lmacros.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

#define l_alloc(s) malloc(s)
#define l_calloc(n,s) malloc((n)*(s))
#define l_alloc0(s) calloc(1,(s))
#define l_calloc0(n,s) calloc((n),(s))
#define l_realloc(p,s) realloc((p),(s))
#define l_recalloc(p,n,s) realloc((p),(n)*(s))
#define L_ALLOC(p) malloc(sizeof(*p))
#define L_ALLOC0(p) calloc(1,sizeof(*p))
#define l_new(t) ((t*)malloc(sizeof(t)))
#define l_cnew(n,t) ((t*)malloc((n)*sizeof(t)))
#define l_new0(t) ((t*)calloc(1,sizeof(t)))
#define l_cnew0(n,t) ((t*)calloc(n,sizeof(t)))
#define l_renew(p,n,t) ((t*)realloc((p),(n)*sizeof(t)))
#define l_memdup(p,s) memcpy(malloc(s),p,s)
#define l_strdup(p) strdup(p)
#define l_free free
#define l_zfree(p) do{l_free(p);p=NULL;}while(0)

static inline void *l_memcpy0(void *dest,const void *src,size_t n)
{
	memcpy(dest,src,n);
	((char*)dest)[n]=0;
	return dest;
}

static inline void *l_memdup0(const void *p,size_t s)
{
	char *r=malloc(s+1);
	memcpy(r,p,s);
	r[s]=0;
	return r;
}

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#define l_strndup(p,n) strndup(p,n)
#define l_alloca(s) alloca(s)
#define l_strdupa(s) ((s)?strcpy(l_alloca(strlen(s)+1),(s)):NULL)
#define l_strndupa(s,n) l_strncpy(l_alloca(n+1),(s),(n))
#elif !defined(_WIN32)
#define l_strndup(p,n) strndup(p,n)
#define l_alloca(s) alloca(s)
#define l_strdupa(s) ((s)?strdupa(s):NULL)
#ifdef strndupa
#define l_strndupa(s,n) strndupa(s,n)
#else
#define l_strndupa(s,n) l_strncpy(l_alloca(n+1),(s),(n))
#endif
#else
#define l_strndup(s,n) l_strncpy(l_alloc(n+1),(s),(n))
#define l_alloca(s) _alloca(s)
#define l_strdupa(s) ((s)?strcpy(l_alloca(strlen(s)+1),(s)):NULL)
#define l_strndupa(s,n) l_strncpy(l_alloca(n+1),(s),(n))
#endif

#define l_alloca0(s) memset(l_alloc(s),0,(s))
#define l_newa(t) ((t*)l_alloc(sizeof(t)))
#define l_newa0(t) ((t*)l_alloca0(sizeof(t)))
#define l_memdupa(p,s) memcpy(l_alloca(s),p,s)

#ifdef __GNUC__
#define l_memdupa0(p,s)					\
(__extension__							\
 	({									\
	 	int __s=(int)(s);				\
		char *__r=l_alloca(__s+1);		\
		memcpy(__r,(p),__s);			\
		__r[__s]=0;						\
		__r;							\
	})									\
)
#endif

typedef struct _lslices LSlices;
LSlices *l_slices_new(int n,...);
void l_slices_free(LSlices *r);
void *l_slice_alloc(LSlices *r,int size);
#define l_slice_new(r,t) l_slice_alloc(r,sizeof(t))
void l_slice_free(LSlices *r,void *p,int size);

#ifdef __GNUC__
#define L_AUTO_FREE_X(p) __attribute__((cleanup(p))
#define L_AUTO_FREE L_AUTO_FREE_X(l_free)
#endif

#endif/*_LMEM_H_*/

