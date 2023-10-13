#ifndef _LEARN_H_
#define _LEARN_H_

#include "cset.h"

#define PREDICT_SENTENCE	0
#define PREDICT_ASSIST		1
#define PREDICT_JP			2

struct learn_data;
typedef struct learn_data LEARN_DATA;

LEARN_DATA *y_mb_learn_load(struct y_mb *mb,const char *in);
void y_mb_learn_free(LEARN_DATA *data);
int y_mb_predict_by_learn(struct y_mb *mb,char *s,int caret,CSET_GROUP_PREDICT *g,int begin);
const char *y_mb_predict_nth(const char *s,int n);

extern int l_predict_simple;
extern int l_predict_sp;
extern int l_predict_simple_mode;

#endif/*_LEARN_H_*/
