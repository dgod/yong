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
void *l_slist_insert(void *h,void *item,int n);
void *l_slist_remove(void *h,void *item);
void l_slist_free(void *h,LFreeFunc func);
void *l_slist_find(void *h,const void *item,LCmpFunc cmp);
void *l_slist_find_r(void *h,const void *item,LCmpDataFunc cmp,void *arg);
int l_slist_length(void *h);
void *l_slist_nth(void *h,int n);
void *l_slist_last(void *h);

#define l_slist_find_by(h,k,cmp,v)					\
(__extension__										\
	({												\
	 	typeof(h) p;								\
		for(p=h;p!=NULL;p=(void*)L_CPTR(p->next))	\
		{											\
			if(!cmp(p->k,v))						\
	 			break;								\
		}											\
		p;											\
	 })												\
)

#if L_WORD_SIZE==64 && L_USE_PTR32

typedef struct _lcslist{
	l_cptr_t next;
}LCSList;

void *l_cslist_append(void *h,void *item);
static inline void *l_cslist_prepend(void *h,void *item)
{
	((LCSList*)item)->next=LPTR_TO_UINT(h);
	return item;
}
void *l_cslist_remove(void *h,void *item);
void l_cslist_free(void *h,LFreeFunc func);
int l_cslist_length(void *h);
void *l_cslist_nth(void *h,int n);
void *l_cslist_insert(void *h,void *item,int n);
void *l_cslist_insert_before(void *h,void *sibling,void *item);
static inline void *l_cslist_insert_after(void *h,void *sibling,void *item)
{
	((LCSList*)item)->next=((LCSList*)sibling)->next;
	((LCSList*)sibling)->next=LPTR_TO_UINT(item);
	return h;
}
#else

#define l_cslist_append(h,item) l_slist_append((h),(item))
#define l_cslist_prepend(h,item) l_slist_prepend((h),(item))
#define l_cslist_remove(h,item) l_slist_remove((h),(item))
#define l_cslist_free(h,func) l_slist_free((h),(func))
#define l_cslist_length(h) l_slist_length(h)
#define l_cslist_nth(h,n) l_slist_nth((h),(n))
#define l_cslist_insert(h,item,n) l_slist_insert((h),(item),(n))
#define l_cslist_insert_before(h,sibling,item) l_slist_insert_before((h),(sibling),(item))
#define l_cslist_insert_after(h,sibling,item) l_slist_insert_after((h),(sibling),(item))

#endif

#define l_cslist_find_by(h,k,cmp,v) l_slist_find_by((h),(k),(cmp),(v))

#endif/*_LSLIST_H_*/
