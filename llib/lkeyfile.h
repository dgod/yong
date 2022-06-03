#ifndef _LKEYFILE_H_
#define _LKEYFILE_H_

struct _lkeyfile;
typedef struct _lkeyfile LKeyFile;

LKeyFile *l_key_file_open(const char *file,int create,...);
LKeyFile *l_key_file_load(const char *data,ssize_t length);
int l_key_file_save(LKeyFile *key_file,const char *path);
void l_key_file_free(LKeyFile *key_file);
const char *l_key_file_get_data(LKeyFile *key_file,const char *group,const char *key);
char *l_key_file_get_string(LKeyFile *key_file,const char *group,const char *key);
int l_key_file_get_int(LKeyFile *key_file,const char *group,const char *key);
int l_key_file_set_data(LKeyFile *key_file,const char *group,const char *key,const char *value);
int l_key_file_set_string(LKeyFile *key_file,const char *group,const char *key,const char *value);
int l_key_file_set_int(LKeyFile *key_file,const char *group,const char *key,int value);
void l_key_file_set_dirty(LKeyFile *key_file);
const char *l_key_file_get_start_group(LKeyFile *key_file);
bool l_key_file_has_group(LKeyFile *key_file,const char *group);
char **l_key_file_get_groups(LKeyFile *key_file);
char **l_key_file_get_keys(LKeyFile *key_file,const char *group);

#define l_key_file_remove_group(key_file,group) \
	l_key_file_set_data((key_file),(group),NULL,NULL)

#define l_key_file_remove_key(key_file,group,key) \
	l_key_file_set_data((key_file),(group),(key),NULL)

#endif/*_LKEYFILE_H_*/
