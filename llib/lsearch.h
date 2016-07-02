#pragma once

#include "ltypes.h"

int l_bsearch_left(const void *key,const void *base,size_t nmemb,size_t size,LCmpFunc compar);
int l_bsearch_right(const void *key,const void *base,size_t nmemb,size_t size,LCmpFunc compar);
