#ifndef _PYZIP_H_
#define _PYZIP_H_

int cp_zip(const char *restrict in,char *restrict out);
int cp_unzip(const char *restrict in,char *restrict out,int size);
int cp_unzip_size(const char *in,int size);
int cp_unzip_py(const char *restrict in,char *restrict out,int size);
int cp_unzip_jp(const char *restrict in,char *restrict out,int size);

int cz_zip(const char *in,char *out);
int cz_unzip(const char *restrict in,char *restrict out,int size);
int cz_gblen(const char *in,int size);

int bs_alloc_size(int len,int *method);
int bs_get_alloc_size(const uint8_t *p);
static inline int bs_get_len(const uint8_t *p)
{
	return p[0]&0x3f;
}
void bs_zip(const char *in,int len,uint8_t *out,int method,const uint8_t *map);
const uint8_t *bs_unzip(const uint8_t *in,uint8_t *out);

#endif/*_PYZIP_H_*/
