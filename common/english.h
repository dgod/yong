#ifndef _ENGLISH_H_
#define _ENGLISH_H_

#include "yong.h"
#include "ltypes.h"

typedef struct{
	int (*Set)(const char *s);
	int (*Get)(char cand[][MAX_CAND_LEN+1],int pos,int count);
	int64_t Priv1;
	uintptr_t Priv2;
	int Count;
}ENGLISH_IM;

void y_english_init(void);
void y_english_destroy(void);
void *y_english_eim(void);
void y_english_key_desc(const char *code,char *res);

#endif/*_ENGLISH_H_*/
