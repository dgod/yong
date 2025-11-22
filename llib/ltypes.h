#ifndef _LTYPES_H_
#define _LTYPES_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define extends(s) \
	union{ \
		s s;\
		s; \
	};

#ifndef offsetof
  #ifdef __GNUC__
    #define offsetof(type,field)  __builtin_offsetof (type, field)
  #else
    #define offsetof(type,field) ((size_t)&((type*)0)->field)
  #endif
#endif

#ifndef container_of
  #define container_of(ptr,type,field) ((type*)((char*)ptr-offsetof(type,field)))
#endif

#ifndef countof
	#if __STDC_VERSION>=202311L
		#define countof(arr) _Countof(arr)
	#else
		#define countof(arr) (sizeof (arr) / sizeof ((arr)[0]))
	#endif
#endif
	
#ifndef lengthof
	#define lengthof(arr) countof(arr)
#endif

#define L_ARRAY_SIZE(arr) lengthof(arr)

typedef void (*LEnumFunc)(void *data,void *user);
typedef void (*LFreeFunc)(void *data);
typedef int (*LCmpFunc)(const void *p1,const void *p2);
typedef int (*LCmpDataFunc)(const void *p1,const void *p2,void *user);
typedef unsigned (*LHashFunc)(const void *key);
typedef LCmpFunc LEqualFunc;

enum{
	L_TYPE_VOID=0,
	L_TYPE_CHAR,
	L_TYPE_INT,
	L_TYPE_FLOAT,
	L_TYPE_POINTER,
	L_TYPE_OP,
};

typedef struct{
	int type;
	union{
		int v_char;
		long v_int;
		double v_float;
		void *v_pointer;
		int v_op;
	};
}LVariant;

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#define LPTR_TO_INT(p) ((int)(size_t)(p))
#define LINT_TO_PTR(i) ((void*)(size_t)(i))

#define L_LITTLE_ENDIAN		1234
#define L_BIG_ENDIAN		4321

#define L_BYTE_ORDER		L_LITTLE_ENDIAN

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__loongarch64)
  #define L_WORD_SIZE			64
#else
  #define L_WORD_SIZE			32
#endif

#ifndef L_ALIGNED_ACCESS
	#define L_ALIGNED_ACCESS	0
#endif

#endif/*_LTYPES_H_*/

