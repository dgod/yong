#pragma once

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdalign.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include <stdatomic.h>

#if __has_include(<stddefer.h>)
	#include <stddefer.h>
#elif __GNUC__ > 8
	#define defer _Defer
	#define _Defer      _Defer_A(__COUNTER__)
	#define _Defer_A(N) _Defer_B(N)
	#define _Defer_B(N) _Defer_C(_Defer_func_ ## N, _Defer_var_ ## N)
	#define _Defer_C(F, V)                                              \
		auto void F(int*);                                              \
		__attribute__((__cleanup__(F), __deprecated__, __unused__))     \
		int V;                                                          \
		__attribute__((__always_inline__, __deprecated__, __unused__))  \
		inline auto void F(__attribute__((__unused__)) int*V)
#else
#endif

#include "lconfig.h"
#include "ltypes.h"
#include "lmacros.h"
#include "lmem.h"
#include "lmman.h"
#include "lslist.h"
#include "llist.h"
#include "lkeyfile.h"
#include "lhashtable.h"
#include "lstring.h"
#include "larray.h"
#include "lconv.h"
#include "lunicode.h"
#include "lgb.h"
#include "lfile.h"
#include "lexpr.h"
#include "lqueue.h"
#include "lsearch.h"
#include "lqsort.h"
#include "lbase64.h"
#include "lxml.h"
#include "lzlib.h"
#include "lbits.h"
#include "lenv.h"
#include "lescape.h"
#include "ltime.h"
#include "lfuncs.h"
#include "lthreads.h"
#include "lcoroutine.h"
#include "lprocess.h"
#include "ldlfcn.h"
#include "ltricky.h"
#include "lre.h"
