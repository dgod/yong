#ifndef _COMMON_H_
#define _COMMON_H_

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef _WIN32
#include <unistd.h>
#include <dlfcn.h>
#include <sys/time.h>
#ifndef CFG_NO_GLIB
#include <glib.h>
#endif
#else
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _WIN64
//#define WINVER 0x0500
#endif
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <richedit.h>
#include <wchar.h>
#include <tchar.h>

#endif

#include "xim.h"
#include "ui.h"
#include "mapfile.h"
#include "llib.h"

int y_im_copy_file(char *src,char *dst);
int y_im_config_path(void);
const char *y_im_get_path(const char *type);
int y_im_str_to_key(const char *s);
int y_im_get_key(const char *name,int pos,int def);
char *y_im_str_escape(const char *s,int commit);
void y_im_expand_space(char *s);
void y_im_expand_env(char *s);
void y_im_disp_cand(const char *gb,char *out,int pre,int suf);
int y_im_str_encode(const char *gb,void *out,int flags);
void y_im_str_encode_r(const void *in,char *gb);
int y_im_strip_key(char *gb);
void y_im_load_punc(void);
int y_im_forward_key(const char *s);
void y_im_url_encode(char *gb,char *out);
int y_im_go_url(const char *s);
int y_im_send_file(const char *s);
int y_im_str_desc(const char *s,void *out);
int y_im_get_real_cand(const char *s,char *out,size_t size);
void y_im_free_urls(void);
void y_im_load_urls(void);
char *y_im_find_url(char *pre);
char *y_im_find_url2(char *pre,int next);
void y_im_backup_file(char *path,char *suffix);
void y_im_copy_config(void);
void *y_im_module_open(char *path);
void *y_im_module_symbol(void *mod,char *name);
void y_im_module_close(void *mod);
int y_im_run_tool(char *func,void *arg,void **out);
FILE *y_im_open_file(const char *fn,const char *mode);
void y_im_remove_file(char *fn);
void y_im_set_default(int index);
int y_im_get_current(char *item,int len);
char *y_im_get_current_engine(void);
void y_im_about_self(void);
void y_im_setup_config(void);
int y_im_key_desc_update(void);
int y_im_key_desc_translate(const char *code,int pos,char *data,char *res,int size);
void y_im_run_helper(char *prog,char *watch,void (*cb)(void));
char *y_im_auto_path(char *fn);
uint32_t y_im_tick(void);
char *y_im_get_im_name(int index);
char *y_im_get_im_config_string(int index,const char *key);
int y_im_get_im_config_int(int index,const char *key);
int y_im_has_im_config(int index,const char *key);
int y_strchr_pos(char *s,int c);
int y_im_last_key(int key);
void y_im_str_strip(char *s);
int y_im_help_desc(char *wh,char *desc,int len);
void y_im_show_help(char *wh);
int y_im_get_keymap(char *name,int len);
int y_im_show_keymap(void);
int y_im_gen_mac(void);
int y_im_diff_hand(char c1,char c2);
void y_im_verbose(const char *fmt,...);
int y_im_is_url(const char *s);

#define VERBOSE(...) //y_im_verbose(__VA_ARGS__)

struct y_im_speed{
	int zi;
	int key;
	int space;
	int select2;
	int select3;
	int select;
	int back;
	int speed;
	time_t start;
	time_t last;
};

void y_im_speed_reset(void);
void y_im_speed_update(int key,const char *s);
int y_im_input_key(int key);

#define SEND_FLUSH			0x01
#define SEND_BIAODIAN		0x02
#define SEND_RAW			0x04

#define DONT_ESCAPE			0x01

int y_xim_init(const char *name);
char *y_xim_get_name(void);
void y_xim_forward_key(int key);
void y_xim_set_last(const char *s);
const char *y_xim_get_last(void);
void y_xim_send_string(const char *s);
void y_xim_send_string2(const char *s,int flag);
void y_xim_preedit_clear(void);
CONNECT_ID *y_xim_get_connect(void);
int y_xim_trigger_key(int key);
void y_xim_put_connect(CONNECT_ID *id);
void y_xim_update_config(void);
void y_xim_explore_url(const char *s);
void y_xim_preedit_draw(char *s,int len);
void y_xim_enable(int enable);
int y_xim_input_key(int key);

void y_im_history_free(void);
void y_im_history_init(void);
void y_im_history_write(const char *s);
void y_im_history_update(void);
int y_im_history_query(const char *src,char out[][MAX_CAND_LEN+1],int max);
const char *y_im_history_get_last(int len);

#if defined(_WIN32) && !defined(_WIN64)
void y_im_nl_day(__time64_t t,char *s);
#else
void y_im_nl_day(time_t t,char *s);
#endif

void *y_dict_open(const char *file);
void y_dict_close(void *p);
char *y_dict_query(void *p,char *s);
int y_dict_query_and_show(void *p,char *s);

void y_im_debug(char *fmt,...);

LKeyFile *y_im_get_menu_config(void);
int y_im_handle_menu(const char *cmd);

int y_kbd_init(const char *fn);
int y_kbd_show(int b);
void y_kbd_popup_menu(void);

int y_replace_init(const char *file);
int y_replace_free(void);
int y_replace_string(const char *in,void (*output)(const char *,int),int flags);
int y_replace_enable(int enable);

void y_im_load_book(void);
void y_im_free_book(void);
int y_im_book_decrypt(const char *s,char *out);
int y_im_book_encrypt(const char *s,char *out);
int y_im_book_key(const char *s);

int y_im_request(int cmd);

void y_im_update_main_config(void);
void y_im_update_sub_config(const char *name);
void y_im_free_config(void);
char *y_im_get_config_string(const char *group,const char *key);
int y_im_get_config_int(const char *group,const char *key);
int y_im_has_config(const char *group,const char *key);
const char *y_im_get_config_data(const char *group,const char *key);

typedef struct{
	void *next;
	char *url;
}Y_USER_URL;

Y_USER_URL *y_im_user_urls(void);

int y_main_init(int index);
void y_main_clean(void);

#endif/*_COMMON_H_*/
