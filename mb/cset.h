#pragma once

#include "yong.h"
#include "mb.h"
#include "learn.h"

enum{
	CSET_TYPE_NONE=0,
	CSET_TYPE_CALC,
	CSET_TYPE_ARRAY,
	CSET_TYPE_PREDICT,
	CSET_TYPE_MB,
	CSET_TYPE_EXTRA_ZI,
	CSET_TYPE_SENTENCE,
	CSET_TYPE_UNKNOWN,
};

typedef struct _cset_group{
	struct _cset_group *next;
	int type;
	int count;
	int offset;
	int (*get)(struct _cset_group *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1]);
	void (*free)(struct _cset_group *g);
}CSET_GROUP;

typedef struct _cset_group_calc{
	CSET_GROUP;
	int size;
	char (*phrase)[MAX_CAND_LEN+1];
}CSET_GROUP_CALC;

typedef struct _cset_group_predict{
	CSET_GROUP;
	char phrase[MAX_CAND_LEN+1];
}CSET_GROUP_PREDICT;

typedef struct _cset_group_mb{
	CSET_GROUP;
	struct y_mb *mb;
}CSET_GROUP_MB;

typedef struct{
	char *cand;
	char *codetip;
	int index;
}CSET_GROUP_ARRAY_ITEM;

typedef struct _cset_group_array{
	CSET_GROUP;
	LArray *array;
}CSET_GROUP_ARRAY;

typedef struct _cset{
	CSET_GROUP *list;

	CSET_GROUP_CALC calc;
	CSET_GROUP_PREDICT predict;
	CSET_GROUP_MB mb;
	CSET_GROUP_ARRAY array;

	LHashTable *assoc;
	short assoc_adjust;
	short assoc_adjust_add;
}CSET;

void cset_init(CSET *cs);
void cset_reset(CSET *cs);
void cset_destroy(CSET *cs);
void cset_append(CSET *cs,CSET_GROUP *g);
void cset_prepend(CSET *cs,CSET_GROUP *g);
void cset_remove(CSET *cs,CSET_GROUP *g);
int cset_count(CSET *cs);
int cset_output(CSET *cs,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1]);
void *cset_get_group_by_type(CSET *cs,int type);
void cset_clear(CSET *cs,int type);

void cset_group_free(CSET_GROUP *g);
int cset_group_offset(CSET_GROUP *g,int offset);
void cset_group_dump(CSET_GROUP *g,FILE *fp);

CSET_GROUP_PREDICT *cset_predict_group_new(CSET *cs);
int cset_predict_group_count(CSET *cs);

CSET_GROUP_CALC *cset_calc_group_new(CSET *cs);
int cset_calc_group_append(CSET_GROUP_CALC *g,const char *s);
int cset_calc_group_count(CSET *cs);

CSET_GROUP_MB *cset_mb_group_new(CSET *cs,struct y_mb *mb,int count);
void cset_mb_group_set(CSET *cs,struct y_mb *mb,int count);

CSET_GROUP_ARRAY *cset_array_group_new(CSET *cs);
int cset_array_group_append(CSET_GROUP_ARRAY *g,const char *cand,const char *codetip);

void cset_set_assoc(CSET *cs,char CalcPhrase[][MAX_CAND_LEN+1],int count);
int cset_has_assoc(CSET *cs,const char *code);
void cset_apply_assoc(CSET *cs);

