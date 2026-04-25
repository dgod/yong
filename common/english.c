#include <ctype.h>
#include <inttypes.h>
#include <time.h>

#include "yong.h"
#include "common.h"
#include "english.h"
#include "translate.h"
#include "llineedit.h"

static int EnglishInit(const char *arg);
static void EnglishReset(void);
static char *EnglishGetCandWord(int index);
static int EnglishGetCandWords(int mode);
static int EnglishDestroy(void);
static int EnglishDoInput(int key);
static int EnglishDoSearch(void);

static int PhraseListCount;
static int key_temp_english;
static int en_commit_select;
static int en_degrade;
static char num_formats[16];
static LLineEdit line;
static bool from_cnen;

static EXTRA_IM EIM={
	.Name			=	"english",
	.Reset			=	EnglishReset,
	.DoInput		=	EnglishDoInput,
	.GetCandWords	=	EnglishGetCandWords,
	.GetCandWord	=	EnglishGetCandWord,
	.Init			=	EnglishInit,
	.Destroy		=	EnglishDestroy
};

#if 0
static int NumSet(const char *s);
static int NumGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_num={.Set=NumSet,.Get=NumGet};
#endif

static int Num2Set(const char *s);
static int Num2Get(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_num2={.Set=Num2Set,.Get=Num2Get};

static int CalcSet(const char *s);
static int CalcGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_calc={.Set=CalcSet,.Get=CalcGet};

static int UrlSet(const char *s);
static int UrlGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_url={.Set=UrlSet,.Get=UrlGet};

static int BdSet(const char *s);
static int BdGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_bd={.Set=BdSet,.Get=BdGet};

static int HexSet(const char *s);
static int HexGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_hex={.Set=HexSet,.Get=HexGet};

static void DictLoad(void);
static void DictFree(void);
static int DictSet(const char *s);
static int DictGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static ENGLISH_IM eim_dict={.Set=DictSet,.Get=DictGet};

#ifndef CFG_XIM_WEBIM
static int PipeSet(const char *s);
static int PipeGet(char cand[][MAX_CAND_LEN+1],int pos,int count);
static void PipeReset(void);
static ENGLISH_IM eim_pipe={.Set=PipeSet,.Get=PipeGet,.Reset=PipeReset};
#endif

extern ENGLISH_IM eim_book;

static ENGLISH_IM *enim[]={
	&eim_num2,
	// &eim_num,
	&eim_calc,
	&eim_url,
	&eim_bd,
	// &eim_money,
	&eim_book,
	&eim_hex,
#ifndef CFG_XIM_WEBIM
	&eim_pipe,
#endif
	&eim_dict
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
	const char *temp=y_im_get_config_data("IM","en_num");
	if(!temp || !temp[0])
		strcpy(num_formats,"0dDimYyn:%");
	else
		l_strcpy(num_formats,sizeof(num_formats),temp);
	EnglishReset();
	DictLoad();

	l_line_edit_set_allow(&line,"\x20-\x7e",true);
	l_line_edit_set_first(&line,key_temp_english,false);
	l_line_edit_set_max(&line,MAX_CODE_LEN);
	l_line_edit_set_nav(&line,YK_LEFT,YK_RIGHT,YK_HOME,YK_END);

	return 0;
}

static void EnglishReset(void)
{
	int i;

	l_line_edit_clear(&line);
	
	PhraseListCount=0;
	from_cnen=false;
	line.first=key_temp_english;
	EIM.CandWordTotal=0;
	EIM.CaretPos=0;
	EIM.CodeInput[0]=0;
	EIM.CodeLen=0;
	EIM.CurCandPage=EIM.CandPageCount=EIM.CandWordCount=0;
	EIM.SelectIndex=0;
	
	for(i=0;i<ENIM_COUNT;i++)
	{
		ENGLISH_IM *e=enim[i];
		if(e->Reset)
			e->Reset();
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
			if(strlen(phrase)<len)
				return 2;
			for(int i=0;i<len;i++)
			{
				if(p[i]>='a' && p[i]<='z')
					p[i]=phrase[i];
			}
			strcat(p,phrase+len);
			EIM.CaretPos=EIM.CodeLen=strlen(EIM.CodeInput);
			return 1;
		}
	}
	return 0;
}

static char *EnglishGetCandWord(int index)
{
	if(index>=EIM.CandWordCount)
		return 0;
	if(index==-1)
		index=EIM.SelectIndex;
	char *ret=&EIM.CandTable[index][0];
	int complete=AutoCompleteByDict(index);
	if(!en_commit_select && complete)
	{
		EnglishDoSearch();
		return NULL;
	}
	if(complete)
	{
		ret=EIM.CodeInput;
		if(ret[0]==key_temp_english && ret[1])
			ret++;
		return ret;
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
	const char *s=EIM.CodeInput;
	
	PhraseListCount=0;
	EIM.CandWordTotal=0;
	if(!y_english_from_cnen())
		s++;
	for(int i=0;i<ENIM_COUNT;i++)
	{
		ENGLISH_IM *e=enim[i];
		e->Set(s);
		PhraseListCount+=e->Count;
	}
	EIM.CandWordTotal=PhraseListCount;
	int max=EIM.CandWordMax;
	EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	EnglishGetCandWords(PAGE_FIRST);
	return PhraseListCount;
}

static inline int key_is_select(int key)
{
	return y_im_check_select(key,3)>=0;
}

static int EnglishDoInput(int key)
{
	key&=~KEYM_BING;
	if(key==YK_SPACE)
	{
		if(en_degrade)
			return IMR_NEXT;
		if(EIM.CandWordCount && !AutoCompleteByDict(YK_SPACE))
			return IMR_NEXT;
	}
	if(en_degrade && EIM.CodeLen>0 && key_is_select(key))
		return IMR_NEXT;
	if(en_degrade==2 && key>='0' && key<='9')
		return IMR_NEXT;
	int ret=l_line_edit_push(&line,key);
	if(ret==1)
	{
		EIM.CodeLen=l_line_edit_copy(&line,EIM.CodeInput,-1,&EIM.CaretPos);
		if(EIM.CodeLen==0)
			return IMR_CLEAN;
	}
	else
	{
		if(key==YK_VIRT_CNEN)
		{
			l_line_edit_set_text(&line,EIM.CodeInput);
			from_cnen=true;
			strcpy(EIM.StringGet,"> ");
			line.first=0;
		}
		else if(key==YK_VIRT_REFRESH)
		{
		}
		else if(key==SHIFT_ENTER)
		{
			int start=EIM.CaretPos=(EIM.CodeInput[0]==key_temp_english)?1:0;
			int c=EIM.CodeInput[start];
			if(c>='a' && c<='z')
				EIM.StringGet[0]=c-'a'+'A';
			strcpy(EIM.StringGet+1,EIM.CodeInput+start+1);
			return IMR_COMMIT;
		}
		else
		{
			return IMR_NEXT;
		}
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

bool y_english_from_cnen(void)
{
	return from_cnen;
}

/*
 * english's num translate
 */

static char *ch_0[]={"〇","零"};
static char *ch_19[]={"一二三四五六七八九","壹贰叁肆伍陆柒捌玖"};
static char *cht_19[]={"一二三四五六七八九","壹貳參肆伍陸柒捌玖"};

#if 0
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
#endif

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
	
static void parse_decimal(int sel,int64_t val,char *s);

static int date_is_valid(const char *s)
{
	int year,month,day;
	int ret=l_sscanf(s,"%d.%d.%d",&year,&month,&day);
	if(ret<1)
		return 0;
	if(year<1000 || year>2999)
		return 0;
	if(ret==1)
		return ret;
	if(month<1 || month>12)
		return 0;
	if(ret==2)
		return ret;
	if(day<1 || day>31)
		return 0;
	return ret;
}

static int time_is_valid(const char *s)
{
	int ret,hour,minute,second;
	ret=l_sscanf(s,"%d:%d:%d",&hour,&minute,&second);
	if(ret<1)
		return 0;
	if(hour>24)
		return 0;
	if(ret==1)
		return ret;
	if(minute>59)
		return 0;
	if(ret==2)
		return ret;
	if(second>59)
		return 0;
	return ret;

}

static int Num2Set(const char *s)
{
	ENGLISH_IM *e=&eim_num2;
	int len,i,format;
	
	e->Priv2=(uintptr_t)s;	
	e->Count=0;
	e->Priv1=0;
	
	len=strlen(s);
	if(!len)
	{
		return 0;
	}
	for(i=0;(format=num_formats[i])!=0;i++)
	{
		switch(format){
			case 'y':
			case 'Y':
			{
				const char *re="^\\d{4}\\.(\\d{1,2}(\\.(\\d{1,2})?)?)?$";
				if(l_re_test(re,s)>0 && date_is_valid(s))
					e->Priv1|=1<<i;
				break;
			}
			case 'n':
			{
				const char *re="^\\d{4}\\.\\d{1,2}\\.\\d{1,2}$";
				if(l_re_test(re,s)>0 && date_is_valid(s)==3)
					e->Priv1|=1<<i;
				break;
			}
			case ':':
			{
				const char *re="^\\d{1,2}:\\d{1,2}(:\\d{0,2})?$";
				if(l_re_test(re,s)>0 && time_is_valid(s))
					e->Priv1|=1<<i;
				break;
			}
			case '%':
			{
				const char *re="^\\d{1,5}(\\.\\d{0,7})?%$";
				if(l_re_test(re,s)>0)
					e->Priv1|=1<<i;
				break;
			}
			case 'd':
			case 'D':
			{
				const char *re="^\\d{1,10}$";
				if(l_re_test(re,s)>0)
				{
					if(len==1 && s[0]=='0')
					{
						if(format=='d' && l_chrnpos(num_formats,'D',i)>=0)
							break;
						if(format=='D' && l_chrnpos(num_formats,'d',i)>=0)
							break;
					}
					e->Priv1|=1<<i;
				}
				break;
			}
			case 'i':
			{
				const char *re="^[1-9]\\d{1,9}$";
				if(l_re_test(re,s)>0)
					e->Priv1|=1<<i;
				break;
			}
			case '0':
			{
				if(len==1 && s[0]=='0')
					e->Priv1|=1<<i;
				break;
			}
			case 'm':
			{
				const char *re="^[1-9]\\d{0,11}(\\.\\d{0,2})?$";
				if(l_re_test(re,s)>0)
					e->Priv1|=1<<i;
				break;
			}
			default:
			{
				break;
			}
		}
		// if(e->Priv1&(1<<i))
			// printf("%c match\n",format);
	}
	e->Count=l_popcount(e->Priv1);
	return e->Count;
}

static int Num2Get(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_num2;
	if(e->Count==0)
		return 0;
	int cur=-1;
	const char *s=(const char*)(uintptr_t)(e->Priv2);
	int trad=im.Trad?L_INT2STR_TRAD:0;
	for(int i=0;i<16 && cur<count-1;i++)
	{
		if(!(e->Priv1&(1<<i)))
			continue;
		cur++;
		if(cur<pos)
			continue;
		char *o=cand[cur];
		int format=num_formats[i];
		switch(format){
			case 'y':
			case 'Y':
			{
				int which=format=='y';
				int year,month,day;
				int ret=l_sscanf(s,"%d.%d.%d",&year,&month,&day);
				int len=l_int_to_str(year,NULL,(which?0:L_INT2STR_HZ|L_INT2STR_ZERO0),o);
				strcpy(o+len,"年");len+=2;
				if(ret>1)
				{
					len+=l_int_to_str(month,NULL,(which?0:L_INT2STR_INDIRECT),o+len);
					strcpy(o+len,"月");len+=2;
					if(ret>2)
					{
						len+=l_int_to_str(day,NULL,(which?0:L_INT2STR_INDIRECT),o+len);
						strcpy(o+len,"日");
					}
				}
				break;
			}
			case 'n':
			{
				int year,month,day;
				l_sscanf(s,"%d.%d.%d",&year,&month,&day);
				int len=sprintf(o,"%s","农历");
				int ret=y_im_nl_from_day(o+len,year,month,day);
				if(ret!=0)
				{
					strcat(cand[i],YT("范围超限"));
				}
				break;
			}
			case ':':
			{
				int ret,len,h,m,sec;
				ret=l_sscanf(s,"%d:%d:%d",&h,&m,&sec);
				len=l_int_to_str(h,NULL,L_INT2STR_INDIRECT,o);
				strcpy(o+len,"点");
				len+=2;
				if(ret>1)
				{
					len+=l_int_to_str(m,NULL,L_INT2STR_HZ|L_INT2STR_MINSEC,o+len);
					strcpy(o+len,"分");
					len+=2;
					if(ret>2)
					{
						len+=l_int_to_str(sec,NULL,L_INT2STR_HZ|L_INT2STR_MINSEC,o+len);
						strcpy(o+len,"秒");
					}
				}
				break;
			}
			case '%':
			{
				int ret,len,p0;
				char p1[8];
				ret=l_sscanf(s,"%d.%7[^%]",&p0,p1);
				len=sprintf(o,"%s","百分之");
				len+=l_int_to_str(p0,NULL,L_INT2STR_INDIRECT,o+len);
				if(ret>1)
				{
					const char *num0=ch_0[0];
					const char *num19=ch_19[0];
					strcpy(o+len,"点");len+=2;
					for(int j=0;p1[j]!=0;j++)
					{
						int n=p1[j]-'0';
						if(n==0)
							strcpy(o+len,num0);
						else
							l_strncpy(o+len,num19+(n-1)*2,2);
						len+=2;
					}
					o[len]=0;
				}
				break;
			}
			case 'd':
			{
				int64_t n=(int64_t)strtoll(s,NULL,10);
				l_int_to_str(n,NULL,L_INT2STR_HZ,o);
				break;
			}
			case 'D':
			{
				int64_t n=(int64_t)strtoll(s,NULL,10);
				l_int_to_str(n,NULL,L_INT2STR_HZ|L_INT2STR_BIG|trad,o);
				break;
			}
			case 'i':
			{
				int64_t n=(int64_t)strtoll(s,NULL,10);
				l_int_to_str(n,NULL,L_INT2STR_INDIRECT|trad,o);
				break;
			}
			case '0':
			{
				l_int_to_str(0,NULL,L_INT2STR_HZ|L_INT2STR_ZERO0,o);
				break;
			}
			case 'm':
			{
				/* 测试例子 101,100001,208,2008,120000,101101,0.34，0.04 */
				int64_t yuan=0;
				uint8_t jiao=0,fen=0;
				l_sscanf(s,"%"PRId64".%c%c",&yuan,&jiao,&fen);
				if(jiao)
					jiao-='0';
				if(fen)
					fen-='0';
				if(yuan)
				{
					parse_decimal(1,yuan,o);
					strcat(o,"元");
				}
				if(!jiao && !fen)
				{
					strcat(o,"整");
				}
				else
				{
					const char *num=(im.Trad?cht_19:ch_19)[1];
					int len=strlen(o);
					if(jiao)
					{
						memcpy(o+len,num+(jiao-1)*2,2);
						len+=2;
						strcpy(o+len,"角");
						len+=2;
					}
					else if(yuan!=0)
					{
						strcpy(o+len,ch_0[1]);
						len+=2;
					}
					o[len]=0;
					if(fen)
					{
						memcpy(o+len,num+(fen-1)*2,2);
						len+=2;
						o[len]=0;
						strcpy(o+len,"分");
					}
					else
					{
						strcat(o,"整");
					}
				}
				break;
			}
			default:
			{
				cand[cur][0]=0;
				break;
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
	if(y_english_from_cnen())
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

static void parse_decimal(int sel,int64_t val,char *s)
{
	int flags=L_INT2STR_INDIRECT;
	if(im.Trad)
		flags|=L_INT2STR_TRAD;
	if(sel)
		flags|=L_INT2STR_BIG|L_INT2STR_KEEP10;
	l_int_to_str(val,NULL,flags,s);
}

/*
 * 测试例子
 * 101,100001,208,2008,120000,101101,0.34，0.04
 */
#if 0
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
#endif

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
	if(key_temp_english && code[0]==key_temp_english && !from_cnen)
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
#ifndef CFG_XIM_WEBIM
	if(eim_pipe.Count>0)
		return 0;
#endif
	// 寻找最后一个单词的开始位置
	p=s;
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

#ifndef CFG_XIM_WEBIM

typedef struct{
	bool run;
	char **result;
}PIPE_PRIV;

static void PipeCb(const char *result,char *code)
{
	ENGLISH_IM *e=&eim_pipe;
	char *cur=LINT_TO_PTR(e->Priv1);
	PIPE_PRIV *pp=LINT_TO_PTR(e->Priv2);
	if(pp)
		pp->run=false;
	if(!cur || !pp)
	{
		l_free(code);
		return;
	}
	if(strcmp(cur,code))
	{
		e->Priv1=0;
		l_free(code);
		PipeSet(cur);
		l_free(cur);
		return;
	}
	if(pp->result)
	{
		l_strfreev(pp->result);
		pp->result=NULL;
	}
	if(!result || !result[0])
	{
		y_im_request(1);
		return;
	}
	char **p=l_strsplit(result,'\n');
	pp->result=p;
	e->Count=l_strv_length(p);
#ifdef _WIN32
	for(int i=0;i<e->Count;i++)
	{
		l_str_trim_right(p[i]);
	}
#endif
	y_im_request(1);
}

static int PipeRun(char **argv,const char *s)
{
	char *user=l_strdup(s);
	int ret=y_im_async_spawn(argv,(void*)PipeCb,user,false);
	l_strfreev(argv);
	return ret;
}

static int PipeSet(const char *s)
{
	ENGLISH_IM *e=&eim_pipe;
	char **argv=NULL;
	int len=strlen(s);
	if(len>1 && s[len-1]=='|')
	{
		if(s[0] && s[1]==' ' && s[2])
		{
			for(int i=0;i<10;i++)
			{
				char key[32];
				sprintf(key,"en_pipe[%d]",i);
				const char *p=y_im_get_config_data("IM",key);
				if(p!=NULL && p[0]==s[0])
				{
					char temp[256];
					len=sprintf(temp,"%s %s",p+2,s+2);
					temp[len-1]=0;
					argv=y_im_parse_argv(temp,-1);
					break;
				}
			}
		}
		else if(s[0]=='|')
		{
			const char *p=strchr(s,' ');
			if(p && p[1])
			{
				char temp[256];
				l_strncpy(temp,s+1,len-2);
				argv=y_im_parse_argv(temp,-1);
			}
		}
	}
	if(argv==NULL)
	{
		PipeReset();
		return 0;
	}
#ifdef _WIN64
	if(!l_file_exists(argv[0]))
	{
		char temp[256];
		sprintf(temp,"%s/%s",y_im_get_path("DATA"),argv[0]);
		if(l_file_exists(temp))
		{
			if(l_fullpath(temp,temp,sizeof(temp)))
			{
				l_free(argv[0]);
				argv[0]=l_strdup(temp);
			}
		}
	}
#endif
	char *prev=LINT_TO_PTR(e->Priv1);
	if(prev)
	{
		if(!strcmp(prev,s))
		{
			l_strfreev(argv);
			return 0;
		}
		l_free(prev);
	}
	e->Priv1=(int64_t)(uintptr_t)l_strdup(s);
	PIPE_PRIV *pp=LINT_TO_PTR(e->Priv2);
	if(pp)
	{
		if(pp->run)
		{
			l_strfreev(argv);
			return 0;
		}
	}
	else
	{
		pp=l_new(PIPE_PRIV);
		pp->result=NULL;
		e->Priv2=(uintptr_t)pp;
	}
	pp->run=true;
	if(0!=PipeRun(argv,s))
	{
		PipeReset();
		return 0;
	}
	return 0;
}

static int PipeGet(char cand[][MAX_CAND_LEN+1],int pos,int count)
{
	ENGLISH_IM *e=&eim_pipe;
	PIPE_PRIV *pp=(PIPE_PRIV*)e->Priv2;
	if(!pp || !pp->result)
		return 0;
	for(int i=0;i<count;i++)
	{
		l_strcpy(cand[i],MAX_CAND_LEN+1,pp->result[pos+i]);
	}
	return 0;
}

static void PipeReset(void)
{
	ENGLISH_IM *e=&eim_pipe;
	e->Count=0;
	if(e->Priv1)
	{
		l_free(LINT_TO_PTR(e->Priv1));
		e->Priv1=0;
	}
	if(e->Priv2)
	{
		PIPE_PRIV *pp=(PIPE_PRIV*)e->Priv2;
		l_strfreev(pp->result);
		l_free(pp);
		e->Priv2=0;
	}
}
#endif//CFG_XIM_WEBIM
