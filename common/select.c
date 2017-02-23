#include "im.h"
#include "yong.h"

#include <stdio.h>
#include <string.h>

static int SelInit(const char *arg);
static void SelReset(void);
static char *SelGetCandWord(int index);
static int SelGetCandWords(int mode);
static int SelDestroy(void);
static int SelDoInput(int key);

static EXTRA_IM EIM={
	"select",
	.Reset			=	SelReset,
	.DoInput		=	SelDoInput,
	.GetCandWords	=	SelGetCandWords,
	.GetCandWord	=	SelGetCandWord,
	.Init			=	SelInit,
	.Destroy		=	SelDestroy,
};

static int SelInit(const char *arg)
{
	return 0;
}

static void SelReset(void)
{
	EIM.CodeInput[0]=0;
	EIM.StringGet[0]=0;
	EIM.CodeLen=0;
	EIM.CurCandPage=EIM.CandPageCount=EIM.CandWordCount=0;
	EIM.SelectIndex=0;
	im.SelectMode=0;
}

static char *SelGetCandWord(int index)
{
	char *ret;
	if(index==-1) index=EIM.SelectIndex;
	if(index>=EIM.CandWordCount) return 0;
	ret=&EIM.CandTable[index][0];
	strcpy(EIM.StringGet,ret);
	return ret;
}

static int SelGetCandWords(int mode)
{
	return IMR_DISPLAY;
}

static int SelDestroy(void)
{
	return 0;
}

static int SelDoInput(int key)
{
	if(key==YK_BACKSPACE)
	{
		return IMR_CLEAN;
	}
	return IMR_NEXT;
}

void *y_select_eim(void)
{
	return &EIM;
}
