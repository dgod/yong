#pragma once

#define MAX_LEN		63
typedef struct{
	char text[MAX_LEN+1];
	uint8_t caret;
	uint8_t len;
}LLineState;

typedef struct{
	LLineState prev;
	union{
		LLineState cur;
		struct{
			char text[MAX_LEN+1];
			uint8_t caret;
			uint8_t len;
		};
	};
	uint8_t allow[12];
	uint8_t max;
	char first;
	bool first_only;
	int left,right,home,end;
}LLineEdit;

void l_line_edit_init(LLineEdit *p);
LLineEdit *l_line_edit_new(void);
#define l_line_edit_free(p) l_free(p)
bool l_line_edit_set_max(LLineEdit *p,int max);
void l_line_edit_set_nav(LLineEdit *p,int left,int right,int home,int end);
void l_line_edit_clear(LLineEdit *p);
void l_line_edit_set_allow(LLineEdit *p,const char *s,bool clear);
int l_line_edit_push(LLineEdit *p,int key);
void l_line_edit_shift(LLineEdit *p,int count);
bool l_line_edit_unshift(LLineEdit *p,const char *s);
void l_line_edit_undo(LLineEdit *p);
#define l_line_edit_get_text(p) ((p)->text)
#define l_line_edit_get_len(p) ((p)->len)
#define l_line_edit_get_caret(p) ((p)->caret)
int l_line_edit_copy(LLineEdit *p,char *result,int len,int *caret);
bool l_line_edit_set_caret(LLineEdit *p,int caret);
bool l_line_edit_set_text(LLineEdit *p,const char *s);
bool l_line_edit_set_first(LLineEdit *p,int first,bool only);

