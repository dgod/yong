#pragma once

typedef struct{
	char code[8];			// 目标编码
	char force;				// 目标不存在时，是否强制添加
}FUZZY_TO;

typedef struct{
	void *next;
	char from[8];			// 要模糊的编码
	FUZZY_TO to[4];			// 模糊目标
	void *begin;			// 要模糊的字列表的开始
	void *end;				// 要模糊的字列表的结束
}FUZZY_ITEM;

typedef LHashTable FUZZY_TABLE;

FUZZY_TABLE *fuzzy_table_load(const char *file);
void fuzzy_table_free(FUZZY_TABLE *ft);
FUZZY_ITEM *fuzzy_table_lookup(FUZZY_TABLE *ft,const char *code);
LArray *fuzzy_key_list(FUZZY_TABLE *ft,const char *code,int len,int split);
