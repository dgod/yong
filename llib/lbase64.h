#pragma once

void l_base64_encode(char *out, const uint8_t *in, int inlen);
int l_base64_decode(uint8_t *out, const char *in);
