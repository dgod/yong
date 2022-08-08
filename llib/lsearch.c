#include "lsearch.h"

int l_bsearch_r(const void *key,const void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg)
{
	int b=0,e=nmemb,h,r;
	while(b<e)
	{
		h=b+(e-b)/2;
		r=compar(key,(const char*)base+size*h,arg);
		if(r>0)
		{
			b=h+1;
		}
		else if(r<0)
		{
			e=h;
		}
		else
		{
			b=h;
			break;
		}
	}
	return b;
}

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

int l_bsearch_left_r(const void *key,const void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg)
{
	int b=0,e=nmemb,h,r;
	while(b<e)
	{
		h=b+(e-b)/2;
		r=compar(key,(const char*)base+size*h,arg);
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

int l_bsearch_right_r(const void *key,const void *base,size_t nmemb,size_t size,LCmpDataFunc compar,void *arg)
{
	int b=0,e=nmemb,h,r;
	while(b<e)
	{
		h=b+(e-b)/2;
		r=compar(key,(const char*)base+size*h,arg);
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

