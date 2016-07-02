#ifndef _LSTRING_H_
#define _LSTRING_H_

char *l_sprintf(const char *fmt,...);
char **l_strsplit(const char *str,int delimiter);
char *l_strjoinv(const char *sep,char **list);
void l_strfreev(char **list);
int l_strv_length(char **list);
unsigned l_str_hash (const void *v);
bool l_str_has_prefix(const char *str,const char *prefix);
bool l_str_has_suffix(const char *str,const char *suffix);
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

int l_vsscanf(const char * buf, const char * fmt, va_list args);
int l_sscanf(const char * buf, const char * fmt, ...);

#endif/*_LSTRING_H_*/
