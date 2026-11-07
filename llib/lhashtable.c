#include "ltypes.h"
#include "lslist.h"
#include "lmem.h"
#include "lhashtable.h"
#include "lfuncs.h"

#include <assert.h>

struct _lhashtable{
	uint32_t size;
	uint32_t offset;
	int deref;
	uint32_t count;
	uint32_t mu;
	void **array;
	LHashFunc hash;
	LCmpFunc cmp;
};
static const int prime_mod[]={
	251,509,1021,2039,4093,8191,16381,32749,65521,
	131071,262139,524287,1048573,2097143,4194301,
	8388593,16777213,33554393,67108859,134217689,
	268435399,536870909,1073741789,2147483647
};

static inline int get_real_size(int size)
{
	int i;
	for(i=0;i<L_ARRAY_SIZE(prime_mod);i++)
	{
		if(prime_mod[i]>=size)
			break;
	}
	assert(i!=L_ARRAY_SIZE(prime_mod));
	return prime_mod[i];
}

#if L_WORD_SIZE==64
static inline void fast_mod_init(LHashTable *f)
{
	f->mu = (uint32_t)(UINT32_MAX / f->size);
}

static inline uint32_t fast_mod_map(const LHashTable *f,uint32_t v)
{
	uint32_t size=f->size;
	uint32_t q = (uint32_t)((uint64_t)v * f->mu >> 32);
	uint32_t r = v - q * size;
	if (r >= size) r -= size;
	return r;
}
#else
#define fast_mod_init(f)
#define fast_mod_map(f,v)	((v)%(f)->size)
#endif

LHashTable *l_hash_table_new(LHashFunc hash,LCmpFunc cmp,int size,int offset)
{
	LHashTable *h=l_new(struct _lhashtable);
	h->size=get_real_size(size);
	fast_mod_init(h);
	if(offset<0)
	{
		h->offset=-offset;
		h->deref=1;
	}
	else
	{
		h->offset=offset;
		h->deref=0;
	}
	h->hash=hash;
	h->cmp=cmp;
	h->array=l_cnew0(h->size,void*);
	h->count=0;
	return h;
}

void l_hash_table_free(LHashTable *h,LFreeFunc func)
{
	if(!h) return;
	if(func)
	{
		for(int i=0;i<h->size;i++)
			l_slist_free(h->array[i],func);
	}
	l_free(h->array);
	l_free(h);
}

void l_hash_table_clear(LHashTable *h,LFreeFunc func)
{
	if(!h || !h->count)
		return;
	for(int i=0;i<h->size;i++)
	{
		if(func)
			l_slist_free(h->array[i],func);
		h->array[i]=NULL;
	}
	h->count=0;
}

static inline uint32_t _hash_index(LHashTable *h,const void *item)
{
	if(h->offset)
	{
		item=(const char*)item+h->offset;
		if(h->deref)
			item=*(void**)item;
	}
	return fast_mod_map(h,h->hash(item));
}

void *l_hash_table_find(LHashTable *h,const void *item)
{
	if(h->offset)
	{
		item=(char*)item+h->offset;
		if(h->deref)
			item=*(void**)item;
		return l_hash_table_lookup(h,item);
	}
	uint32_t index=fast_mod_map(h,h->hash(item));
	return l_slist_find(h->array[index],item,h->cmp);
}

static inline void *_slist_find_key(void *p,const void *key,LHashTable *h)
{
	for(;p!=NULL;p=*(void**)p)
	{
		void *pkey=(char*)p+h->offset;
		if(h->deref)
			pkey=*(void**)pkey;
		if(0==h->cmp(pkey,key))
			return p;
	}
	return NULL;
}

static inline void *_slist_find_item(void *p,const void *item,LHashTable *h)
{
	if(h->offset)
	{
		item=(char*)item+h->offset;
		if(h->deref)
			item=*(void**)item;
		return _slist_find_key(p,item,h);
	}
	else
	{
		return l_slist_find(p,item,h->cmp);
	}
}

void *l_hash_table_lookup(LHashTable *h,const void *key)
{
	int index=fast_mod_map(h,h->hash(key));
	return _slist_find_key(h->array[index],key,h);
}

static void _hash_resize(LHashTable *h)
{
	int size=h->size,i;
	void **array=h->array;
	h->size=get_real_size(size+1);
	fast_mod_init(h);
	h->array=l_cnew0(h->size,void*);
	for(i=0;i<size;i++)
	{
		void *head=array[i],*p;
		while((p=head)!=NULL)
		{
			head=*(void**)p;
			uint32_t index=_hash_index(h,p);
			h->array[index]=l_slist_prepend(h->array[index],p);
		}
	}
	l_free(array);
}

bool l_hash_table_insert(LHashTable *h,void *item)
{
	int index=_hash_index(h,item);
	void *old=_slist_find_item(h->array[index],item,h);
	if(old) return false;
	if(h->count>=h->size)
	{
		_hash_resize(h);
		index=_hash_index(h,item);
	}
	h->array[index]=l_slist_prepend(h->array[index],item);
	h->count++;
	return true;
}

void *l_hash_table_replace(LHashTable *h,void *item)
{
	uint32_t index=_hash_index(h,item);
	void *old=_slist_find_item(h->array[index],item,h);
	if(old)
	{
		h->array[index]=l_slist_remove(h->array[index],old);
	}
	else
	{
		if(h->count>=h->size)
		{
			_hash_resize(h);
			index=_hash_index(h,item);
		}
		h->count++;
	}
	h->array[index]=l_slist_prepend(h->array[index],item);
	return old;
}

void *l_hash_table_remove(LHashTable *h,void *item)
{
	uint32_t index=_hash_index(h,item);
	void *old=_slist_find_item(h->array[index],item,h);
	if(!old)
		return NULL;
	h->array[index]=l_slist_remove(h->array[index],old);
	h->count--;
	return old;
}

void *l_hash_table_del(LHashTable *h,const void *key)
{
	if(!h->offset)
		return NULL;
	uint32_t index=fast_mod_map(h,h->hash(key));
	void *item=_slist_find_key(h->array[index],key,h);
	if(!item)
		return NULL;
	h->array[index]=l_slist_remove(h->array[index],item);
	h->count--;
	return item;
}

void *l_hash_table_rand(LHashTable *h)
{
	if(h->count==0)
		return NULL;
	const int pos=l_rand(0,h->size-1);
	for(int i=pos;;)
	{
		LSList *p=h->array[i];
		if(p==NULL)
		{
			i++;
			if(i==h->size)
				i=0;
			continue;
		}
		if(!p->next)
			return p;
		int count=l_slist_length(p);
		i=l_rand(0,count-1);
		return l_slist_nth(p,i);
	}
	return NULL;
}

int l_hash_table_size(LHashTable *h)
{
	return h->count;
}

void l_hash_iter_init(LHashIter *iter,LHashTable *h)
{
	iter->h=h;
	iter->index=-1;
	iter->next=NULL;
}

void *l_hash_iter_next(LHashIter *iter)
{
	LHashTable *h=iter->h;
	void *ret=iter->next;
	if(ret!=NULL)
	{
		iter->next=*(void**)ret;
		return ret;
	}
	for(int i=iter->index+1;i<h->size;i++)
	{
		if(!h->array[i])
			continue;
		iter->index=i;
		ret=h->array[i];
		iter->next=*(void**)ret;
		return ret;
	}
	return NULL;
}

uint32_t l_str_hash (const void *v)
{
	const uint8_t *p = v;
	uint32_t h = *p;

	if (h) for (p += 1; *p != '\0'; p++)
		h = h*131 + *p;

	return h;
}

uint32_t l_str_hash2 (const void *v,int len)
{
	const uint8_t *p = v;
	unsigned h = *p++;
	for(int i=1;i<len;i++)
		h = h*131 + *p++;
	return h;
}

uint32_t l_int_hash(const void *v)
{
	return *(const uint32_t *)v;
}

// only support c str
bool l_string_set_insert2(LHashTable *h,const char *s,int len)
{
	char *item;
	int index;
	uint32_t hash_val;
	if(len<0)
	{
		len=strlen(s);
		hash_val=l_str_hash2(s,len);
		index=fast_mod_map(h,hash_val);
		void *old=_slist_find_key(h->array[index],s,h);
		if(old)
		{
			return false;
		}
		item=l_alloc(sizeof(void*)+len+1);
		memcpy(item+sizeof(void*),s,len+1);
		s=item+sizeof(void*);
	}
	else
	{
		item=l_alloc(sizeof(void*)+len+1);
		memcpy(item+sizeof(void*),s,len);
		item[sizeof(void*)+len]=0;
		s=item+sizeof(void*);
		hash_val=l_str_hash2(s,len);
		index=fast_mod_map(h,hash_val);
		void *old=_slist_find_key(h->array[index],s,h);
		if(old)
		{
			l_free(item);
			return false;
		}
	}
	if(h->count>=h->size)
	{
		_hash_resize(h);
		index=fast_mod_map(h,hash_val);
	}
	h->array[index]=l_slist_prepend(h->array[index],item);
	h->count++;
	return true;
}
