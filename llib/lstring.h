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
int l_str_replace(char *s,int from,int to);
bool l_str_is_ascii(const char *s);

typedef struct{
	char *str;
	int len;
	int size;
}LString;

#define L_STRING_INIT (LString){.size=0}

LString *l_string_new(int size);
void l_string_init(LString *string,int size);
void l_string_free(LString *string);
void l_string_expand(LString *string,int len);
void l_string_append(LString *string,const char *val,int len);
void l_string_append_c(LString *string,int c);
char *l_string_steal(LString *string);
void l_string_clear(LString *string);
void l_string_erase(LString *string,int pos,int len);

void l_strup(char *s);
void l_strdown(char *s);

int l_vsscanf(const char * buf, const char * fmt, va_list args);
int l_sscanf(const char * buf, const char * fmt, ...);

int l_strcpy(char *dest,int dest_size,const char *src);
void *l_strncpy(char *restrict dest,const char *restrict src,size_t n);

#if defined(_WIN32) || !defined(__GLIBC__)
char *l_stpcpy(char *dest,const char *src);
#else
#define l_stpcpy(dest,src) stpcpy((dest),(src))
#endif

void *l_memmem(const void *haystack,int haystacklen,const void *needle,int needlelen);
int l_mempos(const void *haystack,int haystacklen,const void *needle,int needlelen);
int l_strpos(const char *haystack,const char *needle);
int l_chrpos(const char *s,int c);
int l_chrnpos(const char *s,int c,size_t n);

#define L_INT2STR_HZ			0x01
#define L_INT2STR_UTF8			0x03
#define L_INT2STR_ZERO0			0x04
#define L_INT2STR_BIG			0x08
#define L_INT2STR_INDIRECT		0x11
#define L_INT2STR_TRAD			0x20
#define L_INT2STR_KEEP10		0x40
#define L_INT2STR_MINSEC		0x80
#define L_INT2STR_NEST			0x100
int l_int_to_str(int64_t n,const char *format,int flags,char *out);

int l_strtok(const char *str,int delim,const char *res[],int limit);
int l_strtok0(char *str,int delim,char *res[],int limit);

#endif/*_LSTRING_H_*/

