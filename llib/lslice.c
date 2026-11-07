#include "llib.h"

typedef struct _slice SLICE;
typedef struct _page PAGE;

struct _slice{
	PAGE *page;
	LSList *idle;
	int pos;
	int block;
};

#define BLOCK_MIN		8
#define BLOCK_MAX		24
#define BLOCK_STEP		4
#define SLICE_COUNT		((BLOCK_MAX-BLOCK_MIN)/BLOCK_STEP+1)

typedef struct _lslices{
	int count;
	SLICE slice[SLICE_COUNT];
}LSlices;

struct _page{
	PAGE *next;
	uint8_t data[];
};

static inline int size_to_index(int size)
{
	if(size<=BLOCK_MIN)
		return 0;
	size=(size+(BLOCK_STEP-1))&(~(BLOCK_STEP-1));
	return (size-BLOCK_MIN)/BLOCK_STEP;
}

LSlices *l_slices_new(void)
{
	LSlices *r=l_alloc(sizeof(LSlices));
	r->count=SLICE_COUNT;
	for(int i=0;i<SLICE_COUNT;i++)
	{
		SLICE *s=r->slice+i;
		s->page=NULL;
		s->idle=NULL;
		s->pos=L_PAGE_SIZE;
		s->block=BLOCK_MIN+BLOCK_STEP*i;
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
	int i=size_to_index(size);
	if(i<SLICE_COUNT)
	{
		SLICE *s=r->slice+i;
#ifdef L_MEM_DEBUG
		assert(size<=s->block);
		assert(size<BLOCK_MIN || size>s->block-BLOCK_STEP);
#endif
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
	int i=size_to_index(size);
	if(i<SLICE_COUNT)
	{
		SLICE *s=r->slice+i;
#ifdef L_MEM_DEBUG
		assert(slice_includes(s,p));
		assert(slice_not_idle(s,p));
#endif
		s->idle=l_slist_prepend(s->idle,p);
		return;
	}
	l_cptr_free(p);
}

