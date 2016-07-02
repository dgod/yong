#ifndef _LTRICKY_H_
#define _LTRICKY_H_

#ifdef __GLIBC__

#include "ltypes.h"

#if L_WORD_SIZE==32
__asm__(".symver __isoc99_sscanf,sscanf@GLIBC_2.0");
#else
__asm__(".symver __isoc99_sscanf,sscanf@GLIBC_2.2.5");
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
#endif

#endif/*__GLIBC__*/

#endif/*_LTRICKY_H_*/
