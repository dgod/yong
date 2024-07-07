#include <ctype.h>
#include <inttypes.h>
#include <time.h>

#include "yong.h"
#include "common.h"
#include "english.h"

static int EnglishInit(const char *arg);
static void EnglishReset(void);
static char *EnglishGetCandWord(int index);
static int EnglishGetCandWords(int mode);
static int EnglishDestroy(void);
static int EnglishDoInput(int key);

static int PhraseListCount;
static int key_temp_english;
static int en_commit_select;
static int en_degrade;
extern int key_commit;
extern int key_select[9];

static EXTRA_IM EIM={
	.Name			=	"english",
	.Reset			=	EnglishReset,
	.DoInput		=	EnglishDoInput,
	.GetCandWords	=	EnglishGetCandWords,
	.GetCandWord	=	EnglishGetCandWord,
	.Init			=	EnglishInit,
	.Destroy		=	EnglishDestroy
};

static int NumSet(const char *s);
static int NumGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_num={NumSet,NumGet};

static int CalcSet(const char *s);
static int CalcGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_calc={CalcSet,CalcGet};

static int UrlSet(const char *s);
static int UrlGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_url={UrlSet,UrlGet};

static int BdSet(const char *s);
static int BdGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_bd={BdSet,BdGet};

static int MoneySet(const char *s);
static int MoneyGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_money={MoneySet,MoneyGet};

static int HexSet(const char *s);
static int HexGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_hex={HexSet,HexGet};

static void DictLoad(void);
static void DictFree(void);
static int DictSet(const char *s);
static int DictGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_dict={DictSet,DictGet};

extern ENGLISH_IM eim_book;

static ENGLISH_IM *enim[]={
	&eim_num,&eim_calc,&eim_url,&eim_bd,&eim_money,&eim_book,&eim_hex,&eim_dict
};
#define ENIM_COUNT	(sizeof(enim)/sizeof(ENGLISH_IM*))

static int EnglishInit(const char *arg)
{
	key_temp_english=y_im_get_key("tEN",-1,YK_NONE);
	if(key_temp_english!=YK_NONE)
		key_temp_english=tolower(key_temp_english);
	en_commit_select=y_im_get_config_int("IM","en_commit_select");
	en_degrade=y_im_get_config_int("IM","en_degrade");
	if(en_degrade)
		en_commit_select=1;
	EnglishReset();
	DictLoad();
	return 0;
}

static void EnglishReset(void)
{
	int i;
	
	PhraseListCount=0;
	EIM.CandWordTotal=0;
	EIM.CaretPos=0;
	EIM.CodeInput[0]=0;
	EIM.CodeLen=0;
	EIM.CurCandPage=EIM.CandPageCount=EIM.CandWordCount=0;
	EIM.SelectIndex=0;
	
	for(i=0;i<ENIM_COUNT;i++){
		ENGLISH_IM *e=enim[i];
		e->Count=0;
	}
}

static int AutoCompleteByDict(int index)
{
	int check=0;
	if(index==YK_SPACE)
	{
		check=1;
		index=EIM.SelectIndex;
	}
	if(EIM.CandWordCount==0 || eim_dict.Count==0)
		return 0;
	char *phrase=&EIM.CandTable[index][0];
	int pos=EIM.CandWordMaxReal*EIM.CurCandPage+index;
	int i;
	
	for(i=0;i<ENIM_COUNT;i++)
	{
		ENGLISH_IM *e=enim[i];
		int ec=e->Count;
		if(ec==0)
			continue;
		if(e!=&eim_dict)
		{
			pos-=ec;
			if(pos<0)
				return 0;
		}
		else
		{
			if(pos>=e->Count)
				return 0;
			if(check)
				return 1;
			char *p=LINT_TO_PTR(e->Priv1);
			int len=strlen(p);
			if(strlen(phrase)<=len)
				return 2;
			strcat(p,phrase+len);
			EIM.CaretPos=EIM.CodeLen=strlen(EIM.CodeInput);
			return 1;
		}
	}
	return 0;
}

static char *EnglishGetCandWord(int index)
{
	char *ret;

	if(index>=EIM.CandWordCount)
		return 0;
	if(index==-1)
		index=EIM.SelectIndex;
	ret=&EIM.CandTable[index][0];
	if(!en_commit_select && AutoCompleteByDict(index))
	{
		return NULL;
	}
	strcpy(EIM.StringGet,ret);
	return ret;
}

static int EnglishGetCandWords(int mode)
{
	int max;
	int i,count,start;
	
	if(mode==PAGE_ASSOC)
		return IMR_BLOCK;
		
	max=EIM.CandWordMax;
	EIM.CandWordMaxReal=max;
	
	if(mode==PAGE_FIRST) EIM.CurCandPage=0;
	else if(mode==PAGE_NEXT && EIM.CurCandPage+1<EIM.CandPageCount)
		EIM.CurCandPage++;
	else if(mode==PAGE_PREV && EIM.CurCandPage>0)
		EIM.CurCandPage--;

	if(EIM.CandPageCount==0)
		EIM.CandWordCount=0;
	else if(EIM.CurCandPage<EIM.CandPageCount-1)
		EIM.CandWordCount=max;
	else EIM.CandWordCount=PhraseListCount-max*(EIM.CandPageCount-1);
	
	for(i=0;i<EIM.CandWordCount;i++)
		EIM.CodeTips[i][0]=0;
	start=EIM.CurCandPage*max;count=0;
	for(i=0;i<ENIM_COUNT && count<EIM.CandWordCount;i++)
	{
		ENGLISH_IM *e=enim[i];
		int ec=e->Count;
		int pos=0;
		while(start>0  && ec)
		{
			start--;
			ec--;
			pos++;
		}
		if(!ec) continue;
		ec=MIN(ec,EIM.CandWordCount-count);
		e->Get(&EIM.CandTable[count],pos,ec);
		count+=ec;
	}
	return IMR_DISPLAY;
}

static int EnglishDestroy(void)
{
	DictFree();
	return 0;
}

static int EnglishDoSearch(void)
{
	int i,max;
	char *s=EIM.CodeInput;
	
	PhraseListCount=0;
	EIM.CandWordTotal=0;
	if(EIM.CodeInput[0]==key_temp_english)
		s++;
	for(i=0;i<ENIM_COUNT;i++)
	{
		ENGLISH_IM *e=enim[i];
		e->Set(s);
		PhraseListCount+=e->Count;
	}
	EIM.CandWordTotal=PhraseListCount;
	max=EIM.CandWordMax;
	EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	EnglishGetCandWords(PAGE_FIRST);
	if(!key_temp_english || ((EIM.CodeInput[0]&0x7f)!=key_temp_english))
		strcpy(EIM.StringGet,"> ");
	return PhraseListCount;
}

static int key_is_select(int key)
{
	if(key==key_commit)
		return 1;
	for(int i=0;i<9;i++)
	{
		if(key==key_select[i])
			return 1;
	}
	return 0;
}

static int EnglishDoInput(int key)
{
	int i;
	
	key&=~KEYM_BING;
	if(key==YK_SPACE)
	{
		if(en_degrade)
			return IMR_NEXT;
		if(EIM.CandWordCount && !AutoCompleteByDict(YK_SPACE))
			return IMR_NEXT;
	}
	if(key==YK_BACKSPACE)
	{
		if(EIM.CodeLen==0)
			return IMR_CLEAN_PASS;
		if(EIM.CaretPos==0)
			return IMR_BLOCK;
		for(i=EIM.CaretPos;i<EIM.CodeLen;i++)
			EIM.CodeInput[i-1]=EIM.CodeInput[i];
		EIM.CodeLen--;
		EIM.CaretPos--;
		EIM.CodeInput[EIM.CodeLen]=0;
		if(EIM.CodeLen==0)
			return IMR_CLEAN;
	}
	else if(key==YK_DELETE)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		if(EIM.CodeLen!=EIM.CaretPos)
		{
			for(i=EIM.CaretPos;i<EIM.CodeLen;i++)
				EIM.CodeInput[i]=EIM.CodeInput[i+1];
			EIM.CodeLen--;
			EIM.CodeInput[EIM.CodeLen]=0;
			if(EIM.CodeLen==0)
				return IMR_CLEAN;
		}
	}
	else if(key==YK_HOME)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		EIM.CaretPos=(EIM.CodeInput[0]==key_temp_english)?1:0;
	}
	else if(key==YK_END)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		EIM.CaretPos=EIM.CodeLen;
	}
	else if(key==YK_LEFT)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		if(EIM.CaretPos==0)
			return IMR_DISPLAY;
		if(EIM.CaretPos>1 || (EIM.CaretPos==1 && EIM.CodeInput[EIM.CaretPos-1]!=key_temp_english))
			EIM.CaretPos--;
	}
	else if(key==YK_RIGHT)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		if(EIM.CaretPos<EIM.CodeLen)
			EIM.CaretPos++;
	}
	else if(key>=0x20 && key<0x80)
	{
		if(en_degrade && EIM.CodeLen>0 && key_is_select(key))
			return IMR_NEXT;
		if(en_degrade==2 && key>='0' && key<='9')
			return IMR_NEXT;
		if(EIM.CodeLen>=MAX_CODE_LEN)
			return IMR_BLOCK;
		if(EIM.CodeLen==0)
		{
			EIM.CaretPos=0;
			EIM.StringGet[0]=0;
			EIM.SelectIndex=0;
		}
		for(i=EIM.CodeLen-1;i>=EIM.CaretPos;i--)
			EIM.CodeInput[i+1]=EIM.CodeInput[i];
		EIM.CodeInput[EIM.CaretPos]=key;
		EIM.CodeLen++;
		EIM.CaretPos++;
		EIM.CodeInput[EIM.CodeLen]=0;
	}
	else if(key==YK_VIRT_REFRESH)
	{
	}
	else
	{
		return IMR_NEXT;
	}
	EnglishDoSearch();
	return IMR_DISPLAY;
}

void y_english_init(void)
{
}

void y_english_destroy(void)
{
}

void *y_english_eim(void)
{
	return &EIM;
}

/*
 * english's num translate
 */

static char *ch_0[]={"〇","零"};
static char *ch_19[]={"一二三四五六七八九","壹贰叁肆伍陆柒捌玖"};
static char *cht_19[]={"一二三四五六七八九","壹貳參肆伍陸柒捌玖"};

static int n_spec_count(const char *in,int spec)
{
	int c;
	int count=0;
	while((c=*in++)!=0)
	{
		if(c==spec) count++;
	}
	return count;
}

static int n_is_digit(const char *s,int len)
{
	int i;
	if(len>18)
		return 0;
	for(i=0;i<len;i++)
	{
		if(!isdigit(s[i]))
			return 0;
	}
	return 1;
}

static int n_is_hex(const char *s,int len)
{
	int i;
	if(len<=2 || len>18)
		return 0;
	if(s[0]!='0' || s[1]!='x')
		return 0;
	for(i=2;i<len;i++)
	{
		if(!isxdigit(s[i]))
			return 0;
	}
	return 1;
}

#define NEED_CH(avail) \
do{ \
	int c=*s++; \
	if(!c)		\
		goto out; \
	if(avail!=c) \
		goto out; \
}while(0)

#define NEED_INT(r0,r1)	\
do{	\
	long v=strtol(s,(char**)&s,10);	\
	if(v<r0 || v>r1)	\
		goto out;	\
}while(0)
	

static int n_is_date(const char *s,int len)
{
	int ret=0;
	if(len<5)
		goto out;

	NEED_INT(1000,2999);
	NEED_CH('.');
	if(strspn(s-1,"0123456789.")==strlen(s-1) && s[0]!='.' && n_spec_count(s,'.')<=1)
		ret=1;
out:
	return ret;
}

static int n_is_time(const char *s,int len)
{
	int ret=0;
	if(len<4)
		goto out;
	if(!isdigit(s[0]))
		goto out;
	NEED_INT(0,24);
	NEED_CH(':');
	NEED_INT(0,59);
	if(s[0])
	{
		if(s[0]!=':')
			return 0;
		s++;
		if(s[0])
			NEED_INT(0,59);
		if(s[0])
			return 0;
	}
	ret=1;
out:
	return ret;
}

static void parse_decimal(int sel,int64_t val,char *s);
static int NumSet(const char *s)
{
	ENGLISH_IM *e=&eim_num;
	int len;
	
	e->Priv2=(uintptr_t)s;	
	e->Count=0;
	e->Priv1=0;
	
	len=strlen(s);
	if(!len)
	{
		return 0;
	}
	
	if(!strcmp(s,"0"))
	{
		e->Count=2;
		e->Priv1=3;
	}
	else if(n_is_digit(s,len))
	{
		e->Count=2;
		e->Priv1=0;
		if(len>1)
			e->Count++;
	}
	else if(n_is_date(s,len))
	{
		int year=atoi(s);
		e->Count=2;
		e->Priv1=1;
		if(year>1901 && year<2100)
		{
			int year,month,day;
			char c1,c2;
			if(5==sscanf(s,"%d%c%d%c%d",&year,&c1,&month,&c2,&day))
				e->Count=3;
		}
	}
	else if(n_is_time(s,len))
	{
		e->Count=2;
		e->Priv1=2;
	}
	//printf("%d %d\n",e->Priv1,e->Count);
	return e->Count;
}

static int NumGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_num;
	if(e->Priv1==3)
	{
		for(int i=0;i<count;i++)
		{
			int which=pos+i;
			if(which==0)
				l_int_to_str(0,NULL,L_INT2STR_HZ|L_INT2STR_ZERO0,cand[i]);
			else
				l_int_to_str(0,NULL,L_INT2STR_HZ,cand[i]);
		}
		return 0;
	}
	if(e->Priv1==0)
	{
		const char *s=(const char*)e->Priv2;
		int len=strlen(s);
		int64_t val=strtoll(s,NULL,10);
		char format[16];
		sprintf(format,"%%0%d"PRId64,len);
		for(int i=0;i<count;i++)
		{
			int which=pos+i;
			if(which==0)
				l_int_to_str(val,format,L_INT2STR_HZ,cand[i]);
			else if(which==1)
				l_int_to_str(val,format,L_INT2STR_HZ|L_INT2STR_BIG,cand[i]);
			else
				l_int_to_str(val,NULL,L_INT2STR_INDIRECT,cand[i]);
		}
		return 0;
	}
	if(e->Priv1==1)
	{
		const char *s=(const char*)e->Priv2;
		int year,month,day;
		int len=sscanf(s,"%d.%d.%d",&year,&month,&day);
		for(int i=0;i<count;i++)
		{
			int which=pos+i;
			if(which==0 || which==1)
			{
				int pos=l_int_to_str(year,NULL,(which?0:L_INT2STR_HZ|L_INT2STR_ZERO0),cand[i]);
				strcpy(cand[i]+pos,"年");pos+=2;
				if(len>1)
				{
					pos+=l_int_to_str(month,NULL,(which?0:L_INT2STR_INDIRECT),cand[i]+pos);
					strcpy(cand[i]+pos,"月");pos+=2;
					if(len>2)
					{
						pos+=l_int_to_str(day,NULL,(which?0:L_INT2STR_INDIRECT),cand[i]+pos);
						strcpy(cand[i]+pos,"日");pos+=2;
					}
				}
			}
			else
			{
				struct tm tm;
				strcpy(cand[i],"农历");
				if(3==len)
				{
					memset(&tm,0,sizeof(tm));
					tm.tm_year=year-1900;
					tm.tm_mon=month-1;
					tm.tm_mday=day;
#if defined(_WIN32) && !defined(_WIN64)
					y_im_nl_day(_mktime64(&tm),cand[i]+strlen(cand[i]));
#else
					y_im_nl_day(mktime(&tm),cand[i]+strlen(cand[i]));
#endif
				}
			}
		}
		return 0;
	}
	if(e->Priv1==2)
	{
		const char *s=(const char *)e->Priv2;
		int h,m,sec;
		int len=sscanf(s,"%d:%d:%d",&h,&m,&sec);
		for(int i=0;i<count;i++)
		{
			int which=pos+i;
			if(which==0)
			{
				int pos=0;
				pos=l_int_to_str(h,NULL,L_INT2STR_INDIRECT,cand[i]);
				strcpy(cand[i]+pos,"点");
				pos+=2;
				pos+=l_int_to_str(m,NULL,L_INT2STR_HZ|L_INT2STR_MINSEC,cand[i]+pos);
				strcpy(cand[i]+pos,"分");
				pos+=2;
				if(len==3)
				{
					pos+=l_int_to_str(sec,NULL,L_INT2STR_HZ|L_INT2STR_MINSEC,cand[i]+pos);
					strcpy(cand[i]+pos,"秒");
				}
			}
			else
			{
				int pos=0;
				pos=l_int_to_str(h,"%d",0,cand[i]);
				strcpy(cand[i]+pos,"点");
				pos+=2;
				pos+=l_int_to_str(m,"%d",0,cand[i]+pos);
				strcpy(cand[i]+pos,"分");
				pos+=2;
				if(len==3)
				{
					pos+=l_int_to_str(sec,"%02d",0,cand[i]+pos);
					strcpy(cand[i]+pos,"秒");
				}
			}
		}
	}
	return 0;
}

static int y_expr_calc(const char *s,char *res,int len)
{
	LVariant var;
	if(!strpbrk(s,"+-*/%"))
	{
		return -1;
	}
	var=l_expr_calc(s);
	switch(var.type){
	case L_TYPE_INT:
		snprintf(res,len,"%ld",var.v_int);
		break;
	case L_TYPE_FLOAT:
	{
		snprintf(res,len,"%.7f",var.v_float);
		const char *dot=strchr(res,'.');
		if(dot)
		{
			char *p=res+strlen(res)-1;
			while(p>dot+1 && *p=='0')
			{
				*p--=0;
			}
		}
		break;
	}
	default:
		return -1;
	}
	return 0;
}

static char calc_res[64];
static int CalcSet(const char *s)
{
	ENGLISH_IM *e=&eim_calc;
	int ret;
	e->Count=0;
	if(!strpbrk(s,"0123456789"))
		return 0;
	if(strspn(s,"0123456789.+-*/%()")!=strlen(s))
		return 0;
	ret=y_expr_calc(s,calc_res,sizeof(calc_res));
	e->Count=ret?0:1;
	return e->Count;
}

static int CalcGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	strcpy(cand[0],calc_res);
	return 1;
}

static char url_input[64];
static int UrlSet(const char *s)
{
	Y_USER_URL *url=y_im_user_urls();
	ENGLISH_IM *e=&eim_url;
	Y_USER_URL *p;
	e->Count=0;
	if(strlen(s)<3)
		return 0;
	for(p=url;p;p=p->next)
	{
		if(strstr(p->url,s))
			e->Count++;
	}
	strcpy(url_input,s);
	return 0;
}

static int UrlGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	Y_USER_URL *url=y_im_user_urls();
	Y_USER_URL *p;
	int i;
	
	for(i=0,p=url;p && i<pos;p=p->next)
		i++;
	for(i=0;i<count && p;p=p->next)
	{
		if(strstr(p->url,url_input))
		{
			strcpy(cand[i],p->url);
			i++;
		}
	}
	return 0;
}

static int BdSet(const char *s)
{
	ENGLISH_IM *e=&eim_bd;
	CONNECT_ID *id;
	id=y_xim_get_connect();
	e->Count=0;
	if(!id || strlen(s)!=0)
		return 0;
	if(id->biaodian==LANG_CN)
	{
		const char *p1;
		p1=YongGetPunc(s[-1],LANG_CN,1);
		if(p1)
		{
			e->Count++;
		}
	}
	else
	{
		if(YongGetPunc(s[-1],LANG_EN,1))
			e->Count++;
	}
	e->Priv1=s[-1];
	return 0;
}

static int BdGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_bd;
	CONNECT_ID *id;
	int lang=LANG_CN;

	id=y_xim_get_connect();
	if(id && id->biaodian==LANG_EN)
		lang=LANG_EN;
		
	if(count>0 && lang==LANG_EN)
	{
		const char *p=YongGetPunc((int)e->Priv1,LANG_EN,1);
		strcpy(cand[0],p);
	}
	else if(count>0 && lang==LANG_CN)
	{
		sprintf(cand[0],"$BD(%c)",(char)e->Priv1);
		count--;
	}

	return 0;
}

static int MoneySet(const char *s)
{
	ENGLISH_IM *e=&eim_money;
	int ret,pos=0;
	int64_t n1;
	uint8_t n2=10,n3=10;
	e->Count=0;
	ret=l_sscanf(s,"%"PRId64".%c%c%n",&n1,&n2,&n3,&pos);
	if(ret<1 || (ret==3 && s[pos]) ||
			n_spec_count(s,'.')>1 ||
			strspn(s,"01234567890.")!=strlen(s))
	{
		return 0;
	}
	if(ret==1 && n1==0)
	{
		return 0;
	}
	if(n1<0)
	{
		return 0;
	}
	if(isdigit(n2))
		n2-='0';
	if(isdigit(n3))
		n3-='0';
	e->Priv1=n1;
	e->Priv2=n2<<8|n3;
	e->Count=1;
	return e->Count;
}

static void parse_decimal(int sel,int64_t val,char *s)
{
	int flags=L_INT2STR_INDIRECT;
	if(im.Trad)
		flags|=L_INT2STR_TRAD;
	if(sel)
		flags|=L_INT2STR_BIG;
	l_int_to_str(val,NULL,flags,s);
}

/*
 * 测试例子
 * 101,100001,208,2008,120000,101101,0.34，0.04
 */
static int MoneyGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_money;
	char *s;
	int64_t yuan;
	int jiao,fen;
	if(count<=0)
		return 0;
	s=cand[0];s[0]=0;
	yuan=(int64_t)e->Priv1;jiao=e->Priv2>>8;fen=e->Priv2&0xff;
	if(yuan)
	{
		parse_decimal(1,yuan,s);
		strcat(s,"元");
	}
	if(jiao<10 && !(jiao==0 && fen==0) && !(jiao==0 && fen==10))
	{
		const char *num=(im.Trad?cht_19:ch_19)[1];
		int len=strlen(s);
		if(jiao)
		{
			memcpy(s+len,num+(jiao-1)*2,2);
			len+=2;
		}
		else if(yuan!=0)
		{
			strcpy(s+len,ch_0[1]);
			len+=2;
		}
		s[len]=0;
		if(jiao>0)
		{
			strcpy(s+len,"角");
			len+=2;
		}
		if(fen<10)
		{
			if(fen)
			{
				memcpy(s+len,num+(fen-1)*2,2);
				len+=2;
				s[len]=0;
				strcpy(s+len,"分");
			}
			else
			{
				strcat(s,"整");
			}
		}
		else
		{
			strcat(s,"整");
		}
	}
	else
	{
		strcat(s,"整");
	}
	return 0;
}

static int HexSet(const char *s)
{
	ENGLISH_IM *e=&eim_hex;
	int len;
	
	len=strlen(s);
	if(!len)
	{
		return 0;
	}
	if(n_is_digit(s,len))
	{
		e->Priv2=(uintptr_t)s;	
		e->Priv1=0;
		e->Count=1;
	}
	else if(n_is_hex(s,len))
	{
		e->Priv2=(uintptr_t)s;	
		e->Priv1=1;
		e->Count=1;
	}
	else
	{
		e->Count=0;
		return 0;
	}
	return 1;
}

static int HexGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_hex;
	char *s=(char*)e->Priv2;
	uint32_t temp;
	if(e->Priv1==0)
	{
		temp=(uint32_t)strtoul(s,NULL,10);
		sprintf(cand[0],"0x%"PRIx32,temp);
	}
	else
	{
		temp=(uint32_t)strtoul(s+2,NULL,16);
		sprintf(cand[0],"%"PRIu32,temp);
	}
	return 0;
}

void y_english_key_desc(const char *code,char *res)
{
	int ipos=0,opos=0;
	if(key_temp_english && code[0]==key_temp_english)
	{
		y_im_key_desc_first(key_temp_english,1,res,16);
		opos=strlen(res);
		ipos=1;
	}
	if(!strncasecmp(code+ipos,"key ",4))
	{
		//strcpy(res+pos,"key ");
		memcpy(res+opos,code+ipos,4);
		res[opos+4]=0;
		ipos+=4;
		opos+=4;
	}
	else if(!strncasecmp(code+ipos,"miyao ",6))
	{
		memcpy(res+opos,code+ipos,6);
		res[opos+6]=0;
		ipos+=6;
		opos+=6;
	}
	else
	{
		strcpy(res+opos,code+ipos);
		return;
	}
	if(code[ipos]==' ')
	{
		res[opos]=' ';
		ipos++;
		opos++;
	}
	for(;code[ipos]!=0;ipos++,opos++)
	{
		res[opos]='*';
	}
	res[opos]=0;
}


typedef struct{
	char *content;
	LArray *index[26];
}Dict;
static Dict *dict;

static int DictPhraseCampare(const uint32_t *p1,const uint32_t *p2)
{
	const char *s1=dict->content+*p1;
	const char *s2=dict->content+*p2;
	return strcasecmp(s1,s2);
}

static inline int DictIndex(int i)
{
	if(i>='A' && i<='Z')
		i-='A';
	else if(i>='a' && i<='z')
		i-='a';
	else
		return -1;
	return i;
}

static void DictLoad(void)
{
	char *content,*p;
	int i;
	if(dict)
		return;
	content=l_file_get_contents("mb/english.txt",NULL,
				y_im_get_path("HOME"),
				y_im_get_path("DATA"),NULL);
	if(!content)
		return;
	p=strchr(content,']');
	if(!p)
		return;
	p++;
	while(isspace(*p))
		p++;
	dict=l_new(Dict);
	dict->content=content;
	for(i=0;i<26;i++)
	{
		dict->index[i]=l_array_new(1000,sizeof(uint32_t));
	}
	while(*p!='\0')
	{
		char line[128];
		uint32_t pos=(uint32_t)(size_t)(p-content);
		LArray *a;
		
		for(i=0;i<sizeof(line)-1 && *p!=0;)
		{
			int c=*p++;
			if(c=='\r')
			{
				p[-1]=0;
				continue;
			}
			if(c=='\n')
			{
				p[-1]=0;
				break;
			}
			line[i++]=c;
		}
		line[i]=0;
		i=DictIndex(line[0]);
		if(i==-1)
			continue;
		a=dict->index[i];
		if(a->len==0 || DictPhraseCampare(&pos,l_array_nth(a,a->len-1))>=0)
			l_array_append(a,&pos);
		else
			l_array_insert_sorted(a,&pos,(LCmpFunc)DictPhraseCampare);
	}
}

static void DictFree(void)
{
	int i;
	if(!dict)
		return;
	l_free(dict->content);
	for(i=0;i<26;i++)
	{
		l_array_free(dict->index[i],NULL);
	}
	l_free(dict);
}

static int DictSet(const char *s)
{
	int i;
	const char *p,*t;
	LArray *a;
	ENGLISH_IM *e=&eim_dict;
	e->Count=0;
	if(!dict)
		return 0;
	// 只有光标在最后的位置，才启用这个功能
	if(!(EIM.CaretPos==-1 || EIM.CaretPos==EIM.CodeLen))
	{
		return 0;
	}
	if(!strncasecmp(s,"key ",4) || !strncasecmp(s,"miyao ",6))
		return 0;
	// 寻找最后一个单词的开始位置
	p=s;
#if 0
	do{
		t=strchr(p,' ');
		if(!t)
			break;
		p=t+1;
	}while(1);
#else
	t=p+strlen(p)-1;
	while(t>p)
	{
		int c=*t;
		if(c>='A' && c<='Z' && !(t[-1]>='A' && t[-1]<='Z'))
		{
			p=t;
			break;
		}
		if(c==' ')
		{
			p=t+1;
			break;
		}
		t--;
	}
#endif
	// 检查最后一个字符串是否是纯英文字母组成
	for(t=p;*t!=0;t++)
	{
		if(*t&0x80)
			return 0;
		if(!isalpha(*t))
			return 0;
	}
	i=DictIndex(p[0]);
	if(i==-1)
		return 0;
	a=dict->index[i];
	
	int len=strlen(p);
	int left=-1;
	for(i=0;i<a->len;i++)
	{
		char *item=dict->content+*(uint32_t*)l_array_nth(a,i);
		if(!strncasecmp(p,item,len))
		{
			left=i;
			break;
		}
	}
	if(left==-1)
		return 0;
	for(i++;i<a->len;i++)
	{
		char *item=dict->content+*(uint32_t*)l_array_nth(a,i);
		if(strncasecmp(p,item,len))
			break;
	}
	e->Priv1=(uintptr_t)p;
	e->Priv2=left;
	e->Count=i-left;
	return e->Count;
}

static int DictGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_dict;
	if(e->Count==0)
		return 0;
	const char *p=(const char *)(size_t)e->Priv1;
	int i=DictIndex(p[0]);
	LArray *a=dict->index[i];
	uint32_t *left=l_array_nth(a,(int)e->Priv2+pos);
	for(i=0;i<count;i++)
	{
		strcpy(cand[i],dict->content+left[i]);
	}
	return 0;
}
