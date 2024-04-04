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
#define l_free free
#define l_strdup(p) strdup(p)

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

#endif/*_LMEM_H_*/

