#ifndef _PYZIP_H_
#define _PYZIP_H_

void cp_zip(const char *in,char *out);
void cp_zip2(const char *base,int blen,const char *in,char *out,int prefix);
int cp_unzip(const char *in,char *out);

void cz_zip(const char *in,char *out);
int cz_unzip(const char *in,char *out,int count);

#endif/*_PYZIP_H_*/
