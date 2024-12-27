#ifndef _MD5_H_
#define _MD5_H_

#include <stdint.h>

typedef struct{
	uint32_t i[2];
	uint32_t buf[4];
	uint8_t in[64];
	uint8_t digest[16];
}MD5_CTX;

void l_md5_init(MD5_CTX *);
void l_md5_update(MD5_CTX *,const uint8_t *,unsigned);
void l_md5_final(MD5_CTX *ctx);

#endif /*_MD5_H_*/
