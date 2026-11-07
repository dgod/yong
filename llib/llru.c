#include "llib.h"

#include "llru.h"

LLRU *l_lru_new(LFreeFunc free)
{
	if(!free)
		return NULL;
	LLRU *lru=l_new0(LLRU);
	if(!lru)
		return NULL;
	lru->free=free;
	lru->size=4;
	return lru;
}

void l_lru_free(LLRU *lru)
{
	if(!lru)
		return;
	l_list_free(lru->head,lru->free);
	l_free(lru);
}

int l_lru_add(LLRU *lru,LLRU_ITEM *item)
{
	if(!lru || !item)
		return -1;
	LLRU_ITEM *head=lru->head;
	lru->head=l_list_prepend(head,item);
	if(!head)
	{
		lru->tail=item;
		lru->last_clear=l_ticks();
	}
	lru->length++;
	if(lru->length>lru->size)
	{
		uint64_t now=l_ticks();
		if(now-lru->last_clear<1000 && lru->size<64)
		{
			lru->size*=2;
		}
		else
		{
			lru->last_clear=now;
			LLRU_ITEM *last=lru->tail;
			lru->tail=last->prev;
			l_list_remove(lru->head,last);
			lru->length--;
			lru->free(last);
		}
	}
	return 0;
}

LLRU_ITEM *l_lru_get(LLRU *lru,uint64_t key)
{
	if(!lru)
		return NULL;
	for(LLRU_ITEM *p=lru->head;p!=NULL;p=p->next)
	{
		if(p->key==key)
		{
			if(p!=lru->head)
			{
				if(p==lru->tail)
					lru->tail=p->prev;
				l_list_remove(lru->head,p);
				lru->head=l_list_prepend(lru->head,p);
			}
			return p;
		}
	}
	return NULL;
}

