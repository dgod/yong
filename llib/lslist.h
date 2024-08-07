#ifndef _LSLIST_H_
#define _LSLIST_H_

typedef struct _lslist{
	struct _lslist *next;
}LSList;

void *l_slist_append(void *h,void *item);
static inline void *l_slist_prepend(void *h,void *item)
{
	((LSList*)item)->next=h;
	return item;
}
void *l_slist_insert_before(void *h,void *sibling,void *item);
static inline void *l_slist_insert_after(void *h,void *sibling,void *item)
{
	((LSList*)item)->next=((LSList*)sibling)->next;
	((LSList*)sibling)->next=item;
	return h;
}
void *l_slist_remove(void *h,void *item);
void l_slist_free(void *h,LFreeFunc func);
void *l_slist_find(void *h,const void *item,LCmpFunc cmp);
void *l_slist_find_r(void *h,const void *item,LCmpDataFunc cmp,void *arg);
int l_slist_length(void *h);
void *l_slist_nth(void *h,int n);
void *l_slist_last(void *h);

#endif/*_LSLIST_H_*/
