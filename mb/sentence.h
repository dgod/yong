#pragma once

int sentence_init(void);
void sentence_save(void);
void sentence_destroy(void);
CSET_GROUP *sentence_get(const char *code,struct y_mb *mb,char super);
int sentence_add(const char *code,const char *cand);
int sentence_del(const char *cand);

