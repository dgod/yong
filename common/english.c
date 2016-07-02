#include <ctype.h>
#include <inttypes.h>
#include <time.h>

#include "yong.h"
#include "common.h"
#include "english.h"

static int EnglishInit(char *arg);
static void EnglishReset(void);
static char *EnglishGetCandWord(int index);
static int EnglishGetCandWords(int mode);
static int EnglishDestroy(void);
static int EnglishDoInput(int key);

static int PhraseListCount;
static int key_temp_english;

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

extern ENGLISH_IM eim_book;

static ENGLISH_IM *enim[]={
	&eim_num,&eim_calc,&eim_url,&eim_bd,&eim_money,&eim_book,&eim_hex
};
#define ENIM_COUNT	(sizeof(enim)/sizeof(ENGLISH_IM*))

static int EnglishInit(char *arg)
{
	key_temp_english=y_im_get_key("tEN",-1,YK_NONE);
	if(key_temp_english!=YK_NONE)
		key_temp_english=tolower(key_temp_english);
	EnglishReset();
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

static char *EnglishGetCandWord(int index)
{
	char *ret;

	if(index>=EIM.CandWordCount)
		return 0;
	if(index==-1)
		index=EIM.SelectIndex;
	ret=&EIM.CandTable[index][0];
	strcpy(EIM.StringGet,ret);
	return ret;
}

static int EnglishGetCandWords(int mode)
{
	int max;
	int i,count,start;
	
	if(mode==PAGE_LEGEND)
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

static int EnglishDoInput(int key)
{
	int i;
	
	key&=~KEYM_BING;
	if(key==YK_SPACE)
	{
		if(EIM.CandWordCount)
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
static char *ch_rq[]={"年月日"};
static char *ch_tm[]={"时点分秒"};
static char *cht_tm[]={"時點分秒"};
static char *ch_dw[]={"十百千万亿","拾佰仟万亿"};
static char *cht_dw[]={"十百千萬億","拾佰仟萬億"};

#define BIT_0(t)	(t)
#define BIT_19(t)	((t)<<2)
#define BIT_RQ(t)	((t)<<4)
#define BIT_DW(t)	((t)<<6)
#define BIT_TM(t)	((t)<<8)
#define BIT_INS(t)	((t)<<10)
#define GET_0(t)	((t)&3)
#define GET_19(t)	(((t)>>2)&3)
#define GET_RQ(t)	(((t)>>4)&3)
#define GET_DW(t)	(((t)>>6)&3)
#define GET_TM(t)	(((t)>>8)&3)
#define GET_INS(t)	(((t)>>10)&3)

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

static int num2hz(const char *in,char *out,int type)
{
	int i;
	int b,c;
	char temp[3];
	int num=0;
	int ins;
	int rq;
	
	ins=GET_INS(type);
	rq=GET_RQ(type);
	out[0]=0;
	
	for(i=0;(c=in[i])!=0;i++)
	{
		if(rq) ins=(i>=5);
		if(c=='0')
		{
			b=GET_0(type);
			if(b==0)
			{
				temp[0]=(char)c;
				temp[1]=0;
			}
			else
			{
				if(ins && num==1)
					strcpy(temp,"十");
				else
					strcpy(temp,ch_0[b-1]);
			}
			strcat(out,temp);
		}
		else if(c>='1' && c<='9')
		{
			b=GET_19(type);
			if(b==0)
			{
				temp[0]=(char)c;
				temp[1]=0;
			}
			else
			{
				if(ins && num==1)
				{
					strcpy(temp,"十");
					strcat(out,temp);
				}
				if(ins && c=='1' && isdigit(in[i+1]))
					temp[0]=0;
				else
				{
					if(im.Trad)
						memcpy(temp,&cht_19[b-1][2*(c-'1')],2);
					else
						memcpy(temp,&ch_19[b-1][2*(c-'1')],2);
					temp[2]=0;
				}
			}
			strcat(out,temp);
			num++;
		}
		else if((b=GET_DW(type))!=0 && strchr("subqwy",c))
		{
			switch(c){
			case 's':case'u':c=0;break;
			case 'b':c=1;break;
			case 'q':c=2;break;
			case 'w':c=3;break;
			case 'y':c=4;break;
			}
			if(im.Trad)
				memcpy(temp,&cht_dw[b-1][c*2],2);
			else
				memcpy(temp,&ch_dw[b-1][c*2],2);
			temp[2]=0;
			strcat(out,temp);
			num=0;
		}
		else if((b=GET_RQ(type))!=0 && strchr("nyr",c))
		{
			switch(c){
			case 'n':c=0;break;
			case 'y':c=1;break;
			case 'r':c=2;break;
			}
			memcpy(temp,&ch_rq[b-1][c*2],2);
			temp[2]=0;
			strcat(out,temp);
			num=0;
		}
		else if((b=GET_RQ(type))!=0 && c=='.')
		{
			if(i==4) c=0;
			else c=1;
			memcpy(temp,&ch_rq[b-1][c*2],2);
			temp[2]=0;
			strcat(out,temp);
			num=0;
		}
		else if((b=GET_TM(type))!=0 && strchr("sudfm",c))
		{
			switch(c){
			case 's':c=0;break;
			case 'u':c=0;break;
			case 'd':c=1;break;
			case 'f':c=2;break;
			case 'm':c=3;break;
			}
			if(im.Trad)
				memcpy(temp,&cht_tm[b-1][c*2],2);
			else
				memcpy(temp,&ch_tm[b-1][c*2],2);
			temp[2]=0;
			strcat(out,temp);
			num=0;
		}
	}
	if((b=GET_RQ(type))!=0 && n_spec_count(in,'.')==2 &&
			in[strlen(in)-1]!='.')
	{
		strcat(out,"日");
	}
	return 0;
}

static int n_is_digit(const char *s,int len)
{
	int i;
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
	if(len<=2)
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
		if(!c) goto out; \
		if(strchr((avail),c)==NULL) \
		goto out; \
	}while(0)
	
static int n_is_number(const char *s,int len)
{
	int ret=0;
	
	if(s[0]=='0') goto out;
	if(!strpbrk(s,"0123456789"))
		goto out;
	if(strspn(s,"0123456789subqwy")==strlen(s))
		ret=1;
out:
	return ret;
}

static int n_is_date(const char *s,int len)
{
	int ret=0;
	if(len<5)
		goto out;

	NEED_CH("123456789");
	NEED_CH("0123456789");
	NEED_CH("0123456789");
	NEED_CH("0123456789");
	NEED_CH("n.");
	if(strspn(s-1,"0123456789nyr")==strlen(s-1) ||
			(strspn(s-1,"0123456789.")==strlen(s-1) &&
			s[0]!='.' && n_spec_count(s,'.')<=1))
	{
		ret=1;
	}
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
	if(strspn(s,"0123456789sudfm")==len && (s[len-1]=='m' || s[len-1]=='f'))
		ret=1;
out:
	return ret;
}

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
		e->Priv1=4;
	}
	else if(n_is_digit(s,len))
	{
		e->Count=2;
		e->Priv1=0;
	}
	else if(n_is_number(s,len))
	{
		e->Count=2;
		e->Priv1=1;
	}
	else if(n_is_date(s,len))
	{
		int year=atoi(s);
		e->Count=2;
		e->Priv1=2;
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
		e->Priv1=3;
	}
	//printf("%d %d\n",e->Priv1,e->Count);
	return e->Count;
}

static int NumGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_num;
	int i;
	static const int type[5][4]={
		{BIT_0(2)|BIT_19(1) , BIT_0(2)|BIT_19(2)},
		{BIT_0(2)|BIT_19(1)|BIT_DW(1),BIT_0(2)|BIT_19(2)|BIT_DW(2)},
		{BIT_0(1)|BIT_19(1)|BIT_RQ(1),BIT_0(0)|BIT_19(0)|BIT_RQ(1)},
		{BIT_0(0)|BIT_19(0)|BIT_TM(1),BIT_0(2)|BIT_19(1)|BIT_TM(1)|BIT_INS(1)},
		{BIT_0(1),BIT_0(2)},
	};
	for(i=pos;i<pos+count && i<4;i++)
	{
		if(e->Priv1==2 && i==2)
		{
			const char *s=(const char*)e->Priv2;
			int year,month,day;
			char c1,c2;
			struct tm tm;
			strcpy(cand[i],"农历");
			if(5==sscanf(s,"%d%c%d%c%d",&year,&c1,&month,&c2,&day))
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
		else
		{
			num2hz((char*)e->Priv2,cand[i],type[(int)e->Priv1][i]);
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
		snprintf(res,len,"%f",var.v_float);
		break;
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
	if(!s[0]) return 0;
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
	const char *dw=(im.Trad?cht_dw:ch_dw)[sel];
	const char *num=(im.Trad?cht_19:ch_19)[sel];
	int len=0;
	int empty=0;
	int strip_10=0;/*(val/10==1);*/		// 是否忽略拾前面的一
	int have_value=0;					// 前面是否已经有值出现了
	int prev_value=0;					// 在上一位非0

	// 亿
	if(val/100000000)
	{
		parse_decimal(sel,val / 100000000,s);
		len=strlen(s);memcpy(s+len,dw+8,2);len+=2;s[len]=0;
		if((val/100000000)%10)
			prev_value=1;
		val%=100000000;
		empty=1;
		have_value=1;
	}
	// 前面已经有值，且上一位为0，且后面有值
	if(have_value && !prev_value && val)
	{
		strcpy(s+len,ch_0[1]);len+=2;
		have_value=0;			// 避免重复输入多于的0
	}
	prev_value=0;
	// 万
	if(val/10000)
	{
		parse_decimal(sel,val / 10000,s);
		len=strlen(s);memcpy(s+len,dw+6,2);len+=2;s[len]=0;
		if((val/10000)%10)
			prev_value=1;
		val%=10000;
		empty=1;
		have_value=1;
	}
	if(have_value && !prev_value && val)
	{
		strcpy(s+len,ch_0[1]);len+=2;
		have_value=0;
	}
	prev_value=0;
	// 千
	if(val/1000)
	{
		parse_decimal(sel,val / 1000,s);
		len=strlen(s);memcpy(s+len,dw+4,2);len+=2;s[len]=0;
		if((val/1000)%10)
			prev_value=1;
		empty=1;
		val%=1000;
		have_value=1;
	}
	if(have_value && !prev_value && val)
	{
		strcpy(s+len,ch_0[1]);len+=2;
		have_value=0;
	}
	// 百
	prev_value=0;
	if(val/100)
	{
		parse_decimal(sel,val / 100,s);
		len=strlen(s);memcpy(s+len,dw+2,2);len+=2;s[len]=0;
		if((val/100)%10)
			prev_value=1;
		empty=1;
		val%=100;
		have_value=1;
	}
	if(have_value && !prev_value && val)
	{
		strcpy(s+len,ch_0[1]);//len+=2;
		have_value=0;
	}
	// 十
	prev_value=0;
	if(val/10)
	{
		if(!strip_10) parse_decimal(sel,val / 10,s);
		len=strlen(s);memcpy(s+len,dw+0,2);len+=2;s[len]=0;
		prev_value=1;
		empty=0;
		val%=10;
		have_value=1;
	}
	if(have_value && !prev_value && val)
	{
		strcpy(s+len,ch_0[1]);
	}
	len=strlen(s);
	if(empty && val)
	{
		strcpy(s+len,ch_0[1]);//len+=2;
	}
	if(val)
	{
		memcpy(s+len,num+(val-1)*2,2);len+=2;s[len]=0;
	}
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
	int pos=0;
	if(key_temp_english && ((EIM.CodeInput[0]&0x7f)==key_temp_english))
	{
		res[pos++]=key_temp_english;
	}
	if(!strncasecmp(code+pos,"key ",4))
	{
		//strcpy(res+pos,"key ");
		memcpy(res+pos,code+pos,4);
		res[pos+4]=0;
		pos+=4;
	}
	else if(!strncasecmp(code+pos,"miyao ",6))
	{
		//strcpy(res+1,"miyao ");
		memcpy(res+pos,code+pos,6);
		res[pos+6]=0;
		pos+=6;
	}
	else
	{
		//y_im_str_encode(code,res,DONT_ESCAPE);
		strcpy(res,code);
		return;
	}
	if(code[pos]==' ')
	{
		res[pos++]=' ';
	}
	for(;code[pos]!=0;pos++)
	{
		res[pos]='*';
	}
	res[pos]=0;
}
