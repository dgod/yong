#ifndef _TRANSLATE_H_
#define _TRANSLATE_H_

void y_translate_init(const char *config);
const char *y_translate_get(const char *s);
int y_translate_is_enable(void);

#define YT(s) ((char*)y_translate_get(s))

#endif/*_TRANSLATE_H_*/
