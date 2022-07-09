#ifndef _IM_H_
#define _IM_H_

#include "yong.h"
#include "bihua.h"
#include "english.h"
#include "s2t.h"
#include "layout.h"

#ifdef _WIN32
#include <wchar.h>
#endif

typedef struct{
	void *handle;
	EXTRA_IM *eim;

	int Index;
	int CandWord;
	int Preedit;
	int Biaodian;
	int Trad;
	int TradDef;
	int Bing;
	int AssocLen;
	int AssocLoop;
	int InAssoc;
	int Beep;
	int BingSkip[2];
	int Hint;				// 编码提示

#if defined(_WIN32) || defined(CFG_XIM_ANDROID)
	uint16_t StringGet[MAX_CAND_LEN+1];
	uint16_t CodeInput[MAX_CODE_LEN*2+1];
#else
	char StringGet[(MAX_CAND_LEN+1)*3/2];
	char CodeInput[(MAX_CODE_LEN+1)*3];
#endif
	int CodeLen;
	char CandTable[10][(MAX_CAND_LEN+1)*2];
	char CodeTips[10][(MAX_TIPS_LEN+1)*2];

	char StringGetEngine[MAX_CAND_LEN+2];
	char CodeInputEngine[MAX_CODE_LEN+2];
	char CandTableEngine[10][MAX_CAND_LEN+1];
	char CodeTipsEngine[10][MAX_TIPS_LEN+1];

	int cursor_h;
	double CandPosX[33];
	double CandPosY[33];
	uint16_t CandWidth[10];
	uint16_t CandHeight[10];
	int BihuaMode;
	int EnglishMode;
	int ChinglishMode;
	int SelectMode;
	int StopInput;
#if defined(_WIN32) || defined(CFG_XIM_ANDROID)
	uint16_t Page[32];
#else
	char Page[32];
#endif
	double PageLen;
	double PagePosX;
	double PagePosY;
	double CodePos[4];
	
	Y_LAYOUT *layout;
}IM;

/* this one defined at main.c */
extern IM im;

int InitExtraIM(IM *im,EXTRA_IM *eim,const char *arg);
int LoadExtraIM(IM *im,const char *fn);
const char *YongFullChar(int key);
const char *YongGetPunc(int key,int bd,int peek);
EXTRA_IM *YongCurrentIM(void);

#define CURRENT_EIM YongCurrentIM

#endif/*_FCITX_H_*/
