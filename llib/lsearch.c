#include "lsearch.h"

int l_bsearch_left(const void *key,const void *base,size_t nmemb,size_t size,LCmpFunc compar)
{
	int b=0,e=nmemb,h,r;
	while(b<e)
	{
		h=b+(e-b)/2;
		r=compar(key,(const char*)base+size*h);
		if(r>0)
		{
			b=h+1;
		}
		else
		{
			e=h;
		}
	}
	return b;
}

int l_bsearch_right(const void *key,const void *base,size_t nmemb,size_t size,LCmpFunc compar)
{
	int b=0,e=nmemb,h,r;
	while(b<e)
	{
		h=b+(e-b)/2;
		r=compar(key,(const char*)base+size*h);
		if(r>=0)
		{
			b=h+1;
		}
		else
		{
			e=h;
		}
	}
	return e;
}
