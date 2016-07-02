#ifndef _LQUEUE_H_
#define _LQUEUE_H_

typedef struct {
	void *head;
	void *tail;
	int length;
	LFreeFunc free;
}LQueue;

LQueue *l_queue_new(LFreeFunc free);
void l_queue_free(LQueue *q);
void l_queue_push_head(LQueue *q,void *data);
void l_queue_push_tail(LQueue *q,void *data);
void *l_queue_pop_head(LQueue *q);
void *l_queue_peek_head(LQueue *q);
int l_queue_length(LQueue *q);
void l_queue_remove(LQueue *q,void *data);
#define l_queue_is_empty(q) (((q)->head)?0:1)
#define l_queue_find(q,item,cmp) l_list_find((q)->head,(item),(cmp))

#endif/*_LQUEUE_H_*/
