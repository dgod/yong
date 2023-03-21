#ifndef _PINYIN_H_
#define _PINYIN_H_

#define PY_MAX_TOKEN		128

struct py_item;
typedef struct py_item *py_item_t;

void py_init(int split,char *sp);

int py_parse_string(const char *input,py_item_t *token,int caret,int (*check)(int,const char*,void *),void *arg);
int py_build_string(char *out,py_item_t *token,int count);
int py_build_sp_string(char *out,py_item_t *token,int count);
int py_prepare_string(char *to,const char *from,int *caret);
int py_parse_sp_jp(const char *input,py_item_t *token);
int py_caret_to_pos(py_item_t *token,int count,int caret);
int py_caret_next_item(py_item_t *token,int count,int caret);
int py_string_step(char *input,int caret,uint8_t step[],int max);
int py_conv_from_sp(const char *in,char *out,int size,int split);
int py_conv_to_sp(const char *s,const char *zi,char *out);
int py_conv_to_sp2(const char *s,const char *zi,char *out,int (*first_code)(uint32_t,char*,void*),void *arg);
int py_is_valid_input(int sp,int c,int pos);
int py_is_valid_code(const char *in);
int py_is_valid_quanpin(const char *input);
int py_is_valid_sp(const char *input);
int py_remove_split(py_item_t *token,int count);
int py_pos_of_sp(const char *in,int pos);
int py_pos_of_qp(py_item_t *in,int pos);
int py_sp_has_semi(void);
int py_get_space_pos(py_item_t *token,int count,int space);
const char *py_sp_get_chshzh(void);
int py_sp_unlikely_jp(const char *in);
int py_quanpin_maybe_jp(const py_item_t *token,int count);
int py_first_zrm_shen(const char *s);
int py_jp_from_qp(const char *s,const char *zi,char *out);
int py_item_len(py_item_t it);

#endif/*_PINYIN_H_*/
