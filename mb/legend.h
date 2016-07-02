#ifndef _LEGEND_H_
#define _LEGEND_H_

int y_legend_get(void *handle,const char *src,int slen,
                int dlen,char calc[][MAX_CAND_LEN+1],int max);
void y_legend_move(void *handle,const char *phrase);
void y_legend_free(void *handle);
void *y_legend_new(const char *file,int save);

#endif/*_LEGEND_H_*/
