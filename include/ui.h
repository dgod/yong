#ifndef _UI_H_
#define _UI_H_

#include "xim.h"
#include "im.h"

typedef struct{
	int x,y;
}UI_POINT;

typedef struct{
	int x,y,w,h;
}UI_RECT;

typedef struct{
	/* scale factor */
	int scale;
	/* line width */
	double line_width;
	/* move style */
	int move_style;
	/* window pos and size*/
	UI_RECT rc;
	/* rect where user can drag the window */
	UI_RECT move;
	/* background image */
	char *bg;
	/* border color */
	char *border;
	/* transparent */
	int tran;
	/* auto transparent */
	int auto_tran;
}UI_MAIN;

typedef struct{
	/* scale factor */
	int scale;
	/* line width */
	double line_width;
	/* background image or color */
	char *bg;
	/* border color */
	char *border;
	/* text colors */
	char *text[7];
	/* font of the text */
	char *font;
	/* orig width and height of input window */
	int w,h;
	/* minimal size of input window */
	int mw,mh;
	/* cand string offset */
	UI_POINT cand;
	/* code string offset */
	UI_POINT code;
	/* offset to caret */
	UI_POINT off;
	/* resize region */
	short left,right;
	short top,bottom;
	/* extra space for input window */
	short work_left,work_right;
	short work_bottom;
	/* if code hint */
	int hint;
	/* space between cand words */
	int space;
	/* if follow caret */
	int root;
	/* don't show the input window */
	int noshow;
	/* mode of input window, 0:2 lines 1:1 lines 2:mulitlines */
	int line;
	/* if show page number */
	int page;
	/* if show caret */
	int caret;
	/* howto show no can cand list */
	int no;
	/* howto show seperator */
	char *sep;
	/* how long candword will strip */
	int strip;
	/* transparent */
	int tran;
	/* position of original pos */
	int x,y;
	/* cand max */
	int cand_max;
}UI_INPUT;

typedef struct{
	int x,y;
	char *normal;
	char *over;
	char *down;
	char *font;
	char *color;
	void (*click)(void *);
	void *arg;
}UI_BUTTON;

typedef struct{
	int enable;
	char *icon[2];
}UI_TRAY;

#define KBD_BTN_NONE		0
#define KBD_BTN_NORMAL		1
#define KBD_BTN_ALPHA		2
#define KBD_BTN_CAPSLOCK	3
#define KBD_BTN_SHIFT		4
#define KBD_BTN_KEY			5
typedef struct{
	unsigned int width:8;
	unsigned int type:3;
	unsigned int pressed:1;
	union{
		char ch[8];
		int key;
	};
}UI_KBD_BTN;

typedef struct{
	short cur;
	short width;
	short height;
	char *bg[2];
	char *text[2];
	char *font;
	UI_KBD_BTN array[5][14];
}UI_KBD;
extern UI_KBD y_ui_kbd;

#define UI_TRAY_ON		0
#define UI_TRAY_OFF		1

#define UI_BTN_CN		0
#define UI_BTN_EN		1
#define UI_BTN_QUAN		2
#define UI_BTN_BAN		3
#define UI_BTN_CN_BIAODIAN	4
#define UI_BTN_EN_BIAODIAN	5
#define UI_BTN_SIMP		6
#define UI_BTN_TRAD		7
#define UI_BTN_KEYBOARD	8
#define UI_BTN_NAME		9
#define UI_BTN_MENU		10
#define UI_BTN_COUNT		(UI_BTN_MENU+1)

int ui_init(void);
void ui_clean(void);
int ui_loop(void);
int ui_main_update(UI_MAIN *param);
void *ui_main_win(void);
int ui_input_update(UI_INPUT *param);
int ui_input_redraw(void);
int ui_button_update(int id,UI_BUTTON *param);
int ui_button_show(int id,int show);
int ui_main_show(int show);
int ui_input_show(int show);
int ui_input_move(int off,int *x,int *y);
void ui_tray_update(UI_TRAY *param);
void ui_tray_status(int which);
void ui_tray_tooltip(const char *tip);
char *ui_get_select(int (*)(const char*));
void ui_setup_config(void);
void ui_select_type(char *type);
void ui_update_menu(void);
void ui_skin_path(const char *p);
void ui_cfg_ctrl(char *name,...);
void ui_show_message(const char *s);
void ui_show_image(char *name,char *file,int top,int tran);
int ui_button_label(int id,const char *text);

typedef struct{
	int (*init)(void);
	void (*clean)(void);
	int (*loop)(void);

	int (*main_update)(UI_MAIN *);
	void *(*main_win)(void);
	int (*main_show)(int show);

	int (*input_update)(UI_INPUT *);
	void *(*input_win)(void);
	int (*input_draw)(void);
	int (*input_redraw)(void);
	int (*input_show)(int show);
	int (*input_move)(int off,int *x,int *y);
	
	int (*button_update)(int id,UI_BUTTON *);
	int (*button_show)(int id,int show);
	int (*button_label)(int id,const char *text);
	
	void (*tray_update)(UI_TRAY *);
	void (*tray_status)(int which);
	void (*tray_tooltip)(const char *tip);
	
	char *(*get_select)(int(*)(const char*));
	void (*select_type)(char *type);
	
	void (*update_menu)(void);
	void (*skin_path)(const char *p);
	void (*cfg_ctrl)(char *name,...);
	void (*show_message)(const char *s);
	void (*show_image)(char *name,char *file,int top,int tran);
	void (*show_tip)(const char *,...);
	
	void (*beep)(int c);
	int (*request)(int cmd);
	
	double (*get_scale)(void);
}Y_UI;

extern Y_UI y_ui;
void ui_setup_default(Y_UI *p);
void ui_setup_fbterm(Y_UI *p);

int y_ui_init(const char *name);
#define y_ui_loop() \
	y_ui.loop()
#define y_ui_clean() \
	y_ui.clean()

#define y_ui_main_update(a) \
	(y_ui.main_update?y_ui.main_update(a):0)
#define y_ui_main_win() \
	(y_ui.main_win?y_ui.main_win():NULL)
#define y_ui_main_show(a) \
	(y_ui.main_show?y_ui.main_show(a):0)

#define y_ui_input_update(a) \
	(y_ui.input_update?y_ui.input_update(a):0)
#define y_ui_input_win() \
	(y_ui.input_win?y_ui.input_win():NULL)
#define y_ui_input_draw() \
	(y_ui.input_draw?y_ui.input_draw():0)
#define y_ui_input_redraw(a) \
	(y_ui.input_redraw?y_ui.input_redraw(a):0)
#define y_ui_input_show(a) \
	(y_ui.input_show?y_ui.input_show(a):0)
#define y_ui_input_move(a,b,c) \
	(y_ui.input_move?y_ui.input_move(a,b,c):0)
	
#define y_ui_button_update(a,b) \
	(y_ui.button_update?y_ui.button_update(a,b):0)
#define y_ui_button_show(a,b) \
	(y_ui.button_show?y_ui.button_show(a,b):0)
#define y_ui_button_label(a,b) \
	(y_ui.button_label?y_ui.button_label(a,b):0)
	
#define y_ui_tray_update(a) \
	do{if(y_ui.tray_update)y_ui.tray_update(a);}while(0)
#define y_ui_tray_status(a) \
	do{if(y_ui.tray_status)y_ui.tray_status(a);}while(0)
#define y_ui_tray_tooltip(a) \
	do{if(y_ui.tray_tooltip)y_ui.tray_tooltip(a);}while(0)
	
#define y_ui_select_type(a) \
	do{if(y_ui.select_type)y_ui.select_type(a);}while(0)
#define y_ui_get_select(a) \
	(y_ui.get_select?y_ui.get_select(a):0)

#define y_ui_update_menu(void) \
	do{if(y_ui.update_menu)y_ui.update_menu();}while(0)

#define y_ui_skin_path(a) \
	do{if(y_ui.skin_path)y_ui.skin_path(a);}while(0)

#define y_ui_show_message(a) \
	do{if(y_ui.show_message)y_ui.show_message(a);}while(0)

#define y_ui_show_image(a,b,c,d) \
	do{if(y_ui.show_image)y_ui.show_image(a,b,c,d);}while(0)
	
#define y_ui_show_tip(a...) \
	do{if(y_ui.show_tip)y_ui.show_tip(a);}while(0)
	
#define y_ui_cfg_ctrl(a...) \
	do{if(y_ui.cfg_ctrl)y_ui.cfg_ctrl(a);}while(0)
	
#define y_ui_beep(a) \
	do{if(y_ui.beep) y_ui.beep(a);}while(0)
	
#define y_ui_get_scale() \
	(y_ui.get_scale?y_ui.get_scale():1)
	
#endif/*_UI_H_*/
