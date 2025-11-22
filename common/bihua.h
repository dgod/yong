/*!
 * \author dgod
 */
#ifndef _BIHUA_H_
#define _BIHUA_H_

/* note that we don't support gbk and above at ->name */
typedef struct{
	char *name;
	char *code;
}Y_BIHUA_INFO;

#define Y_BIHUA_KEY		"abcdefghijklmnopqrstuvwxyz;',./"

void *y_bihua_eim(void);
int y_bihua_good(void);

#endif/*_BIHUA_H_*/
