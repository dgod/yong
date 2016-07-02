#ifndef _LGB_H_
#define _LGB_H_

int l_unichar_to_gb(uint32_t c,uint8_t *outbuf);
uint32_t l_gb_to_unichar(const uint8_t *s);
const uint8_t *l_gb_next_char(const uint8_t *s);

#endif/*_LGB_H_*/

