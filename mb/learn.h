#ifndef _LEARN_H_
#define _LEARN_H_

void *y_mb_learn_load(struct y_mb *mb,char *in);
void y_mb_learn_free(void *data);
int y_mb_predict_by_learn(struct y_mb *mb,char *s,int caret,char *out,int size,int begin);
const char *y_mb_predict_nth(const char *s,int n);

extern int l_predict_simple;
extern int l_predict_sp;
extern int l_predict_simple_mode;

#endif/*_LEARN_H_*/
