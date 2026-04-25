#pragma once



#ifndef L_USE_PTR32
#define L_USE_PTR32	0
#endif

#if L_WORD_SIZE==32
typedef void * l_ptr32_t;
#undef L_USE_PTR32
#else
typedef uint32_t l_ptr32_t;
#endif

#if L_USE_PTR32
typedef l_ptr32_t l_cptr_t;
#else
typedef void *l_cptr_t;
#endif

#if L_WORD_SIZE==32 || !L_USE_PTR32
#define L_CPTR(_p)	(_p)
#define L_CPTR_NEXT(_p)	(*(void**)(_p))
#define L_CPTR_T(_p) (_p)
#define l_cptr_null NULL
#else
#define L_CPTR(_p)	_Generic((_p),l_cptr_t:(void*)(uintptr_t)(_p),default:(_p))
#define L_CPTR_NEXT(_p)	(void*)L_CPTR((*(l_cptr_t*)L_CPTR(_p)))
#define L_CPTR_T(_p) _Generic((_p),l_cptr_t:(_p),default:(l_cptr_t)(uintptr_t)(_p))
#define l_cptr_null 0
#endif

#define L_PAGE_SIZE	0x4000

void *l_alloc_page(void);
void l_free_page(void *p);

typedef struct{
	int page_first_alloc;
	int page_cache_count;
	int page_committed;
#if L_USE_PTR32
	struct{
		int blocks;
		int total_size;
		int min_size;
		int max_size;
	}idle_list;	
#endif
}L_CPTR_INFO;

// #define L_MEM_DEBUG
L_CPTR_INFO l_cptr_info(void);
bool l_cptr_leaked(void);

#if defined(L_MEM_DEBUG) && L_USE_PTR32
void l_cptr_assert(void *p);
#endif

#if L_USE_PTR32
void *l_cptr_alloc(size_t size);
void l_cptr_free(void *p);
#else
#define l_cptr_alloc(_size)	l_alloc(_size)
#define l_cptr_free(_p)	l_free(_p)
#endif
