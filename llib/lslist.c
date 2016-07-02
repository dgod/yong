#include "ltypes.h"
#include "lmem.h"

struct _lslist;
typedef struct _lslist LSList;
struct _lslist{
	LSList *next;
};

LSList *l_slist_append(LSList *h,void *data)
{
	LSList *n=(LSList*)data;
	LSList *p;
	n->next=0;
	if(!h) return n;
	for(p=h;p->next!=0;p=p->next);
	p->next=n;
	return h;
}

LSList *l_slist_prepend(LSList *h,void *data)
{
	LSList *p=(LSList*)data;
	p->next=h;
	return p;
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

void *l_slist_find(void *h,void *item,LCmpFunc cmp)
{
	LSList *p=h;
	for(;p!=0;p=p->next)
	{
		if(0==cmp(p,item))
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
	for(i=0;i<n;i++)
		p=p->next;
	return p;
}

