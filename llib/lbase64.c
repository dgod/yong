#include "lmem.h"
#include "ltypes.h"

static const char b64_list[] = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void l_base64_encode(char *out, const uint8_t *in, int inlen)
{
	for (; inlen >= 3; inlen -= 3)
	{
		*out++ = b64_list[in[0] >> 2];
		*out++ = b64_list[((in[0] << 4) & 0x30) | (in[1] >> 4)];
		*out++ = b64_list[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
		*out++ = b64_list[in[2] & 0x3f];
		in += 3;
	}
	if (inlen > 0)
	{
		uint8_t fragment;
		*out++ = b64_list[in[0] >> 2];
		fragment = (in[0] << 4) & 0x30;
		if (inlen > 1)
		fragment |= in[1] >> 4;
		*out++ = b64_list[fragment];
		*out++ = (inlen < 2) ? '=' : b64_list[(in[1] << 2) & 0x3c];
		*out++ = '=';
	}
	*out = '\0';
}

#define XX 99
static const uint8_t b64_index[128] = {
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
	52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
	XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
	15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
	XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
	41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
};

#define DEC(c) (((c)<128)?b64_index[c]:XX)
int l_base64_decode(uint8_t *out, const char *in)
{
	int len = 0;
	uint8_t d1, d2, d3, d4;
	do {
		d1 = in[0];
		if(DEC(d1) == XX) return -1;
		d2 = in[1];
		if(DEC(d2) == XX) return -1;
		d3 = in[2];
		if (d3 != '=' && DEC(d3) == XX) return -1;
		d4 = in[3];
		if (d4 != '=' && DEC(d4) == XX) return -1;
		in += 4;
		if(out!=NULL) *out++ = (DEC(d1) << 2) | (DEC(d2) >> 4);
		++len;
		if (d3 != '=')
		{
			if(out!=NULL) *out++ = ((DEC(d2) << 4) & 0xf0) | (DEC(d3) >> 2);
			++len;
			 if (d4 != '=')
			 {
				if(out!=NULL) *out++ = ((DEC(d3) << 6) & 0xc0) | DEC(d4);
				++len;
			}
		}
	}while (*in && d4 != '=');
	return len;
}
