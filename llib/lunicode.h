#ifndef _LUNICODE_H_
#define _LUNICODE_H_

int l_unichar_to_utf8 (uint32_t c,uint8_t *outbuf);
uint32_t l_utf8_to_unichar(const uint8_t *s);
uint8_t *l_utf8_strncpy(uint8_t *dst,const uint8_t *src,size_t n);
const uint8_t *l_utf8_offset(const uint8_t *s,int offset);
int l_unichar_to_utf16 (uint32_t c,uint16_t *outbuf);
uint32_t l_utf16_to_unichar(const uint16_t *s);
const uint8_t *l_utf8_next_char(const uint8_t *s);
const uint16_t *l_utf16_next_char(const uint16_t *s);

#endif/*_LUNICODE_H_*/
