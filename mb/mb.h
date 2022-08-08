#ifndef _MB_H_
#define _MB_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include "yong.h"
#include "trie.h"

#define Y_MB_KEY_SIZE		63
#define Y_MB_DATA_SIZE		255
#define Y_MB_DATA_CALC		32
#define Y_MB_WILDCARD		63
#define Y_MB_KEY_CP			((int)((sizeof(uintptr_t)<<3)/6))
#define Y_MB_KEY_MARK		0x01
#define Y_MB_DATA_CP		((int)(sizeof(uintptr_t)+sizeof(uint16_t)))

/* code for one zi */
struct y_mb_code{
	struct y_mb_code *next;
	union{
		struct{
			uint8_t virt:1;	/* only used to auto phrase */
			uint8_t main:1; /* this is from main mb */
			uint8_t len:6;
			uint8_t data[3];
		};
		uint32_t val;
	};
};

/* use hash to store zi for fast access */
struct y_mb_zi{
	struct y_mb_zi *next;
	/* main code */
	struct y_mb_code *code;
	/* just the hz val */
	uint32_t data;
};

/* use array to store rules */
struct y_mb_rule{
	void *next;
	/* is above n char? */
	unsigned char a;
	/* n char */
	unsigned char n;
 	struct {
		/* reverse order */
		unsigned char r:1;
		/* indicate p is not index, but just the real code */
		unsigned char d:1;
		/* index of zi */
		unsigned char i:6;
		/* index of code */
		int8_t p;
	}code[Y_MB_KEY_SIZE+1];
};

/* store as list, child of item */
struct y_mb_ci{
	struct y_mb_ci *next;
	uint16_t len:9;
	uint16_t zi:1;
	uint16_t ext:1;
	uint16_t del:1;
	uint16_t simp:1;
	uint16_t dic:3;
	uint8_t data[2];
};

/* store as list, is one node of code */
struct y_mb_item{
	struct y_mb_item *next;
	uintptr_t code; /* if not pointer, this one only have 2- */
	struct y_mb_ci *phrase;
};

struct y_mb_index{
	struct y_mb_index *next;
	struct y_mb_item *item;
	struct y_mb_item *half;
	uint64_t zi_count:24;
	uint64_t ext_count:24;
	uint64_t index:16;
	uint32_t ci_count;
};

struct y_mb_context{
	int sp;
	int result_match;
	int result_dummy;
	int result_count;
	int result_count_zi;
	int result_count_ci_ext;
	int result_filter;			/* not allow ext zi */
	int result_filter_zi;		/* only allow zi */
	int result_filter_ext;		/* only allow ext zi */
	int result_filter_ci_ext;	/* allow ext ci */
	int result_wildcard;
	int result_compact;
	int result_has_next;
	struct y_mb_item *result_first;
	struct y_mb_index *result_index;
	void *result_ci;
	char input[Y_MB_KEY_SIZE+1];
};

/* pin ci */
struct y_mb_pin_ci{
	struct y_mb_pin_ci *next;
	int8_t pos;
	uint8_t len;
	char data[];
};

/* pin code */
struct y_mb_pin_item{
	struct y_mb_pin_item *next;
	struct y_mb_pin_ci *list;
	uint8_t len;
	char data[];		// with nul at end
};

/* user phrase */
#define Y_MB_DELETE		(-1)
#define Y_MB_PREPEND	(0)
#define Y_MB_APPEND		(0x7fffffff)

#define Y_MB_DIC_MAIN	0x00
#define Y_MB_DIC_SUB	0x01
#define Y_MB_DIC_PIN	0x04
#define Y_MB_DIC_USER	0x05
#define Y_MB_DIC_TEMP	0x06
#define Y_MB_DIC_ASSIST	0x07

/* MB */
struct y_mb{
	/* mb name */
	char name[16];

	/* rule to make phrase */
	struct y_mb_rule *rule;
	
	/* if do optiomize, if it is assist mb */
	int flag;

	/* hanzi list */
	void *zi;

	/* main mb */
	char *main;
	
	/* sub dicts */
	char *dicts[10];

	/* user mb */
	char *user;
	int dirty;
	int dirty_max;

	/* assist mb */
	char lead;
	struct y_mb *ass;
	char *ass_main;
	
	/* quick mb */
	char quick_lead;
	struct y_mb *quick_mb;
	
	/* normal hz table of the mb */
	char *normal;

	/* some config of mb */
	char key[64];
	char key0[8];
	char map[128];
	char wildcard;
	char wildcard_orig;
	uint8_t english;
	uint8_t filter;
	uint8_t len;
	uint8_t commit_mode;
	uint8_t commit_len;
	uint8_t commit_which;
	uint8_t stop;
	char push[10];
	char pull[10];
	char skip[10];
	char bihua[10];
	char nomove[4];

	uint8_t match:1;
	uint8_t simple:2;
	uint8_t compact:2;
	uint8_t yong:2;
	uint8_t pinyin:2;
	uint8_t auto_clear:4;
	uint8_t nsort:1;
	uint8_t hint:1;
	uint8_t dwf:1;/* disable wildcard at first */
	uint8_t capital:1;
	uint8_t jing_used:1;
	uint8_t encrypt;
	uint8_t auto_move;
	uint8_t sloop;
	uint8_t split;
	
	/*0: gb18030 1: utf-8 */
	uint8_t encode;

	/* something used to fast append data */
	struct y_mb_index *last_index;
	struct y_mb_item *last_link;

	/* index of the mb */
	struct y_mb_index *index;
	struct y_mb_index *half;
	trie_tree_t *trie;
	
	/* pin mb index */
	void *pin;
	
	/* fuzzy table */
	void *fuzzy;
	
	/* context of current input */
	struct y_mb_context ctx;
	
	/* cancel mb load */
	volatile uint8_t cancel;
};

#define MB_DUMP_MAIN	0x01
#define MB_DUMP_USER	0x02
#define MB_DUMP_DICTS	0x04
#define MB_DUMP_TEMP	0x08
#define MB_DUMP_ADJUST	0x10
#define MB_DUMP_HEAD	0x20
#define MB_DUMP_ALL		0xff

#define MB_FMT_YONG		0x00
#define MB_FMT_WIN		0x01
#define MB_FMT_FCITX	0x02
#define MB_FMT_SCIM		0x03

#define MB_FLAG_ASSIST	0x01		/* load mb as assist */
#define MB_FLAG_SLOW	0x02		/* test if code data pair exist */
#define MB_FLAG_ADICT	0x04		/* use assist just as sub dict */
#define MB_FLAG_ZI		0x08		/* force load zi at assist */
#define MB_FLAG_CODE	0x10		/* only load code */
#define MB_FLAG_NOUSER	0x20		/* don't load user dict */
#define MB_FLAG_ASSIST_CODE 0x19

/* params used to adjust default mb */
struct y_mb_arg{
	char *dicts;					/* dicts defined at config file */
	int apos;						/* assist code position */
	int wildcard;					/* wildcard in config file */
};

void y_mb_init(void);
void y_mb_cleanup(void);
struct y_mb *y_mb_new(void);
void y_mb_free(struct y_mb *mb);
int y_mb_load_to(struct y_mb *mb,const char *fn,int flag,struct y_mb_arg *arg);
struct y_mb *y_mb_load(const char *fn,int flag,struct y_mb_arg *arg);
int y_mb_set(struct y_mb *mb,const char *s,int len,int filter);
int y_mb_get(struct y_mb *mb,int at,int num,
	char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1]);
struct y_mb_ci *y_mb_get_first(struct y_mb *mb,char *cand);
int y_mb_get_legend(struct y_mb *mb,const char *src,int slen,
		int dlen,char calc[][MAX_CAND_LEN+1],int max);
int y_mb_super_get(struct y_mb *mb,char calc[][MAX_CAND_LEN+1],int max,char super);
int y_mb_get_simple(struct y_mb *mb,char *code,char *data,int p);
int y_mb_is_key(struct y_mb *mb,int c);
int y_mb_is_keys(struct y_mb *mb,char *s);
int y_mb_is_full(struct y_mb *mb,int len);
int y_mb_is_stop(struct y_mb *mb,int c,int pos);
int y_mb_is_pull(struct y_mb *mb,int c);
int y_mb_has_next(struct y_mb *mb,int dext);
int y_mb_move_phrase(struct y_mb *mb,const char *code,const char *phrase,int dir);
int y_mb_auto_move(struct y_mb *mb,const char *code,const char *phrase,int auto_move);
int y_mb_add_phrase(struct y_mb *mb,const char *code,const char *phrase,int pos,int dic);
int y_mb_del_phrase(struct y_mb *mb,const char *code,const char *phrase);
int y_mb_find_code(struct y_mb *mb,const char *hz,char (*tab)[MAX_CAND_LEN+1],int max);
int y_mb_find_simple_code(struct y_mb *mb,const char *hz,const char *code,char *out,int filter,int total);
int y_mb_get_full_code(struct y_mb *mb,const char *data,char *code);
void y_mb_set_zi(struct y_mb *mb,int zi);
void y_mb_set_ci_ext(struct y_mb *mb,int ci_ext);
int y_mb_code_by_rule(struct y_mb *mb,const char *s,int len,char *out,...);
void y_mb_save_user(struct y_mb *mb);
int y_mb_has_wildcard(struct y_mb *mb,const char *s);
FILE *y_mb_open_file(const char *fn,const char *mode);
int y_mb_max_match(struct y_mb *mb,char *s,int len,int dlen,int filter,int *good,int *less);
int y_mb_dump(struct y_mb *mb,FILE *fp,int option,int format,char *pre);
int y_mb_get_exist_code(struct y_mb *mb,const char *data,char *code);
void y_mb_push_context(struct y_mb *mb,struct y_mb_context *ctx);
void y_mb_pop_context(struct y_mb *mb,struct y_mb_context *ctx);
int y_mb_assist_get(struct y_mb *mb,char calc[][MAX_CAND_LEN+1],int max,char super,int end);
int y_mb_assist_get2(struct y_mb *mb,char calc[][MAX_CAND_LEN+1],int max,char super[2],int end);
int y_mb_before_assist(struct y_mb *mb);
struct y_mb_ci *y_mb_code_exist(struct y_mb *mb,const char *code,int len,int count);
struct y_mb_ci *y_mb_ci_exist(struct y_mb *mb,const char *data,int dic);
int y_mb_is_good_code(struct y_mb *mb,const char *code,const char *s);
char *y_mb_ci_string(struct y_mb_ci *ci);
char *y_mb_ci_string2(struct y_mb_ci *ci,char *out);
int y_mb_predict_simple(struct y_mb *mb,char *s,char *out,int *out_len,int (*freq)(const char *));
struct y_mb_item *y_mb_get_zi(struct y_mb *mb,const char *s,int len,int filter);
struct y_mb_ci *y_mb_get_first_zi(struct y_mb *mb,const char *s,int len,int filter);
int y_mb_in_result(struct y_mb *mb,struct y_mb_ci *c);
int y_mb_assist_test(struct y_mb *mb,struct y_mb_ci *c,char super,int n,int end);
int y_mb_assist_test_hz(struct y_mb *mb,const char *s,char super);
struct y_mb_ci *y_mb_check_assist(struct y_mb *mb,const char *s,int len,char super,int end);
int y_mb_is_assist_key(struct y_mb *mb,int key);
int y_mb_load_quick(struct y_mb *mb,const char *quick);
int y_mb_load_pin(struct y_mb *mb,const char *pin);
void y_mb_init_pinyin(struct y_mb *mb);
int y_mb_load_fuzzy(struct y_mb *mb,const char *fuzzy);
void y_mb_key_map_init(const char *key,int wildcard,char *map);
int y_mb_zi_has_code(struct y_mb *mb,const char *zi,const char *code);

/* yong only */
void y_mb_calc_yong_tip(struct y_mb *mb,const char *code,const char *cand,char *tip);

extern EXTRA_IM EIM;
static inline struct y_mb *Y_MB_ACTIVE(struct y_mb *mb)
{
	char *s=EIM.CodeInput,c=s[0];
	if(!c) return mb;
	if(mb->ass && c==mb->lead)
		return mb->ass;
	if(mb->quick_mb && c==mb->quick_lead)
		return mb->quick_mb;
	return mb;
}

#endif/*_MB_H_*/
