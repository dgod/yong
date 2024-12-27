#pragma once

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdalign.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "lconfig.h"
#include "ltypes.h"
#include "lmacros.h"
#include "lmem.h"
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
#include "ldlfcn.h"
#include "ltricky.h"
