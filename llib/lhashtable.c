#include "ltypes.h"
#include "lslist.h"
#include "lmem.h"
#include "lhashtable.h"

struct _lhashtable{
	int size;
	LHashFunc hash;
	LCmpFunc cmp;
	void *array[];
};

LHashTable *l_hash_table_new(int size,LHashFunc hash,LCmpFunc cmp)
{
	LHashTable *h;
	h=l_alloc0(sizeof(*h)+sizeof(void*)*size);
	h->size=size;
	h->hash=hash;
	h->cmp=cmp;
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
	l_free(h);
}

void *l_hash_table_find(LHashTable *h,void *item)
{
	int index=h->hash(item)%h->size;
	return l_slist_find(h->array[index],item,h->cmp);
}

bool l_hash_table_insert(LHashTable *h,void *item)
{
	int index=h->hash(item)%h->size;	
	void *old=l_slist_find(h->array[index],item,h->cmp);
	if(old) return false;
	h->array[index]=l_slist_prepend(h->array[index],item);
	return true;
}

void *l_hash_table_replace(LHashTable *h,void *item)
{
	int index=h->hash(item)%h->size;	
	void *old=l_slist_find(h->array[index],item,h->cmp);
	if(old)
	{
		h->array[index]=l_slist_remove(h->array[index],old);
	}
	h->array[index]=l_slist_prepend(h->array[index],item);
	return old;
}

void l_hash_table_remove(LHashTable *h,void *item)
{
	int index=h->hash(item)%h->size;
	h->array[index]=l_slist_remove(h->array[index],item);
}

int l_hash_table_size(LHashTable *h)
{
	int count=0;
	int i;
	for(i=0;i<h->size;i++)
	{
		count+=l_slist_length(h->array[i]);
	}
	return count;
}

void l_hash_iter_init(LHashIter *iter,LHashTable *h)
{
	iter->h=h;
	iter->item=0;
	iter->index=-1;
	iter->next=0;
}

int l_hash_iter_next(LHashIter *iter)
{
	LHashTable *h=iter->h;
	int i=iter->index;
	iter->item=iter->next;
	if(iter->item)
	{
		iter->next=*(void**)iter->item;
		return 0;
	}
	for(i=i+1;i<h->size;i++)
	{
		if(!h->array[i]) continue;
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
