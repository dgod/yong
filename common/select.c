#include "im.h"
#include "yong.h"

#include <stdio.h>
#include <string.h>
#include "llib.h"
#include "common.h"

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

static LPtrArray *cands;

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
	if(cands)
	{
		l_ptr_array_free(cands,l_free);
		cands=NULL;
	}
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
	if(cands)
	{
		if(mode==PAGE_ASSOC)
			return IMR_PASS;
		if(mode==PAGE_FIRST)
			EIM.CurCandPage=0;
		else if(mode==PAGE_NEXT && EIM.CurCandPage+1<EIM.CandPageCount)
			EIM.CurCandPage++;
		else if(mode==PAGE_PREV && EIM.CurCandPage>0)
			EIM.CurCandPage--;
		if(EIM.CurCandPage<EIM.CandPageCount-1)
			EIM.CandWordCount=EIM.CandWordMax;
		else
			EIM.CandWordCount=EIM.CandWordTotal-EIM.CandWordMax*(EIM.CandPageCount-1);
		EIM.CandWordMaxReal=EIM.CandWordMax;
		int first=EIM.CurCandPage*EIM.CandWordMax;
		for(int i=0;i<EIM.CandWordCount;i++)
		{
			const char *s=l_ptr_array_nth(cands,first+i);
			strcpy(EIM.CandTable[i],s);
		}
	}
	return IMR_DISPLAY;
}

static int SelDestroy(void)
{
	if(cands)
	{
		l_ptr_array_free(cands,l_free);
		cands=NULL;
	}
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

void y_select_set(LPtrArray *arr,const char *tip)
{
	SelReset();
	if(cands)
		l_ptr_array_free(cands,l_free);
	cands=arr;
	if(tip)
		strcpy(EIM.StringGet,tip);
	else
		EIM.StringGet[0]=0;
	EIM.CandWordMax=im.CandWord;
	EIM.CandWordTotal=l_ptr_array_length(arr);
	EIM.CandPageCount=EIM.CandWordTotal/EIM.CandWordMax+((EIM.CandWordTotal%EIM.CandWordMax)?1:0);
	EIM.SelectIndex=0;
	im.CodeInput[0]=0;
	y_im_str_encode(EIM.StringGet,im.StringGet,0);
	SelGetCandWords(PAGE_FIRST);
	im.SelectMode=1;
}

void *y_select_eim(void)
{
	return &EIM;
}
