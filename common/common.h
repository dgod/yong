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

int y_im_copy_file(const char *src,const char *dst);
int y_im_config_path(void);
const char *y_im_get_path(const char *type);
int y_im_str_to_key(const char *s,int *repeat);
int *y_im_str_to_keys(const char *s);
int y_im_get_key(const char *name,int pos,int def);
int y_im_key_eq(int k1,int k2);
int y_im_parse_keys(const char *s,int *out,int size);
char **y_im_parse_argv(const char *s,int size);
char *y_im_str_escape(const char *s,int commit,int64_t t);
void y_im_disp_cand(const char *gb,char *out,int pre,int suf,const char *code,const char *tip);
int y_im_str_encode(const char *gb,void *out,int flags);
void y_im_str_encode_r(const void *in,char *gb);
int y_im_str_len(const void *in);
void y_im_load_punc(void);
int y_im_forward_key(const char *s);
void y_im_url_encode(const char *gb,char *out);
int y_im_go_url(const char *s);
int y_im_send_file(const char *s);
int y_im_str_desc(const char *s,void *out);
int y_im_get_real_cand(const char *s,char *out,size_t size);
void y_im_free_urls(void);
void y_im_load_urls(void);
char *y_im_find_url(const char *pre);
char *y_im_find_url2(const char *pre,int next);
void y_im_backup_file(const char *path,const char *suffix);
void y_im_copy_config(void);
void *y_im_module_open(const char *path);
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
int y_im_key_desc_translate(const char *code,const char *tip,const int pos,const char *data,char *res,int size);
bool y_im_cand_desc_translate(const char *data,const char *code,const char *tip,char *res,int size);
bool y_im_key_desc_is_first(int code);
int y_im_key_desc_first(int code,int len,char *res,int size);
int y_im_run_helper(const char *prog,const char *watch,void (*cb)(void),void (*exit_cb)(int));
char *y_im_auto_path(char *fn);
uint32_t y_im_tick(void);
char *y_im_get_im_name(int index);
char *y_im_get_im_config_string(int index,const char *key);
const char *y_im_get_im_config_data(int index,const char *key);
int y_im_get_im_config_int(int index,const char *key);
int y_im_has_im_config(int index,const char *key);
int y_strchr_pos(const char *s,int c);
int y_im_last_key(int key);
void y_im_str_strip(char *s);
int y_im_help_desc(char *wh,char *desc,int len);
void y_im_show_help(char *wh);
int y_im_get_keymap(char *name,int len);
int y_im_show_keymap(void);
int y_im_gen_mac(void);
int y_im_diff_hand(char c1,char c2);
int y_im_is_url(const char *s);
void y_im_set_last_code(const char *s,const char *cand);
const char *y_im_get_last_code(void);
void y_im_repeat_last_code(void);

#define EXPAND_SPACE			0x01
#define EXPAND_ENV				0x02
#define EXPAND_DESC				0x04
void y_im_expand_space(char *s);
void y_im_expand_env(char *s,int size);
int y_im_expand_with(const char *s,char *to,int size,int which);

#define VERBOSE(...) //YongLogWrite(__VA_ARGS__)

struct y_im_speed{
	int zi;
	int key;
	int space;
	int select2;
	int select3;
	int select;
	int back;
	int speed;
	int64_t start;
	int64_t last;
};

void y_im_speed_init(void);
void y_im_speed_save(void);
void y_im_speed_reset(void);
void y_im_speed_update(int key,const char *s);
char *y_im_speed_stat(void);
int y_im_input_key(int key);

#define SEND_FLUSH			0x01
#define SEND_BIAODIAN		0x02
#define SEND_RAW			0x04
#define SEND_GO				0x08

#define DONT_ESCAPE			0x01

int y_xim_init(const char *name);
char *y_xim_get_name(void);
void y_xim_forward_key(int key,int repeat);
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
void y_xim_send_keys(const char *s);
int y_xim_get_onspot(void);

void y_im_history_free(void);
void y_im_history_init(void);
int y_im_history_write(const char *s,bool flush);
void y_im_history_update(void);
int y_im_history_query(const char *src,char out[][MAX_CAND_LEN+1],int max);
const char *y_im_history_get_last(int len);
void y_im_history_flush(void);
void y_im_history_redirect_init(void);
void y_im_history_redirect_free(void);
int y_im_history_redirect_run(void);

int y_im_nl_day(int64_t t,char *s);

void *y_dict_open(const char *file);
void y_dict_close(void *p);
char *y_dict_query(void *p,char *s);
int y_dict_query_and_show(void *p,const char *s);
int y_dict_query_network(const char *s);

LKeyFile *y_im_get_menu_config(void);
int y_im_handle_menu(const char *cmd);

int y_kbd_init(const char *fn);
int y_kbd_show(int b);
int y_kbd_show_with_main(int b);
void y_kbd_popup_menu(void);
void y_kbd_select(int pos,int sub);

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
char *y_im_get_config_string_gb(const char *group,const char *key);
int y_im_get_config_int(const char *group,const char *key);
int y_im_has_config(const char *group,const char *key);
const char *y_im_get_config_data(const char *group,const char *key);
void y_im_set_config_string(const char *group,const char *key,const char *val);
void y_im_save_config(void);

int y_im_load_app_config(void);
void y_im_free_app_config(void);
LKeyFile *y_im_get_app_config(const char *exe);
LKeyFile *y_im_get_app_config_by_pid(int pid);
#ifdef _WIN32
LKeyFile *y_im_get_app_config_by_hwnd(HWND w);
char *y_im_get_app_exe_by_hwnd(HWND w,char out[256]);
#endif

typedef struct{
	void *next;
	char *url;
}Y_USER_URL;

Y_USER_URL *y_im_user_urls(void);

int y_im_async_init(void);
int y_im_async_write_file(const char *file,LString *data,bool backup);
int y_im_async_wait(int timeout);
void y_im_async_destroy(void);
int y_im_async_spawn(char **argv,void (*cb)(const char *text,void *user),void *user);

int y_main_init(int index);
void y_main_clean(void);

int y_im_check_select(int key,int flags);

#endif/*_COMMON_H_*/
