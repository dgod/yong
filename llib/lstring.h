#ifndef _LSTRING_H_
#define _LSTRING_H_

char *l_sprintf(const char *fmt,...);
char **l_strsplit(const char *str,int delimiter);
char *l_strjoinv(const char *sep,char **list);
void l_strfreev(char **list);
int l_strv_length(char **list);
bool l_str_has_prefix(const char *str,const char *prefix);
bool l_str_has_suffix(const char *str,const char *suffix);
#define l_str_has_surround(str,prefix,suffix)	(l_str_has_prefix((str),(prefix)) && l_str_has_suffix((str),(suffix)))
char *l_str_trim_left(char *str);
char *l_str_trim_right(char *str);
char *l_str_trim(char *str);

typedef struct{
	char *str;
	int len;
	int size;
}LString;

LString *l_string_new(int size);
void l_string_free(LString *string);
void l_string_append(LString *string,const char *val,int len);
void l_string_append_c(LString *string,int c);

void l_strup(char *s);
void l_strdown(char *s);

int l_vsscanf(const char * buf, const char * fmt, va_list args);
int l_sscanf(const char * buf, const char * fmt, ...);

int l_strcpy(char *dest,int dest_size,const char *src);
void *l_strncpy(char *restrict dest,const char *restrict src,size_t n);

void *l_memmem(const void *haystack,int haystacklen,const void *needle,int needlelen);
int l_mempos(const void *haystack,int haystacklen,const void *needle,int needlelen);
int l_strpos(const char *haystack,const char *needle);

#endif/*_LSTRING_H_*/

