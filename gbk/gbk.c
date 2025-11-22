#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YONG_IM_ENGINE
#include "yong.h"
#include "gbk.h"
#include "llib.h"

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

static int GbkInit(const char *arg)
{
	return 0;
}

static void GbkReset(void)
{
	EIM.CandWordTotal=0;
	CodeInput[0]=0;
	StringGet[0]=0;
	CodeLen=0;
	CurCandPage=CandPageCount=CandWordCount=0;
	SelectIndex=0;
}

static char *GbkGetCandWord(int index)
{
	char *ret;
	if(index>=CandWordCount || index<-1)
		return 0;
	if(index==-1)
		index=SelectIndex;
	ret=&CandTable[index][0];
	strcpy(StringGet,ret);
	return ret;
}

static int GbkGetCandWords(int mode)
{
	int i;
	
	if(mode==PAGE_ASSOC)
		return IMR_PASS;

	if(CandPageCount==0)
		return IMR_PASS;
	if(mode==PAGE_FIRST) CurCandPage=0;
	else if(mode==PAGE_NEXT && CurCandPage+1<CandPageCount)
		CurCandPage++;
	else if(mode==PAGE_PREV && CurCandPage>0)
		CurCandPage--;

	if(CurCandPage<CandPageCount-1)
		CandWordCount=CandWordMax;
	else
		CandWordCount=PhraseListCount-CandWordMax*(CandPageCount-1);
	EIM.CandWordMaxReal=CandWordMax;

	for(i=0;i<CandWordMax;i++)
	{
		CodeTips[i][1]=CodeTips[i][0]=0;
	}

	if(CodeLen>4)
	{		
		CodeTips[0][0]=0;
		if(PhraseListCount>0)
		{
			unsigned long code=strtoul(CodeInput,0,16);
			for(i=0;i<4;i++)
				CandTable[0][i]=(char)((code>>((3-i)*8)) & 0xff);
			CandTable[0][4]=0;
		}
		else
		{
			CandTable[0][0]=0;
		}
	}
	else if(CodeLen==4)
	{
		long code=strtol(CodeInput,0,16);
		unsigned char high=code>>8&0xff,low=code&0xff;

		CandTable[0][0]=(char)high;
		CandTable[0][1]=(char)low;
		CandTable[0][2]=0;
	}
	else if(CodeLen==3)
	{
		long t=strtol(CodeInput,0,16)<<4;
		int start=CandWordMax*CurCandPage;
		for(i=0;i<CandWordCount;i++)
		{
			long tmp=t|(start+i);
			CandTable[i][0]=(char)(tmp>>8);
			CandTable[i][1]=(char)(tmp&0xff);
			CandTable[i][2]=0;
			CodeTips[i][0]=tip[start+i];
		}
	}
	else if(CodeLen==2)
	{
		char code=(char)strtol(CodeInput,0,16);
		CandTable[0][0]=code;
		CandTable[0][1]=0;
	}
	else if(CodeLen==1)
	{
		long t=strtol(CodeInput,0,16)<<4;
		int start=CandWordMax*CurCandPage;
		for(i=0;i<CandWordCount;i++)
		{
			CandTable[i][0]=(char)(t|(start+i));
			CandTable[i][1]=0;
			CodeTips[i][0]=tip[start+i];
		}
	}
	SelectIndex=0;

	return IMR_DISPLAY;
}

static int GbkDestroy(void)
{
	return 0;
}

static int GbkOnVirtQuery(const char *ph)
{
	int len;
	if(!ph)
		return IMR_BLOCK;
	len=strlen(ph);
	if(len!= 1 && len!=2 && len!=4)
		return IMR_BLOCK;
	strcpy(StringGet,EIM.Translate("·´²é£º"));
	CandWordCount=0;
	if(len==4)
	{
		uint8_t *p=(uint8_t*)ph;
		if(!gb_is_gb18030_ext(p))
				return IMR_BLOCK;
			
		CodeTips[0][0]=0;
		strcpy(CodeInput,ph);
		sprintf(CandTable[0],"%08x",l_read_u32be(p));
		CandWordCount=1;
	}			
	if(len==2)
	{
		unsigned char high,low;
		high=ph[0];low=ph[1];
		if(high>=0x81 && high<=0xfe && high!=0x7f && 
			low>=0x40 && low<=0xfe && low!=0x7f)
		{
			CodeTips[0][0]=0;
			strcpy(CodeInput,ph);
			sprintf(CandTable[0],"%02x%02x",high,low);
			CandWordCount=1;
		}
	}
	else
	{
		char code=ph[0];
		if(code>=0x20 && code<=0x7e)
		{
			CodeTips[0][0]=0;
			strcpy(CodeInput,ph);
			sprintf(&CandTable[0][0],"%02x",code);
			CandWordCount=1;
		}
	}
	if((len==2 || len==4) && CandWordCount==1)
	{
		uint16_t temp[8];
		temp[0]=temp[1]=0;
		l_gb_to_utf16(ph,temp,sizeof(temp));
		if(temp[0])
		{
			CodeTips[1][0]=0;
			if(temp[1]==0)
			{
				sprintf(&CandTable[1][0],"u+%04x",temp[0]);
			}
			else
			{
				uint32_t X,W,U,C;
				uint16_t hi=temp[0],lo=temp[1];
				X = ((hi & ((1 << 6) -1)) << 10) | (lo & ((1 << 10) -1));
				W = (hi >> 6) & ((1 << 5) - 1);
				U = W + 1;
				C = U << 16 | X;
				sprintf(&CandTable[1][0],"u+%05x",C);
			}
			CandWordCount++;
		}
	}
	if((len==2 || len==4) && CandWordCount==2)
	{
		uint8_t temp[8];
		temp[0]=temp[1]=0;
		l_gb_to_utf8(ph,(char*)temp,sizeof(temp));
		if(temp[0])
		{
			char *p=CandTable[2];
			int i;
			CodeTips[2][0]=0;
			strcpy(p,"u8+");p+=3;
			for(i=0;temp[i]!=0;i++)
			{
				sprintf(p+2*i,"%02x",temp[i]);
			}
			CandWordCount++;
		}
	}
	if(CandWordCount>0)
	{
		CandPageCount=1;
		CurCandPage=0;
		return IMR_DISPLAY;
	}

	return IMR_BLOCK;
}

static int GbkDoInput(int key)
{
	int ret=IMR_DISPLAY;
	int i;
	
	switch(key){
	case YK_BACKSPACE:
	{
		if(CodeLen==0)
			return IMR_CLEAN_PASS;
		if(CaretPos==0)
			return IMR_BLOCK;
		if(CaretPos==1 && CodeInput[0]=='u' && CodeLen!=1)
			return IMR_BLOCK;
		for(i=CaretPos;i<CodeLen;i++)
			CodeInput[i-1]=CodeInput[i];
		CodeLen--;
		CaretPos--;
		CodeInput[CodeLen]=0;
		break;
	}
	case YK_DELETE:
	{
		if(CodeLen==0)
			return IMR_PASS;
		if(CodeLen==CaretPos)
			return IMR_BLOCK;
		if(CaretPos==0 && CodeInput[0]=='u')
			return IMR_BLOCK;
		for(i=CaretPos;i<CodeLen;i++)
			CodeInput[i]=CodeInput[i+1];
		CodeLen--;
		CodeInput[CodeLen]=0;
		break;
	}
	case YK_HOME:
	{
		if(CodeLen==0)
			return IMR_PASS;
		CaretPos=0;
		break;
	}
	case YK_END:
	{
		if(CodeLen==0)
			return IMR_PASS;
		CaretPos=CodeLen;
		break;
	}
	case YK_LEFT:
	{
		if(CodeLen==0)
			return IMR_PASS;
		if(CaretPos>0)
			CaretPos--;
		break;
	}
	case YK_RIGHT:
	{
		if(CodeLen==0)
			return IMR_PASS;
		if(CaretPos<CodeLen)
			CaretPos++;
		break;
	}
	case '0'...'9':
	case 'a'...'f':
	{
		if(CodeLen==8)
			return IMR_BLOCK;
		if(CodeLen==0) CaretPos=0;
		if(CodeLen>0 && CaretPos==0 && CodeInput[0]=='u')
			return IMR_BLOCK;
		if(CodeLen>0 && CodeInput[0]=='u' && CodeLen>=7)
			return IMR_BLOCK;
		for(i=CodeLen-1;i>=CaretPos;i--)
			CodeInput[i+1]=CodeInput[i];
		CodeInput[CaretPos]=key;
		CodeLen++;
		CaretPos++;
		CodeInput[CodeLen]=0;
		break;
	}
	case 'u':
	{
		if(CodeLen!=0)
			return IMR_BLOCK;
		CodeInput[0]='u';
		CodeInput[1]=0;
		CodeLen=1;
		CaretPos=1;
		break;
	}
	case YK_VIRT_QUERY:
	{
		char *ph;
		if(CodeLen || !EIM.GetSelect)
			return IMR_BLOCK;
		ph=EIM.GetSelect(GbkOnVirtQuery);
		if(ph)
			return GbkOnVirtQuery(ph);
		else
			return IMR_BLOCK;
	}
	default:
	{
		return IMR_NEXT;
	}}
	if(CodeLen>0 && CodeInput[0]=='u')
	{
		int len;
		PhraseListCount=0;
		CandPageCount=0;
		CandWordCount=0;
		if(CodeLen>1)
		{
			uint32_t c=strtoul(CodeInput+1,0,16);
			if(c>=0x20 && c<0x10FFFF)
			{
				int len=l_unichar_to_gb(c,(uint8_t*)CandTable[0]);
				CandTable[0][len]=0;
				if(CandTable[0][0])
				{
					PhraseListCount=1;
					CandPageCount=1;
					CandWordCount=1;
				}
			}
			len=strlen(CodeInput+1);
			if(len>=5 && !(len&0x01))
			{
				uint8_t temp[8];
				int i;
				for(i=0,len=len/2;i<len;i++)
				{
					int val;
					sscanf(CodeInput+1+2*i,"%02x",&val);
					temp[i]=val;
				}
				temp[i]=0;
				if(i>0)
				{
					l_utf8_to_gb((char*)temp,CandTable[PhraseListCount],8);
					if(CandTable[PhraseListCount][0]!=0)
					{
						PhraseListCount++;
						CandWordCount++;
					}
				}
			}
		}
	}
	else if(CodeLen>4)
	{
		unsigned long code=strtoul(CodeInput,0,16);
		for(i=0;i<4;i++)
			CandTable[0][i]=(code>>((3-i)*8)) & 0xff;
		CandTable[0][4]=0;
		if(CodeLen<8 || !gb_is_gb18030_ext((uint8_t*)CandTable[0]))
		{
			PhraseListCount=0;
			CandPageCount=0;
			CandWordCount=0;
		}
		else
		{
			PhraseListCount=1;
			CandPageCount=1;
			CandWordCount=1;
		}
		GbkGetCandWords(PAGE_FIRST);
	}
	else if(CodeLen==4)
	{
		long code=strtol(CodeInput,0,16);
		unsigned char high=code>>8&0xff,low=code&0xff;
		if(high>=0x81 && high<=0xfe && high!=0x7f && 
				low>=0x40 && low<=0xfe && low!=0x7f)
		{
			PhraseListCount=1;
			CandPageCount=1;
			CandWordCount=1;
			GbkGetCandWords(PAGE_FIRST);
		}
		else
		{
			PhraseListCount=0;
			CandPageCount=0;
			CandWordCount=0;
		}
	}
	else if(CodeLen==3)
	{
		unsigned char high;
		char low;
		char tmp[3];
		tmp[0]=CodeInput[0];tmp[1]=CodeInput[1];tmp[2]=0;
		high=(unsigned char)strtol(tmp,0,16);
		low=CodeInput[2];
		if(high>=0x81 && high<=0xfe && low>='4' && low<='f')
		{
			PhraseListCount=((low=='7' || low=='f')?15:16);
			CandPageCount=PhraseListCount/CandWordMax+((PhraseListCount%CandWordMax)?1:0);
			GbkGetCandWords(PAGE_FIRST);
		}
		else
		{
			PhraseListCount=0;
			CandPageCount=0;
			CandWordCount=0;
		}
	}
	else if(CodeLen==2)
	{
		char code=(char)strtol(CodeInput,0,16);
		if((code&0x80)==0 && code>=0x20 && code<=0x7e)
		{
			PhraseListCount=1;
			CandPageCount=PhraseListCount/CandWordMax+((PhraseListCount%CandWordMax)?1:0);
			GbkGetCandWords(PAGE_FIRST);
		}
		else
		{
			PhraseListCount=0;
			CandPageCount=0;
			CandWordCount=0;
		}
	}
	else if(CodeLen==1)
	{
		char code=CodeInput[0];
		if(code>='2' && code<='7')
		{
			PhraseListCount=((code=='7')?15:16);
			CandPageCount=PhraseListCount/CandWordMax+((PhraseListCount%CandWordMax)?1:0);
			GbkGetCandWords(PAGE_FIRST);
		}
		else
		{
			PhraseListCount=0;
			CandPageCount=0;
			CandWordCount=0;
		}
	}
	else
	{
		PhraseListCount=0;
		CandPageCount=0;
		CandWordCount=0;
	}
	return ret;
}
