#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "lsearch.h"
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

void l_isort_r(void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg)
{
	if(nmemb<=1)
		return;
	int p=0;
	for(int i=1;i<nmemb;i++)
	{
		char *cur=(char*)base+i*size;
		int ret=compar(cur,(char*)base+p*size,arg);
		if(ret==0)
		{
			p++;
		}
		else if(ret<0)
		{
			p=l_bsearch_right_r(cur,base,p,size,compar,arg);
		}
		else
		{
			p++;
			if(i>p)
			{
				p+=l_bsearch_right_r(cur,(char*)base+p*size,i-p,size,compar,arg);
			}
		}
		if(p!=i)
		{
			if(size==4)
			{
				int32_t temp=*(int32_t*)cur;
				char *orig=base+size*p;
				memmove(orig+4,orig,(i-p)*4);
				*(int32_t*)orig=temp;
			}
			else
			{
				char temp[size];
				memcpy(temp,cur,size);
				char *orig=base+size*p;
				memmove(orig+size,orig,(i-p)*size);
				memcpy(orig,temp,size);
			}
		}
	}
}

