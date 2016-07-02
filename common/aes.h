#ifndef AES_H_
#define AES_H_
	
#include <stdint.h>

void aes_set_key(const uint8_t *cipher_key, int key_len);
int aes_encrypt(const uint8_t *plain_text, uint8_t *cipher_text);	//加密
int aes_decrypt(const uint8_t *cipher_text, uint8_t *plain_text);	//解密

#endif
