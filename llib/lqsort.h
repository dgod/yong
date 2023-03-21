#pragma once

#include "ltypes.h"
#include "lmacros.h"

#define l_qsort(base,nmemb,size,compar) qsort((base),(nmemb),(size),(compar))

#if defined(_GNU_SOURCE) && !defined(__ANDROID__)
#define l_qsort_r(base,nmemb,size,compar,arg) qsort_r((base),(nmemb),(size),(compar),(arg))
#else
void l_qsort_r(void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg);
#endif

void l_isort_r(void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg);

