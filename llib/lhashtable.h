#ifndef _LHASHTABLE_H_
#define _LHASHTABLE_H_

struct _lhashtable;
typedef struct _lhashtable LHashTable;

typedef struct{
	LHashTable *h;
	int index;
	void *next;
}LHashIter;

LHashTable *l_hash_table_new(LHashFunc hash,LCmpFunc cmp,int size,int offset);
void l_hash_table_free(LHashTable *h,LFreeFunc func);
void l_hash_table_clear(LHashTable *h,LFreeFunc func);
void *l_hash_table_find(LHashTable *h,const void *item);
void *l_hash_table_lookup(LHashTable *h,const void *key);
bool l_hash_table_insert(LHashTable *h,void *item);
void *l_hash_table_replace(LHashTable *h,void *item);
void *l_hash_table_remove(LHashTable *h,void *item);
void *l_hash_table_del(LHashTable *h,const void *key);
int l_hash_table_size(LHashTable *h);
void *l_hash_table_rand(LHashTable *h);

void l_hash_iter_init(LHashIter *iter,LHashTable *h);
void *l_hash_iter_next(LHashIter *iter);

uint32_t l_str_hash (const void *v);
uint32_t l_str_hash2 (const void *v,int len);
uint32_t l_int_hash(const void *v);

#define _L_HASH_DEREF_STRING(t,k) (			\
			_Generic(&(((t*)NULL)->k),		\
				signed char **:-1,			\
				const signed char **:-1,	\
				unsigned char**:-1,			\
				const unsigned char **:-1,	\
				char **:-1,					\
				const char **:-1,			\
				void **:-1,					\
				const void **:-1,			\
				default:1))
#define L_HASH_TYPE_STRING(t,k) (_L_HASH_DEREF_STRING(t,k) * (int)offsetof(t,k))
#define L_HASH_TABLE_STRING(t,k,s) l_hash_table_new(l_str_hash,(LEqualFunc)strcmp,(s),L_HASH_TYPE_STRING(t,k))
#define L_HASH_TABLE_INT(t,k,s) l_hash_table_new(l_int_hash,l_int_equal,(s),(int)offsetof(t,k))

#ifdef __GNUC__
#define L_HASH_TABLE_LOOKUP_INT(t,i)		\
(__extension__								\
	({										\
	 	typeof(i) __i=i;					\
		l_hash_table_lookup(t,&__i);		\
	})										\
)
#endif

static inline LHashTable *l_string_set_new(int size)
{
	return l_hash_table_new(l_str_hash,(LEqualFunc)strcmp,size,sizeof(void*));
}

bool l_string_set_insert2(LHashTable *h,const char *s,int len);

static inline bool l_string_set_insert(LHashTable *h,const char *s)
{
	return l_string_set_insert2(h,s,-1);
}

static inline bool l_string_set_has(LHashTable *h,const char *s)
{
	return l_hash_table_lookup(h,s)?true:false;
}

static inline void l_string_set_del(LHashTable *h,const char *s)
{
	void *item=l_hash_table_del(h,s);
	l_free(item);
}

static inline void l_string_set_free(LHashTable *h)
{
	l_hash_table_free(h,l_free);
}

static inline void l_string_set_clear(LHashTable *h)
{
	l_hash_table_clear(h,l_free);
}

#endif/*_LHASHTABLE_H_*/

