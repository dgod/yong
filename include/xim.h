#ifndef _XIM_H_
#define _XIM_H_

#include "yong.h"
#include <time.h>

#define POSITION_ORIG	0xffff

#define LANG_CN		0
#define LANG_EN		1
#define CORNER_HALF	0
#define CORNER_FULL	1

typedef struct _CONNECT_ID{
	struct _CONNECT_ID *next;
	unsigned int connect_id;
	unsigned int dummy:1;
	unsigned int state:1;
	unsigned int lang:1;
	unsigned int corner:1;
	unsigned int biaodian:1;
	unsigned int trad:1;
	unsigned int track:1;
	unsigned int focus:1;
	int x,y;
	time_t last_active;
}CONNECT_ID;

typedef struct{
	char *name;
	CONNECT_ID *(*get_connect)(void);
	void (*put_connect)(CONNECT_ID *);
	int (*init)(void);
	void (*destroy)(void);
	void (*enable)(int enable);
	void (*forward_key)(int key,int repeat);
	int (*trigger_key)(int key);
	void (*send_string)(const char *s,int flags);
	int (*preedit_clear)(void);
	int (*preedit_draw)(const char *s,int len);
	int (*input_key)(int key);
	void (*update_config)(void);
	void (*explore_url)(const char *s);
}Y_XIM;

CONNECT_ID *YongGetConnect(void);
void YongMoveInput(int x,int y);
void YongShowInput(int show);
void YongShowMain(int show);
int YongHotKey(int key);
int YongKeyInput(int key,int mod);
void YongForwardKey(int key,int repeat);
void YongSendString(const char *s,int flags);
int YongInitXIM(void);
void YongDestroyXIM(void);
void YongResetIM(void);
void YongSetLang(int lang);
void YongSetBiaodian(int biaodian);
void YongSetCorner(int corner);
void YongSetTrad(int trad);
void YongUpdateMain(CONNECT_ID *id);
void YongReloadAll(void);
void YongDestroyIM(void);
int YongSwitchIM(int id);
void YongDoDebug(void);
void YongRecycleConnect(int force);
void YongEnableIM(int enable);
int YongTriggerKey(int key);
int YongOutputSet(int type);
int YongPreeditClear(void);
int YongPreeditDraw(const char *s,int len);
void YongSendFile(const char *fn);
void YongSendClipboard(const char *s);
int YongInputKey(int key);
const void *YongGetSelectNumber(int n);
void YongLogWrite(const char *fmt,...);

#endif/*_XIM_H_*/
