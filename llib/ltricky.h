#ifndef _LTRICKY_H_
#define _LTRICKY_H_

#if defined(__GLIBC__) && (defined(__i386__) || defined(__x86_64__))

#include "ltypes.h"

#if L_WORD_SIZE==32
__asm__(".symver __isoc99_sscanf,sscanf@GLIBC_2.0");
__asm__(".symver __isoc23_sscanf,sscanf@GLIBC_2.0");
__asm__(".symver __isoc99_vsscanf,vsscanf@GLIBC_2.0");
__asm__(".symver __isoc23_strtol,strtol@GLIBC_2.0");
__asm__(".symver __isoc23_strtoul,strtoul@GLIBC_2.0");
__asm__(".symver __isoc23_strtoll,strtoll@GLIBC_2.0");
#else
__asm__(".symver __isoc99_sscanf,sscanf@GLIBC_2.2.5");
__asm__(".symver __isoc23_sscanf,sscanf@GLIBC_2.2.5");
__asm__(".symver __isoc99_vsscanf,vsscanf@GLIBC_2.2.5");
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtol,strtol@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtoul,strtoul@GLIBC_2.2.5");
__asm__(".symver __isoc23_strtoll,strtoll@GLIBC_2.2.5");
#endif

#endif/*__GLIBC__*/

#endif/*_LTRICKY_H_*/
