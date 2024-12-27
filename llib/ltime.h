#include <time.h>
#include <stdint.h>

#include "ltypes.h"

#ifdef _WIN32
#include <windows.h>
static inline uint64_t l_ticks(void)
{
#ifdef _WIN64
	return GetTickCount64();
#else
	return GetTickCount();
#endif
}
#else
static inline uint64_t l_ticks(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC,&t);
	return t.tv_sec*1000+t.tv_nsec/1000000;
}
#endif


#if L_WORD_SIZE==64 || !defined(_WIN32)
#define l_time() (int64_t)time(NULL)
#define l_localtime(timep) localtime((const time_t*)(timep))
#define l_mktime(tm) (int64_t)mktime(tm)
#else
#define l_time() (int64_t)_time64(NULL)
#define l_localtime(timep) _localtime64((const __time64_t*)(timep))
#define l_mktime(tm) (int64_t)_mktime64(tm)
#endif

