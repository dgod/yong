#pragma once

#define L_ESCAPE_NEXT				0x01
#define L_ESCAPE_GB					0x02
#define L_ESCAPE_LAST				0x04
#define L_ESCAPE_HEX				((char)-1)

typedef struct{
	char from;
	char to;
}L_ESCAPE_ITEM;

typedef struct lescapeconfig{
	char lead;
	uint8_t flags;
	uint8_t count;
	char sep;
	char env[4];
	char surround[2];
	L_ESCAPE_ITEM map[];
}L_ESCAPE_CONFIG;

void *l_unescape(const void *in, char *out, int size, const L_ESCAPE_CONFIG *config);
char **l_unescape_array(const void *in, const L_ESCAPE_CONFIG *config);
int l_escape(const void *in, char *out, int size, const L_ESCAPE_CONFIG *config);

int encodeURIComponent(const char *in,char *out,int size);

