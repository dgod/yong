#include "llib.h"

#define SLICE_DEBUG			0

#if SLICE_DEBUG || defined(EMSCRIPTEN)

#define SLICE_PAGE_SIZE		(16*1024)
static inline void *alloc_page(void)
{
	return malloc(SLICE_PAGE_SIZE);
}
static inline void free_page(void *p)
{
	free(p);
}
#elif defined(_WIN32)
#include <windows.h>
#define SLICE_PAGE_SIZE		(16*1024)
static inline void *alloc_page(void)
{
	return VirtualAlloc(NULL,SLICE_PAGE_SIZE,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
}
static inline void free_page(void *p)
{
	VirtualFree(p,0,MEM_RELEASE);
}
#else
#include <sys/mman.h>
#include <unistd.h>
static int SLICE_PAGE_SIZE=0;
static inline void *alloc_page(void)
{
	return mmap(NULL,SLICE_PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
}

static inline void free_page(void *p)
{
	munmap(p,SLICE_PAGE_SIZE);
}

#endif

typedef struct _slice SLICE;
typedef struct _page PAGE;

struct _slice{
	PAGE *page;
	LSList *idle;
	int pos;
	int block;
};

typedef struct _lslices{
	int count;
	SLICE slice[];
}LSlices;

struct _page{
	PAGE *next;
	uint8_t data[];
};

LSlices *l_slices_new(int n,...)
{
	int size[n];
	int count=0;
	va_list ap;
#ifndef SLICE_PAGE_SIZE
	if(!SLICE_PAGE_SIZE)
	{
		SLICE_PAGE_SIZE=(int)sysconf(_SC_PAGESIZE);
		if(SLICE_PAGE_SIZE<16*1024)
			SLICE_PAGE_SIZE=16*1024;
	}
#endif
	va_start(ap,n);
	for(int i=0;i<n;i++)
	{
		int temp=va_arg(ap,int);
		// printf("%d: %d\n",i,temp);
		int j=array_index(size,count,temp);
		if(j>=0)
			continue;
		assert(temp<64);
		size[count++]=temp;
	}
	va_end(ap);
	qsort(size,count,sizeof(int),l_int_equal);
	LSlices *r=l_alloc(sizeof(LSlices)+sizeof(SLICE)*count);
	r->count=count;
	for(int i=0;i<count;i++)
	{
		SLICE *s=r->slice+i;
		s->page=NULL;
		s->idle=NULL;
		s->pos=SLICE_PAGE_SIZE;
		s->block=size[i];
	}
	return r;
}

void l_slices_free(LSlices *r)
{
	if(!r)
		return;

	for(int i=0;i<r->count;i++)
	{
		SLICE *s=r->slice+i;
		PAGE *page=s->page;
		while(page)
		{
			PAGE *next=page->next;
			free_page(page);
			page=next;
		}
	}
	l_free(r);
}

#if SLICE_DEBUG
static bool slice_includes(const SLICE *s,const void *mem)
{
	const PAGE *page=s->page;
	const uint8_t *p=mem;
	while(page!=NULL)
	{
		if(p>=page->data && p<page->data+SLICE_PAGE_SIZE-sizeof(PAGE))
			return true;
		page=page->next;
	}
	return false;
}
#endif

void *l_slice_alloc(LSlices *r,int size)
{
	for(int i=0;i<r->count;i++)
	{
		SLICE *s=r->slice+i;
		if(s->block<size)
			continue;
		LSList *ret=s->idle;
		if(ret!=NULL)
		{
			s->idle=ret->next;
			return ret;
		}
		if(s->pos+s->block>SLICE_PAGE_SIZE-sizeof(PAGE))
		{
			PAGE *page=alloc_page();
			page->next=s->page;
			s->page=page;
			s->pos=s->block;
			return page->data;
		}
		else
		{
			ret=(void*)(s->page->data+s->pos);
			s->pos+=s->block;
			return ret;
		}
	}
	return l_alloc(size);
}

void l_slice_free(LSlices *r,void *p,int size)
{
	if(!p)
		return;
	for(int i=0;i<r->count;i++)
	{
		SLICE *s=r->slice+i;
		if(s->block<size)
			continue;
#if SLICE_DEBUG
		assert(slice_includes(s,p));
#endif
		s->idle=l_slist_prepend(s->idle,p);
		return;
	}
	free(p);
}

