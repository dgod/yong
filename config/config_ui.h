#ifndef _CONFIG_UI_H_
#define _CONFIG_UI_H_

#include "llib.h"

extern LKeyFile *config;
extern LXml *custom;

enum{
	CU_WINDOW=0,
	CU_LABEL,
	CU_EDIT,
	CU_LIST,
	CU_COMBO,
	CU_CHECK,
	CU_BUTTON,
	CU_TREE,
	CU_ITEM,
	CU_GROUP,
	CU_PAGE,
	CU_FONT,
	CU_IMAGE,
	CU_SEPARATOR
};

typedef struct{
	int x,y,w,h;
}CURect;

typedef struct _CUAction *CUAction;
typedef struct _CUCtrl *CUCtrl;
typedef struct _CUMenu *CUMenu;
typedef struct _CUMenuEntry *CUMenuEntry;

struct _CUAction{
	CUAction next;
	int (*action)(CUCtrl,int,char **);
	int arc;
	char **arg;
};

struct _CUCtrl{
	char *group;
	int type;
	void *self;
	CUCtrl parent;
	CUCtrl next;
	CUCtrl child;
	CUCtrl tlist;
	CURect pos;
	char *text;

	char *cgroup;
	char *ckey;
	int cpos;
	
	char **data;
	char **view;

	CUAction action;
	CUAction init;
	CUAction save;
	
	CUMenu menu;
	
	int init_done;
	int visible;

	const LXmlNode *node;
	int realized;
	void *priv;
};

struct _CUMenuEntry{
	char *text;
	void (*cb)(void *,void*);
	void *arg;
	LFreeFunc free;
};

struct _CUMenu{
	void *self;
	int count;
	int x,y;
	struct _CUMenuEntry entries[];
};

const char *y_im_get_path(const char *type);
LKeyFile *y_im_load_config(char *fn);
int y_im_set_default(int index);
void cu_reload();
void cu_notify_reload();

int cfg_install(const char *name);
int cfg_uninstall(const char *name);
extern int cu_reload_ui;
extern int cu_quit_ui;

int cu_ctrl_init_self(CUCtrl p);
int cu_ctrl_init_done(CUCtrl p);
void cu_ctrl_destroy_self(CUCtrl p);
int cu_ctrl_show_self(CUCtrl p,int b);
int cu_ctrl_set_self(CUCtrl p,const char *s);
int cu_ctrl_set_prop(CUCtrl p,const char *prop);
char *cu_ctrl_get_self(CUCtrl p);

void cu_config_save(void);
int cu_config_init_default(CUCtrl p);
int cu_config_save_default(CUCtrl p);
int cu_config_set(const char *group,const char *key,int pos,const char *value);
char *cu_config_get(const char *group,const char *key,int pos);

CUCtrl cu_ctrl_new(CUCtrl parent,const LXmlNode *node);
void cu_ctrl_free(CUCtrl p);
CUCtrl cu_ctrl_list_from_type(int type);
CUCtrl cu_ctrl_from_group(CUCtrl root,const char *group);
CUCtrl cu_ctrl_get_root(CUCtrl p);
void cu_ctrl_foreach(CUCtrl root,void (*cb)(CUCtrl,void*),void*user);

int cu_ctrl_action_run(CUCtrl ctrl,CUAction action);
void cu_ctrl_action_free(CUCtrl ctrl,CUAction action);

int cu_confirm(CUCtrl p,const char *message);

void cu_menu_popup(CUCtrl p,CUMenu m);
void cu_menu_init_self(CUMenu m);
void cu_menu_destroy_self(CUMenu m);
CUMenu cu_menu_new(int count);
void cu_menu_free(CUMenu m);
int cu_menu_append(CUMenu m,char *text,void (*cb)(void*,void*),void *arg,LFreeFunc arg_free);
CUMenu cu_menu_install(void);

char *cu_translate(const char *s);

int cu_init(void);
int cu_loop(void);
int cu_quit(void);
int cu_step(void);

void cu_show_page(const char *name);

int cu_screen_dpi(void);

void cu_init_all(CUCtrl ctrl,void *user);

extern double CU_SCALE;

#endif/*_CONFIG_UI_H_*/
