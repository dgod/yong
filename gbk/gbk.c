#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yong.h"
#include "llib.h"
#include "llineedit.h"

#define PhraseListCount EIM.CandWordTotal
static const char tip[16]="0123456789abcdef";

static int GbkInit(const char *arg);
static void GbkReset(void);
static char *GbkGetCandWord(int index);
static int GbkGetCandWords(int mode);
static int GbkDestroy(void);
static int GbkDoInput(int key);

L_EXPORT(EXTRA_IM EIM)={
	"ÄÚÂë",
	.Reset			=	GbkReset,
	.DoInput		=	GbkDoInput,
	.GetCandWords	=	GbkGetCandWords,
	.GetCandWord	=	GbkGetCandWord,
	.Init			=	GbkInit,
	.Destroy		=	GbkDestroy,
};

static LLineEdit line;

static int GbkInit(const char *arg)
{
	l_line_edit_init(&line);
	l_line_edit_set_max(&line,9);
	l_line_edit_set_allow(&line,tip,true);
	l_line_edit_set_allow(&line,"u",false);
	l_line_edit_set_nav(&line,YK_LEFT,YK_RIGHT,YK_HOME,YK_END);
	return 0;
}

static void GbkReset(void)
{
	l_line_edit_clear(&line);
	EIM.CandWordTotal=0;
	EIM.CodeInput[0]=0;
	EIM.StringGet[0]=0;
	EIM.CodeLen=0;
	EIM.CurCandPage=EIM.CandPageCount=EIM.CandWordCount=0;
	EIM.SelectIndex=0;
}

static char *GbkGetCandWord(int index)
{
	char *ret;
	if(index>=EIM.CandWordCount || index<-1)
		return 0;
	if(index==-1)
		index=EIM.SelectIndex;
	ret=&EIM.CandTable[index][0];
	strcpy(EIM.StringGet,ret);
	return ret;
}

static int GbkGetCandWords(int mode)
{
	int i;
	
	if(mode==PAGE_ASSOC)
		return IMR_PASS;

	if(EIM.CandPageCount==0)
		return IMR_PASS;
	if(mode==PAGE_FIRST) EIM.CurCandPage=0;
	else if(mode==PAGE_NEXT && EIM.CurCandPage+1<EIM.CandPageCount)
		EIM.CurCandPage++;
	else if(mode==PAGE_PREV && EIM.CurCandPage>0)
		EIM.CurCandPage--;

	if(EIM.CurCandPage<EIM.CandPageCount-1)
		EIM.CandWordCount=EIM.CandWordMax;
	else
		EIM.CandWordCount=PhraseListCount-EIM.CandWordMax*(EIM.CandPageCount-1);
	EIM.CandWordMaxReal=EIM.CandWordMax;

	for(i=0;i<EIM.CandWordMax;i++)
	{
		EIM.CodeTips[i][1]=EIM.CodeTips[i][0]=0;
	}

	if(EIM.CodeLen>=4 || EIM.CodeLen==2)
	{		
		if(PhraseListCount>0)
		{
			l_hex2bin(EIM.CandTable[0],EIM.CodeInput);
			EIM.CandTable[0][EIM.CodeLen/2]=0;
		}
	}
	else if(EIM.CodeLen==3)
	{
		uint32_t t=strtoul(EIM.CodeInput,0,16)<<4;
		int start=EIM.CandWordMax*EIM.CurCandPage;
		for(i=0;i<EIM.CandWordCount;i++)
		{
			uint32_t tmp=t|(start+i);
			EIM.CandTable[i][0]=(char)(tmp>>8);
			EIM.CandTable[i][1]=(char)(tmp&0xff);
			EIM.CandTable[i][2]=0;
			EIM.CodeTips[i][0]=tip[start+i];
		}
	}
	else if(EIM.CodeLen==1)
	{
		long t=strtol(EIM.CodeInput,0,16)<<4;
		int start=EIM.CandWordMax*EIM.CurCandPage;
		for(i=0;i<EIM.CandWordCount;i++)
		{
			EIM.CandTable[i][0]=(char)(t|(start+i));
			EIM.CandTable[i][1]=0;
			EIM.CodeTips[i][0]=tip[start+i];
		}
	}
	EIM.SelectIndex=0;

	return IMR_DISPLAY;
}

static int GbkDestroy(void)
{
	return 0;
}

static int GbkOnVirtQuery(const char *ph)
{
	if(!ph)
		return IMR_BLOCK;
	int len=l_gb_strlen(ph,-1);
	if(len!= 1)
		return IMR_BLOCK;
	strcpy(EIM.StringGet,EIM.Translate("·´²é£º"));
	EIM.CodeTips[0][0]=EIM.CodeTips[1][0]=EIM.CodeTips[2][0]=0;
	strcpy(EIM.CodeInput,ph);
	uint32_t ch=l_gb_to_char((const uint8_t*)ph);
	sprintf(EIM.CandTable[0],"%x",ch);
	EIM.CandWordCount=1;
	if(ch>=0x80)
	{
		uint32_t uch=l_gb_to_unichar((const uint8_t*)ph);
		sprintf(&EIM.CandTable[1][0],"u+%04x",uch);
		EIM.CandWordCount++;

		uint8_t temp[8];
		l_gb_to_utf8(ph,(char*)temp,sizeof(temp));
		l_bin2hex(EIM.CandTable[2],temp,strlen((char*)temp));
		EIM.CandWordCount++;
	}
	if(EIM.CandWordCount>0)
	{
		EIM.CandPageCount=1;
		EIM.CurCandPage=0;
		return IMR_DISPLAY;
	}

	return IMR_BLOCK;
}

static int GbkDoInput(int key)
{
	int ret=l_line_edit_push(&line,key);
	if(ret==1)
	{
		const char *s=l_line_edit_get_text(&line);
		if(l_re_test("^u?[a-f0-9]{0,8}$",s)<0)
		{
			l_line_edit_undo(&line);
			return IMR_BLOCK;
		}
		EIM.CodeLen=l_line_edit_copy(&line,EIM.CodeInput,-1,&EIM.CaretPos);
		if(EIM.CodeLen==0)
			return IMR_CLEAN;
		else
			ret=IMR_DISPLAY;
	}
	else if(ret==0)
	{
		if(key==YK_VIRT_QUERY)
		{
			char *ph;
			if(EIM.CodeLen || !EIM.GetSelect)
				return IMR_BLOCK;
			ph=EIM.GetSelect(GbkOnVirtQuery);
			if(ph)
				return GbkOnVirtQuery(ph);
			else
				return IMR_BLOCK;
		}
		return IMR_NEXT;
	}
	else
	{
		ret=IMR_BLOCK;
	}
	if(EIM.CodeLen>0 && EIM.CodeInput[0]=='u')
	{
		int len;
		PhraseListCount=0;
		EIM.CandPageCount=0;
		EIM.CandWordCount=0;
		if(EIM.CodeLen>1)
		{
			uint32_t c=strtoul(EIM.CodeInput+1,0,16);
			if(c>=0x20 && c<0x10FFFF)
			{
				l_unichar_to_gb0(c,(uint8_t*)EIM.CandTable[0]);
				if(EIM.CandTable[0][0])
				{
					PhraseListCount=1;
					EIM.CandPageCount=1;
					EIM.CandWordCount=1;
				}
			}
			len=strlen(EIM.CodeInput+1);
			if(len>=4 && !(len&0x01))
			{
				uint8_t temp[8];
				l_hex2bin(temp,EIM.CodeInput+1);
				temp[len/2]=0;
				if(l_utf8_strlen(temp,-1)==1)
				{
					l_utf8_to_gb((char*)temp,EIM.CandTable[PhraseListCount],8);
					if(EIM.CandTable[PhraseListCount][0]!=0)
					{
						PhraseListCount++;
						EIM.CandWordCount++;
					}
				}
			}
		}
	}
	else if(EIM.CodeLen>4)
	{
		if(EIM.CodeLen==8)
			l_hex2bin(EIM.CandTable[0],EIM.CodeInput);
		if(EIM.CodeLen!=8 || !gb_is_gb18030_ext((uint8_t*)EIM.CandTable[0]))
		{
			PhraseListCount=0;
			EIM.CandPageCount=0;
			EIM.CandWordCount=0;
		}
		else
		{
			PhraseListCount=1;
			EIM.CandPageCount=1;
			EIM.CandWordCount=1;
		}
		GbkGetCandWords(PAGE_FIRST);
	}
	else if(EIM.CodeLen==4)
	{
		l_hex2bin(EIM.CandTable[0],EIM.CodeInput);
		if(gb_is_gbk(EIM.CandTable[0]))
		{
			PhraseListCount=1;
			EIM.CandPageCount=1;
			EIM.CandWordCount=1;
			GbkGetCandWords(PAGE_FIRST);
		}
		else
		{
			PhraseListCount=0;
			EIM.CandPageCount=0;
			EIM.CandWordCount=0;
		}
	}
	else if(EIM.CodeLen==3)
	{
		char tmp[3];
		tmp[0]=EIM.CodeInput[0];tmp[1]=EIM.CodeInput[1];tmp[2]=0;
		uint8_t high=(uint8_t)strtol(tmp,0,16);
		uint8_t low=EIM.CodeInput[2];
		if(high>=0x81 && high<=0xfe && low>='4' && low<='f')
		{
			PhraseListCount=((low=='7' || low=='f')?15:16);
			EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+((PhraseListCount%EIM.CandWordMax)?1:0);
			GbkGetCandWords(PAGE_FIRST);
		}
		else
		{
			PhraseListCount=0;
			EIM.CandPageCount=0;
			EIM.CandWordCount=0;
		}
	}
	else if(EIM.CodeLen==2)
	{
		char code=(char)strtol(EIM.CodeInput,0,16);
		if((code&0x80)==0 && code>=0x20 && code<=0x7e)
		{
			PhraseListCount=1;
			EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+((PhraseListCount%EIM.CandWordMax)?1:0);
			GbkGetCandWords(PAGE_FIRST);
		}
		else
		{
			PhraseListCount=0;
			EIM.CandPageCount=0;
			EIM.CandWordCount=0;
		}
	}
	else if(EIM.CodeLen==1)
	{
		char code=EIM.CodeInput[0];
		if(code>='2' && code<='7')
		{
			PhraseListCount=((code=='7')?15:16);
			EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+((PhraseListCount%EIM.CandWordMax)?1:0);
			GbkGetCandWords(PAGE_FIRST);
		}
		else
		{
			PhraseListCount=0;
			EIM.CandPageCount=0;
			EIM.CandWordCount=0;
		}
	}
	else
	{
		PhraseListCount=0;
		EIM.CandPageCount=0;
		EIM.CandWordCount=0;
	}
	return ret;
}
