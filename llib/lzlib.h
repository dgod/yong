#pragma once


int l_zlib_decode(void *obuffer, int olen, const void *ibuffer, int ilen, int header);
void *l_zlib_decode_alloc(const void *buffer, int len, int *outlen, int header);
