#ifndef _LGB_H_
#define _LGB_H_

int l_unichar_to_gb(uint32_t c,uint8_t *outbuf);
uint32_t l_gb_to_unichar(const uint8_t *s);
void *l_gb_next_char(const void *p);
int l_gb_strlen(const void *p,int size);
void *l_gb_offset(const void *p,int offset);
uint32_t l_gb_to_char(const void *p);
uint32_t l_gb_last_char(const void *p);
int l_char_to_gb(uint32_t c,void *outbuf);
const void *l_gb_strchr(const void *p,uint32_t c);

#endif/*_LGB_H_*/

