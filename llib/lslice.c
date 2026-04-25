#include "llib.h"

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
	va_start(ap,n);
	for(int i=0;i<n;i++)
	{
		int temp=va_arg(ap,int);
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
		s->pos=L_PAGE_SIZE;
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
		l_slist_free(s->page,(LFreeFunc)l_free_page);
	}
	l_free(r);
}

#ifdef L_MEM_DEBUG
static bool slice_includes(const SLICE *s,const void *mem)
{
	const uint8_t *p=mem;
	for(const PAGE *page=s->page;page!=NULL;page=page->next)
	{
		if(p<page->data || p>page->data+L_PAGE_SIZE-sizeof(PAGE))
			continue;
		if(((p-page->data)%s->block)!=0)
			return false;
		return true;
	}
	return false;
}
static bool slice_not_idle(const SLICE *s,const void *mem)
{
	for(LSList *p=s->idle;p!=NULL;p=p->next)
	{
		if((const void*)p==mem)
			return true;
	}
	return true;
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
		if(s->pos+s->block>L_PAGE_SIZE-sizeof(PAGE))
		{
			PAGE *page=l_alloc_page();
			s->page=l_slist_prepend(s->page,page);
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
	return l_cptr_alloc(size);
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
#ifdef L_MEM_DEBUG
		assert(slice_includes(s,p));
		assert(slice_not_idle(s,p));
#endif
		s->idle=l_slist_prepend(s->idle,p);
		return;
	}
	l_cptr_free(p);
}

