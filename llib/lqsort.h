#pragma once

#include "ltypes.h"

#define l_qsort(base,nmemb,size,compar) qsort((base),(nmemb),(size),(compar))

#ifdef _GNU_SOURCE
#define l_qsort_r(base,nmemb,size,compar,arg) qsort_r((base),(nmemb),(size),(compar),(arg))
#else
void l_qsort_r(void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg);
#endif

