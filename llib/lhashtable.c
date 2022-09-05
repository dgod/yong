#include "ltypes.h"
#include "lslist.h"
#include "lmem.h"
#include "lhashtable.h"

#include <assert.h>

struct _lhashtable{
	int size;
	int offset;
	int deref;
	LHashFunc hash;
	LCmpFunc cmp;
	void **array;
	uint32_t count;
};

LHashTable *l_hash_table_new(int size,LHashFunc hash,LCmpFunc cmp)
{
	LHashTable *h;
	h=l_new0(struct _lhashtable);
	if(size<0)
	{
		size=-size;
		h->size=256;
		h->offset=size&~L_HASH_DEREF_MARKER;
		h->deref=(size&L_HASH_DEREF_MARKER)?1:0;
		assert(h->offset>=sizeof(void*));
	}
	else
	{
		if(size<=1)
			size=256;
		h->size=size;
		h->offset=0;
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
		int i;
		for(i=0;i<h->size;i++)
			l_slist_free(h->array[i],func);
	}
	l_free(h->array);
	l_free(h);
}

void l_hash_table_clear(LHashTable *h,LFreeFunc func)
{
	if(!h) return;
	int i;
	for(i=0;i<h->size;i++)
	{
		if(func)
			l_slist_free(h->array[i],func);
		h->array[i]=NULL;
	}
	h->count=0;
}

static inline int _hash_index(LHashTable *h,const void *item)
{
	if(h->offset)
	{
		item=(const char*)item+h->offset;
		if(h->deref)
			item=*(void**)item;
	}
	return h->hash(item)%h->size;
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
	int index=h->hash(item)%h->size;
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
	if(!h->offset)
		return NULL;
	int index=h->hash(key)%h->size;
	return _slist_find_key(h->array[index],key,h);
}

static void _hash_resize(LHashTable *h)
{
	int size=h->size,i;
	void **array=h->array;
	h->size=size*3/2;
	h->array=l_cnew0(h->size,void*);
	for(i=0;i<size;i++)
	{
		void *head=array[i],*p;
		while((p=head)!=NULL)
		{
			head=*(void**)p;
			int index=_hash_index(h,p);
			h->array[index]=l_slist_prepend(h->array[index],p);
		}
	}
	l_free(array);
}

bool l_hash_table_insert(LHashTable *h,void *item)
{
	if(h->count>h->size)
		_hash_resize(h);
	int index=_hash_index(h,item);
	void *old=_slist_find_item(h->array[index],item,h);
	if(old) return false;
	h->array[index]=l_slist_prepend(h->array[index],item);
	h->count++;
	return true;
}

void *l_hash_table_replace(LHashTable *h,void *item)
{
	int index=_hash_index(h,item);
	void *old=_slist_find_item(h->array[index],item,h);
	if(old)
	{
		h->array[index]=l_slist_remove(h->array[index],old);
	}
	else
	{
		if(h->count>h->size)
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
	int index=_hash_index(h,item);
	void *old=_slist_find_item(h->array[index],item,h);
	if(!old)
		return NULL;
	h->array[index]=l_slist_remove(h->array[index],old);
	h->count--;
	return old;
}

int l_hash_table_size(LHashTable *h)
{
#if 0
	int count=0;
	int i;
	for(i=0;i<h->size;i++)
	{
		count+=l_slist_length(h->array[i]);
	}
	return count;
#else
	return h->count;
#endif
}

void l_hash_iter_init(LHashIter *iter,LHashTable *h)
{
	iter->h=h;
	iter->item=NULL;
	iter->index=-1;
	iter->next=NULL;
}

int l_hash_iter_next(LHashIter *iter)
{
	LHashTable *h=iter->h;
	int i=iter->index;
	iter->item=iter->next;
	if(iter->item!=NULL)
	{
		iter->next=*(void**)iter->item;
		return 0;
	}
	for(i=i+1;i<h->size;i++)
	{
		if(!h->array[i])
			continue;
		iter->index=i;
		iter->item=h->array[i];
		iter->next=*(void**)iter->item;
		return 0;
	}
	iter->item=NULL;
	iter->next=NULL;
	return -1;
}

void *l_hash_iter_data(LHashIter *iter)
{
	return iter->item;
}
