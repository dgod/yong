#pragma once

typedef struct _llist{
	struct _llist *next;
	struct _llist *prev;
}LList;

void *l_list_append(void *h,void *item);
void *l_list_prepend(void *h,void *item);
void *l_list_remove(void *h,void *item);
#define l_list_free(h,func) l_slist_free((h),(func))
#define l_list_find(h,item,cmp) l_slist_find((h),(item),(cmp))
#define l_list_length(h) l_slist_length(h)
#define l_list_nth(h,n) l_slist_nth((h),(n))
