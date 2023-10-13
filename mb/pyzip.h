#ifndef _PYZIP_H_
#define _PYZIP_H_

int cp_zip(const char *in,char *out);
int cp_unzip(const char *in,char *out,int size);
int cp_unzip_size(const char *in,int size);
int cp_unzip_py(const char *in,char *out,int size);

int cz_zip(const char *in,char *out);
int cz_unzip(const char *in,char *out,int size);
int cz_gblen(const char *in,int size);

#endif/*_PYZIP_H_*/
