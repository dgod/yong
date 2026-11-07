#include "llib.h"
#include "bloom.h"

BloomFilter *bloom_new(uint32_t size)
{
	int bytes=(size+7)/8;
	BloomFilter *bf=l_alloc0(sizeof(BloomFilter)+bytes);
	if(bf)
		bf->size=bytes*8;
	for(int i=0;i<countof(bf->cache);i++)
	{
		BloomCacheItem *it=l_new(BloomCacheItem);
		bf->idle=l_slist_prepend(bf->idle,it);
	}
	return bf;
}

void bloom_add(BloomFilter *bf,const char *s,int len)
{
	unsigned h=l_str_hash2(s,len);
	int pos=h%bf->size;
#ifdef BLOOM_DEBUG
	bf->count++;
	if(l_bitmap_get(bf->bits,pos))
		bf->conflict++;
#endif
	l_bitmap_set(bf->bits,pos);
}

static inline BloomCacheItem *bloom_cache_get(BloomFilter *bf,uint32_t hash,const char *s,int len)
{
	uint32_t i=hash%BLOOM_CACHE_SIZE;
	BloomCacheItem *prev=NULL;
	for(BloomCacheItem *p=bf->cache[i];p!=NULL;prev=p,p=p->next)
	{
		if(p->ksize!=len)
			continue;
		if(memcmp(s,p->key,len)==0)
		{
			if(prev!=NULL)
			{
				prev->next=p->next;
				p->next=bf->cache[i];
				bf->cache[i]=p;
			}
			return p;
		}
	}
	return NULL;
	
}

bool bloom_check(BloomFilter *bf,const char *s,int len,BloomCacheItem **item)
{
	uint32_t h=l_str_hash2(s,len);
	int pos=h%bf->size;
	bool ret=l_bitmap_get(bf->bits,pos);
	if(ret)
	{
		*item=bloom_cache_get(bf,h,s,len);
		if(!*item)
		{
			bf->last_key=s;
			bf->last_ksize=len;
			bf->last_hash=h;
		}
	}
	return ret;
}

void bloom_free(BloomFilter *bf)
{
	if(!bf)
		return;
	for(int i=0;i<countof(bf->cache);i++)
		l_slist_free(bf->cache[i],l_free);
	l_slist_free(bf->idle,l_free);
	l_free(bf);
}

void bloom_cache_add(BloomFilter *bf,void *data)
{
	BloomCacheItem *p=bf->idle;
	if(p)
		bf->idle=p->next;
	else
		p=l_new(BloomCacheItem);
	p->data=data;
	p->ksize=bf->last_ksize;
	memcpy(p->key,bf->last_key,p->ksize);
	int i=bf->last_hash%BLOOM_CACHE_SIZE;
	bf->cache[i]=l_slist_prepend(bf->cache[i],p);
	bf->cached++;
}

void bloom_cache_clear(BloomFilter *bf)
{
	if(!bf)
		return;
	if(bf->cached<=BLOOM_CACHE_SIZE)
		return;
	for(int i=0;i<countof(bf->cache);i++)
	{
		BloomCacheItem *h=bf->cache[i];
		if(!h) continue;
		while(h->next)
		{
			BloomCacheItem *p=h->next;
			h->next=p->next;
			bf->idle=l_slist_prepend(bf->idle,p);
			bf->cached--;
		}
	}
}

