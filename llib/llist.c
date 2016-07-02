#include "ltypes.h"
#include "lmem.h"

struct _llist;
typedef struct _llist LList;
struct _llist{
	LList *next;
	LList *prev;
};

void *l_list_append(LList *h,LList *item)
{
	LList *p;
	if(!h)
	{
		item->prev=item->next=NULL;
		return item;
	}
	for(p=h;p->next!=0;p=p->next);
	p->next=item;item->next=NULL;item->prev=p;
	return h;
}

void *l_list_prepend(LList *h,LList *item)
{
	item->prev=NULL;
	item->next=h;
	if(h) h->prev=item;
	return item;
}

void *l_list_remove(LList *h,LList *item)
{
	if(h==item)
	{
		h=item->next;
		if(h) h->prev=NULL;
	}
	else
	{
		LList *n=item->next,*p=item->prev;
		p->next=n;
		if(n) n->prev=p;
	}
	item->prev=item->next=NULL;
	return h;
}
