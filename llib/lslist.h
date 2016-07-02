#ifndef _LSLIST_H_
#define _LSLIST_H_

typedef struct _lslist{
	struct _lslist *next;
}LSList;

void *l_slist_append(void *h,void *item);
void *l_slist_prepend(void *h,void *item);
void *l_slist_remove(void *h,void *item);
void l_slist_free(void *h,LFreeFunc func);
void *l_slist_find(void *h,void *item,LCmpFunc cmp);
int l_slist_length(void *h);
void *l_slist_nth(void *h,int n);

#endif/*_LSLIST_H_*/
