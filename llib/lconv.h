#ifndef _LCONV_H_
#define _LCONV_H_

char *l_gb_to_utf8(const char *s,char *out,int size);
void *l_gb_to_utf16(const char *s,void *out,int size);
char *l_utf8_to_gb(const char *s,char *out,int size);
void *l_utf8_to_utf16(const char *s,void *out,int size);
char *l_utf16_to_utf8(const void *s,char *out,int size);
char *l_utf16_to_gb(const void *s,char *out,int size);

#endif/*_LCONV_H_*/

