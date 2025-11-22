#ifndef _LFILE_H_
#define _LFILE_H_

#include <time.h>

FILE *l_file_vopen(const char *file,const char *mode,va_list ap,size_t *size);
FILE *l_file_open(const char *file,const char *mode,...);
char *l_file_vget_contents(const char *file,size_t *length,va_list ap);
char *l_file_get_contents(const char *file,size_t *length,...);
int l_file_set_contents(const char *file,const void *contents,size_t length,...);

struct _ldir;
typedef struct _ldir LDir;

LDir *l_dir_open(const char *path);
void l_dir_close(LDir *dir);
const char *l_dir_read_name(LDir *dir);
int l_mkdir(const char *name,int mode);
int l_rmdir(const char *name);
char **l_readdir(const char *path);
int l_remove(const char *name);

bool l_file_is_dir(const char *path);
int l_access(const char *path,int mode);
bool l_file_exists(const char *path);
time_t l_file_mtime(const char *path);
ssize_t l_file_size(const char *path);
ssize_t l_filep_size(FILE *fp);
int l_file_touch(const char *path,int64_t mtime);

int l_file_copy(const char *dst,const char *src,...);

int l_get_line(char *line, size_t n, FILE *fp);

char *l_fullpath(char *abs,const char *rel,size_t size);
char *l_getcwd(void);
char *l_path_resolve(const char *path);

#endif/*_LFILE_H_*/
