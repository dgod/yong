#include "ltypes.h"
#include "lqueue.h"
#include "lslist.h"
#include "llist.h"
#include "lmem.h"

#include <assert.h>
#include <stdio.h>

LQueue *l_queue_new(LFreeFunc free)
{
	LQueue *res=l_new(LQueue);
	res->head=res->tail=NULL;
	res->length=0;
	res->free=free;
	return res;
}

void l_queue_free(LQueue *q)
{
	if(!q) return;
	l_list_free(q->head,q->free);
	l_free(q);
}

void l_queue_push_head(LQueue *q,void *data)
{
	LList *head=q->head;
	q->head=l_list_prepend(head,data);
	if(!head) q->tail=data;
	q->length++;
}

void l_queue_push_tail(LQueue *q,void *data)
{
	LList *tail=q->tail,*p=data;
	if(!tail)
	{
		q->head=q->tail=p;
		p->prev=p->next=NULL;
	}
	else
	{
		tail->next=p;
		p->prev=tail;
		p->next=NULL;
		q->tail=p;
	}
	q->length++;
}

void *l_queue_pop_head(LQueue *q)
{
	void *res=q->head;
	if(!res) return NULL;
	q->head=l_list_remove(q->head,q->head);
	q->length--;
	if(!q->head) q->tail=NULL;
	return res;
}

void l_queue_remove(LQueue *q,void *data)
{
	if(data==q->tail)
		q->tail=((LList*)data)->prev;
	q->head=l_list_remove(q->head,data);
	q->length--;
}
