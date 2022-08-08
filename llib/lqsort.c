#include <stdlib.h>
#include "lqsort.h"

#ifndef _GNU_SOURCE

// we do not need to be thread safe
#if 0
static __thread void *_arg;
static __thread LCmpDataFunc _compar_data;
#else
static void *_arg;
static LCmpDataFunc _compar_data;
#endif
static int _compar(const void *p1,const void *p2)
{
	return _compar_data(p1,p2,_arg);
}
void l_qsort_r(void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg)
{
	_arg=arg;
	_compar_data=compar;
	qsort(base,nmemb,size,_compar);
	_arg=NULL;
	_compar_data=NULL;
}
#endif

