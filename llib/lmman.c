#include "llib.h"
#include "lmman.h"

#ifdef __linux__
#include <sys/mman.h>
#endif

#ifdef L_MEM_DEBUG
#define MEM_ASSERT(expr) assert(expr)
#else
#define MEM_ASSERT(expr) 
#endif

#define CHECK_PAGE_BEGIN		(uint8_t*)0x01000000				// 16MB
#define CHECK_PAGE_END			(uint8_t*)0x80000000				// 2GB
#define CHECK_PAGE_STEP			0x01000000							// 16MB

#define PAGE_CACHE_MAX			8									// 1-64
#define PAGE_COUNT_MAX			8192
#define PAGE_MASK_COUNT			(PAGE_COUNT_MAX/64)

#if defined(_WIN64)
static PVOID (WINAPI *p_VirtualAlloc2)(HANDLE Process, PVOID BaseAddress, SIZE_T Size, ULONG AllocationType, ULONG PageProtection, MEM_EXTENDED_PARAMETER* ExtendedParameters, ULONG ParameterCount);
#endif

#if L_USE_PTR32
typedef struct{
	uint32_t prev:14;
	uint32_t size:14;
	uint32_t tail:1;
	uint32_t used:1;
	uint32_t resv:2;
	union{
		// idle list link node, only exists when used==0
		struct{
			l_cptr_t lnext;
			l_cptr_t lprev;
		};
		// used data user memory block
		uint8_t data[8];
	};
}MEM_INFO;
static_assert(sizeof(MEM_INFO)==12,"MEM_INFO size bad");
#endif // L_USE_PTR32

typedef struct{
	// range of first alloc page
	uint8_t *base,*end;
	// usage of first alloc page, 0: used 1: free
	void *page_cache;
#if L_USE_PTR32
	// linked list of free memory
	MEM_INFO *head;
	uint8_t *unused;
#endif // L_USE_PTR32
	int32_t page_cache_count;
	int32_t page_committed;
	uint64_t mask[PAGE_COUNT_MAX];
}HEAP;
static HEAP *heap;

static bool first_page_init(void)
{
	const int count=PAGE_COUNT_MAX;
	memset(heap->mask,0xff,PAGE_MASK_COUNT*sizeof(uint64_t));
		
	const int TOTAL_SIZE=count*L_PAGE_SIZE;

#if L_WORD_SIZE==32
	#ifdef _WIN32
	{
		heap->base=VirtualAlloc(NULL,TOTAL_SIZE,MEM_RESERVE,PAGE_NOACCESS);
		MEM_ASSERT(heap->base!=NULL);
	}
	#else
	{
		void *p=mmap(NULL,L_PAGE_SIZE*count,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
		if(p!=MAP_FAILED)
			heap->base=p;
	}
	#endif
#elif defined(__linux__) && defined(__x86_64__)
	{
		void *p=mmap(NULL,TOTAL_SIZE,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
		if(p!=MAP_FAILED)
			heap->base=p;
	}
#else // L_WORD_SIZE==64
	#if defined(_WIN64)
	if(p_VirtualAlloc2)
	{
		MEM_ASSERT(p_VirtualAlloc2!=NULL);
		MEM_ADDRESS_REQUIREMENTS addressReqs = {0};
		addressReqs.HighestEndingAddress=CHECK_PAGE_END-L_PAGE_SIZE-1;
		MEM_EXTENDED_PARAMETER param = {0};
		param.Type=MemExtendedParameterAddressRequirements;
		param.Pointer=&addressReqs;
		heap->base=p_VirtualAlloc2(NULL,NULL,TOTAL_SIZE,MEM_RESERVE,PAGE_NOACCESS,&param,1);
		MEM_ASSERT(heap->base!=NULL);
	}
	else
	{
		uint8_t *p=CHECK_PAGE_BEGIN;
		while(p<CHECK_PAGE_END-TOTAL_SIZE)
		{
			uint8_t *ret=VirtualAlloc(p,TOTAL_SIZE,MEM_RESERVE,PAGE_NOACCESS);
			if(ret==NULL)
			{
				p+=CHECK_PAGE_STEP;
				continue;
			}
			heap->base=p;
			heap->end=p+TOTAL_SIZE;
			break;
		}
		MEM_ASSERT(heap->base!=NULL);
	}
	#else
	p=CHECK_PAGE_BEGIN;
	while(p<CHECK_PAGE_END-TOTAL_SIZE)
	{
		uint8_t *ret=mmap(p,TOTAL_SIZE,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
		if(ret==(uint8_t*)MAP_FAILED)
		{
			p+=CHECK_PAGE_STEP;
			continue;
		}
		if(ret>CHECK_PAGE_END-TOTAL_SIZE)
		{
			p+=CHECK_PAGE_STEP;
			munmap(ret,TOTAL_SIZE);
			continue;
		}
		heap->base=ret;
		break;
	}
	MEM_ASSERT(heap->base!=NULL);
	#endif // _WIN64
#endif // L_WORD_SIZE
	if(heap->base==NULL)
	{
		return false;
	}
	heap->end=heap->base+TOTAL_SIZE;
#ifdef _WIN32
	uint8_t *p=VirtualAlloc(heap->base,PAGE_CACHE_MAX*L_PAGE_SIZE,MEM_COMMIT,PAGE_READWRITE);
	assert(p!=NULL);
#else
	int ret=mprotect(heap->base, PAGE_CACHE_MAX*L_PAGE_SIZE, PROT_READ | PROT_WRITE);
	assert(ret==0);
#endif

	heap->mask[0]&=~((1<<PAGE_CACHE_MAX)-1);
	for(int i=0;i<PAGE_CACHE_MAX;i++)
	{
		heap->page_cache=l_slist_prepend(heap->page_cache,heap->base+L_PAGE_SIZE*i);
	}
	heap->page_cache_count=PAGE_CACHE_MAX;
	heap->page_committed=PAGE_CACHE_MAX;

	return true;
}

static bool heap_init(void)
{
#if defined(_WIN64)
	p_VirtualAlloc2=l_defsym("KernelBase.dll","VirtualAlloc2");
#endif
	heap=l_alloc0(sizeof(HEAP));
	if(!heap) return false;
	if(!first_page_init())
	{
		l_free(heap);
		heap=NULL;
		return false;
	}
	return true;
}

void *l_alloc_page(void)
{
	if(!heap)
	{
		if(!heap_init())
			return NULL;
	}
	if(heap->page_cache_count)
	{
		MEM_ASSERT(heap->page_cache!=NULL);
		void *ret=heap->page_cache;
		heap->page_cache=*(void**)ret;
		heap->page_cache_count--;
		return ret;
	}
	for(int i=0;i<PAGE_MASK_COUNT;i++)
	{
		if(heap->mask[i]==0)
			continue;
#ifdef _MSC_VER
		DWORD pos;
		_BitScanForward64(&pos, heap->mask[i]);
#else
		int pos=__builtin_ffsll(heap->mask[i])-1;
#endif
		uint8_t *p=heap->base+L_PAGE_SIZE*(i*64+pos);
#ifdef _WIN32
		void *ret=VirtualAlloc(p,L_PAGE_SIZE,MEM_COMMIT,PAGE_READWRITE);
		if(!ret)
			return NULL;
#else
		int ret=mprotect(p, L_PAGE_SIZE, PROT_READ | PROT_WRITE);
		if(ret!=0)
			return NULL;
#endif
		heap->mask[i]&=heap->mask[i]-1;
		heap->page_committed++;
		return p;
	}
#if !L_USE_PTR32
	#ifdef _WIN32
	return VirtualAlloc(NULL,L_PAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
	#else
	void *p=mmap(NULL,L_PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
	if(p==MAP_FAILED)
		return NULL;
	return p;
	#endif
#else
#ifdef _WIN64
	if(p_VirtualAlloc2)
	{
		MEM_ADDRESS_REQUIREMENTS addressReqs = {
			.HighestEndingAddress=CHECK_PAGE_END-L_PAGE_SIZE-1
		};
		MEM_EXTENDED_PARAMETER param = {0};
		param.Type=MemExtendedParameterAddressRequirements;
		param.Pointer=&addressReqs;
		uint8_t *ret=p_VirtualAlloc2(NULL,NULL,L_PAGE_SIZE,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE,&param,1);
		if(!ret)
			return NULL;
		MEM_ASSERT(ret<=CHECK_PAGE_END-L_PAGE_SIZE);
		heap->page_committed++;
		return ret;
	}
	return NULL;
#else
	#if defined(__linux__) && defined(__x86_64__)
	void *ret=mmap(NULL,L_PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
	if(ret==MAP_FAILED)
		return NULL;
	heap->page_committed++;
	return ret;
	#else
	return NULL;
	#endif
#endif
#endif
}

void l_free_page(void *p)
{
	MEM_ASSERT(heap!=NULL);
	MEM_ASSERT((((uintptr_t)p)&(4096-1))==0);
#if L_USE_PTR32
	MEM_ASSERT((uint8_t*)p<CHECK_PAGE_END);
#endif
	if(heap->page_cache_count<PAGE_CACHE_MAX)
	{
		heap->page_cache=l_slist_prepend(heap->page_cache,p);
		heap->page_cache_count++;
		return;
	}
	if((uint8_t*)p>=heap->base && (uint8_t*)p<heap->end)
	{
		int pos=((uint8_t*)p-heap->base)/L_PAGE_SIZE;
		heap->mask[pos/64]|=1ULL<<(pos%64);
#ifdef _WIN32
		VirtualFree(p,L_PAGE_SIZE,MEM_DECOMMIT);
#else
		madvise(p, L_PAGE_SIZE, MADV_DONTNEED);
		mprotect(p, L_PAGE_SIZE, PROT_NONE);
#endif
		heap->page_committed--;
		return;
	}
#ifdef _WIN32
	VirtualFree(p,0,MEM_RELEASE);
#else
	munmap(p, L_PAGE_SIZE);
#endif
	heap->page_committed--;
}



