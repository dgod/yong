#ifndef _LHASHTABLE_H_
#define _LHASHTABLE_H_

struct _lhashtable;
typedef struct _lhashtable LHashTable;

typedef struct{
	LHashTable *h;
	int index;
	void *item;
	void *next;
}LHashIter;

LHashTable *l_hash_table_new(int size,LHashFunc hash,LCmpFunc cmp);
void l_hash_table_free(LHashTable *h,LFreeFunc func);
void *l_hash_table_find(LHashTable *h,void *item);
bool l_hash_table_insert(LHashTable *h,void *item);
void *l_hash_table_replace(LHashTable *h,void *item);
void l_hash_table_remove(LHashTable *h,void *item);
int l_hash_table_size(LHashTable *h);

void l_hash_iter_init(LHashIter *iter,LHashTable *h);
int l_hash_iter_next(LHashIter *iter);
void *l_hash_iter_data(LHashIter *iter);

#define L_HASH_STRING(n,t,k) 				\
static unsigned n##_hash(const t *p)		\
{											\
	return l_str_hash(p->k);				\
}											\
static int n##_cmp(const t*v1,const t*v2) 	\
{											\
	return strcmp(v1->k,v2->k);				\
}

#endif/*_LHASHTABLE_H_*/

