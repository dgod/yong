#ifndef _ASSOC_H_
#define _ASSOC_H_

int y_assoc_get(void *handle,const char *src,int slen,
                int dlen,char calc[][MAX_CAND_LEN+1],int max);
void y_assoc_move(void *handle,const char *phrase);
void y_assoc_free(void *handle);
void *y_assoc_new(const char *file,int save);

#endif/*_ASSOC_H_*/
