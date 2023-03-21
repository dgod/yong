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

LHashTable *l_hash_table_new(int size,LHashFunc hash,LEqualFunc equal);
void l_hash_table_free(LHashTable *h,LFreeFunc func);
void l_hash_table_clear(LHashTable *h,LFreeFunc func);
void *l_hash_table_find(LHashTable *h,const void *item);
void *l_hash_table_lookup(LHashTable *h,const void *key);
bool l_hash_table_insert(LHashTable *h,void *item);
void *l_hash_table_replace(LHashTable *h,void *item);
void *l_hash_table_remove(LHashTable *h,void *item);
int l_hash_table_size(LHashTable *h);

void l_hash_iter_init(LHashIter *iter,LHashTable *h);
int l_hash_iter_next(LHashIter *iter);
void *l_hash_iter_data(LHashIter *iter);

unsigned l_str_hash (const void *v);
unsigned l_int_hash(const void *v);
int l_int_equal(const void *v1,const void *v2);

#define L_HASH_STRING(n,t,k) 				\
static unsigned n##_hash(const t *p)		\
{											\
	return l_str_hash(p->k);				\
}											\
static int n##_cmp(const t*v1,const t*v2) 	\
{											\
	return strcmp(v1->k,v2->k);				\
}

#define L_HASH_DEREF_MARKER				0x40000000
#define _L_HASH_DEREF_STRING(t,k) (			\
			_Generic(&(((t*)NULL)->k),		\
				signed char **:1,			\
				const signed char **:1,		\
				unsigned char**:1,			\
				const unsigned char **:1,	\
				char **:1,					\
				const char **:1,			\
				void **:1,					\
				const void **:1,			\
				default:0)?L_HASH_DEREF_MARKER:0)
#define L_HASH_TYPE_STRING(t,k) (-(_L_HASH_DEREF_STRING(t,k) | (int)offsetof(t,k)))
#define L_HASH_TABLE_STRING(t,k) l_hash_table_new(L_HASH_TYPE_STRING(t,k),l_str_hash,(LEqualFunc)strcmp)

#define L_HASH_TABLE_INT(t,k) l_hash_table_new(-(int)offsetof(t,k),l_int_hash,l_int_equal)

#endif/*_LHASHTABLE_H_*/

