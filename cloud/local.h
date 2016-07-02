#ifndef _LOCAL_H_
#define _LOCAL_H_

void local_load_pinyin(const char *fn);
void local_load_assist(const char *fn,int pos);
void local_free_all(void);
bool local_assist_match(const char *p,int c);
bool local_is_assist_key(int key);
const char *local_pinyin_get(const char *pinyin);

void local_load_user(const char *fn);
const void *local_phrase_set(const char *pinyin);
int local_phrase_count(const void *phrase);
int local_phrase_get(const void *phrase,int at,int num,
	char cand[][MAX_CAND_LEN+1]);
#endif/*_LOCAL_H_*/
