#include "ltypes.h"
#include "lmem.h"
#include "lmman.h"

#include <assert.h>

struct _lslist;
typedef struct _lslist LSList;
struct _lslist{
	LSList *next;
};

LSList *l_slist_append(LSList *h,LSList *n)
{
	LSList *p;
	n->next=0;
	if(!h) return n;
	for(p=h;p->next!=0;p=p->next);
	p->next=n;
	return h;
}

void *l_slist_insert_before(LSList *h,LSList *sibling,LSList *data)
{
	data->next=sibling;
	if(h==sibling)
		return data;
	LSList *p=h;
	while(1)
	{
		if(p->next==sibling)
		{
			p->next=data;
			return h;
		}
		p=p->next;
		assert(p!=NULL);
	}
}

void l_slist_free(LSList *h,void (*func)(void*))
{
	LSList *p,*n;
	for(p=h;p!=0;p=n)
	{
		n=p->next;
		func(p);
	}
}

void *l_slist_insert(LSList *h,LSList *item,int n)
{
	LSList *prev=NULL,*cur=h;
	for(int i=0;i<n && cur;i++)
	{
		prev=cur;
		cur=cur->next;
	}
	item->next=cur;
	if(prev)
	{
		prev->next=item;
		return h;
	}
	return item;
}

LSList *l_slist_remove(LSList *h,void *data)
{
	LSList *p;
	if(!h) return 0;
	if(h==data)
	{
		p=h->next;
		return p;
	}
	for(p=h;p->next!=0;p=p->next)
	{
		if(p->next==data)
		{
			p->next=p->next->next;
			break;
		}
	}
	return h;
}

void *l_slist_find(void *h,const void *item,LCmpFunc cmp)
{
	LSList *p=h;
	for(;p!=0;p=p->next)
	{
		if(0==cmp(p,item))
			return p;
	}
	return NULL;
}

void *l_slist_find_r(void *h,const void *item,LCmpDataFunc cmp,void *arg)
{
	LSList *p=h;
	for(;p!=0;p=p->next)
	{
		if(0==cmp(p,item,arg))
			return p;
	}
	return NULL;
}

int l_slist_length(void *h)
{
	LSList *p=h;
	int i;
	for(i=0;p!=NULL;p=p->next,i++);
	return i;
}

void *l_slist_nth(void *h,int n)
{
	LSList *p=h;
	int i;
	for(i=0;i<n && p;i++)
		p=p->next;
	return p;
}

void *l_slist_last(void *h)
{
	LSList *p=h;
	if(!p)
		return NULL;
	for(;p->next!=NULL;p=p->next);
	return p;
}

#if L_WORD_SIZE==64 && L_USE_PTR32

typedef struct _lcslist{
	l_cptr_t next;
}LCSList;

LCSList *l_cslist_append(LCSList *h,LCSList *n)
{
	LCSList *p;
	n->next=0;
	if(!h) return n;
	for(p=h;p->next!=0;p=L_CPTR_NEXT(p));
	p->next=LPTR_TO_UINT(n);
	return h;
}

void *l_cslist_insert(LCSList *h,LCSList *item,int n)
{
	LCSList *prev=NULL,*cur=h;
	for(int i=0;i<n && cur;i++)
	{
		prev=cur;
		cur=L_CPTR(cur->next);
	}
	item->next=L_CPTR_T(cur);
	if(prev)
	{
		prev->next=L_CPTR_T(item);
		return h;
	}
	return item;
}

void *l_cslist_insert_before(LCSList *h,LCSList *sibling,LCSList *data)
{
	data->next=L_CPTR_T(sibling);
	if(h==sibling)
		return data;
	LCSList *p=h;
	while(1)
	{
		LCSList *n=L_CPTR(p->next);
		if(n==sibling)
		{
			p->next=L_CPTR_T(data);
			return h;
		}
		p=n;
		assert(p!=NULL);
	}
}

LCSList *l_cslist_remove(LCSList *h,LCSList *data)
{
	LCSList *p,*n;
	if(!h) return 0;
	if(h==data)
	{
		p=L_CPTR_NEXT(h);
		return p;
	}
	for(p=h;p->next!=0;p=n)
	{
		n=L_CPTR_NEXT(p);
		if(n==data)
		{
			p->next=n->next;
			break;
		}
	}
	return h;
}

void l_cslist_free(LCSList *h,void (*func)(void*))
{
	LCSList *p,*n;
	for(p=h;p!=0;p=n)
	{
		n=L_CPTR_NEXT(p);
		func(p);
	}
}
int l_cslist_length(void *h)
{
	LCSList *p=h;
	int i;
	for(i=0;p!=NULL;p=L_CPTR(p->next),i++);
	return i;
}

void *l_cslist_nth(void *h,int n)
{
	LCSList *p=h;
	for(int i=0;i<n && p;i++)
		p=L_CPTR(p->next);
	return p;
}

#endif

