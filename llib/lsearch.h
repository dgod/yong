#pragma once

#include "ltypes.h"

#define l_bsearch(key,base,nmemb,size,compar) bsearch((key),(base),(nmemb),(size),(compar))
int l_bsearch_left(const void *key,const void *base,size_t nmemb,size_t size,LCmpFunc compar);
int l_bsearch_right(const void *key,const void *base,size_t nmemb,size_t size,LCmpFunc compar);
int l_bsearch_r(const void *key,const void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg);
int l_bsearch_left_r(const void *key,const void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg);
int l_bsearch_right_r(const void *key,const void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg);

