#include "yong.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

#include "pinyin.h"
#include "llib.h"
#include "local.h"
#include "cloud.h"

#ifdef CFG_XIM_ANDROID
#define rand() (int)lrand48()
#endif

static int SGY_Init(const char *arg);
static void SGY_Reset(void);
static char *SGY_GetCandWord(int index);
static int SGY_GetCandWords(int mode);
static int SGY_Destroy(void);
static int SGY_DoInput(int key);

L_EXPORT(EXTRA_IM EIM)={
	.Flag			=	IM_FLAG_ASYNC,
	.Name			=	"",
	.Reset			=	SGY_Reset,
	.DoInput		=	SGY_DoInput,
	.GetCandWords	=	SGY_GetCandWords,
	.GetCandWord	=	SGY_GetCandWord,
	.Init			=	SGY_Init,
	.Destroy		=	SGY_Destroy,
};

static char *u2gb(char *u)
{
	char data[256];
	l_utf8_to_gb(u,data,sizeof(data));
	return l_strdup(data);
}

void sg_cookie_free(sg_cookie_t *cookie)
{
	if(!cookie) return;
	free(cookie->name);
	free(cookie->value);
	free(cookie);
}

static void sg_res_free(sg_res_t *r)
{
	int i;
	if(!r) return;
	free(r->q);
	for(i=0;i<r->c;i++)
		free(r->cs[i].s);
	free(r->cs);
	free(r);
}

void sg_recc(sg_cache_t *c,int rec)
{
	for(int i=0;i<4 && l_hash_table_size(c->t)>10001;i++)
	{
		sg_res_t *p=l_hash_table_rand(c->t);
		if(p==c->l || p==c->l_old) continue;	/* locked */
		if(p->l<=2) continue;	/* always high freq */
		l_hash_table_remove(c->t,p);
		sg_res_free(p);
	}
}

void sg_cache_add(sg_cache_t *c,sg_res_t *r)
{
	sg_res_t *p=l_hash_table_replace(c->t,r);
	if(p!=NULL && p!=r)
	{
		sg_res_free(p);
	}
}

static sg_res_t *sg_add_local_result(sg_res_t *r,const char *pinyin,const char *list)
{
	char **temp;
	int len;
	int count;
	int i;
	if(!list) return r;
	temp=l_strsplit(list,' ');
	count=l_strv_length(temp);
	len=strlen(pinyin);
	if(!r)
	{
		r=l_new0(sg_res_t);
		r->q=l_strdup(pinyin);
		r->l=len;
		r->c=count;
		r->cs=l_cnew0(r->c,sg_cand_t);
		for(i=0;i<count;i++)
		{
			r->cs[i].s=l_strdup(temp[i]);
			r->cs[i].l=r->l;
		}
	}
	else
	{
		int ndup=0;
		r->cs=l_renew(r->cs,r->c+count,sg_cand_t);
		for(i=0;i<count;i++)
		{
			int j;
			for(j=0;j<r->c;j++)
			{
				if(!strcmp(r->cs[j].s,temp[i]))
					break;
			}
			if(j==r->c)
			{
				r->cs[r->c+ndup].s=l_strdup(temp[i]);
				r->cs[r->c+ndup].l=len;
				ndup++;
			}
		}
		r->c+=ndup;
	}
	l_strfreev(temp);
	return r;
}

sg_res_t *sg_local(sg_res_t *r,const char *p,bool strict)
{
	const char *list;
	char pinyin[16];
	int len;
	
	len=snprintf(pinyin,sizeof(pinyin),"%s",p);
	if(len>=15 && strict) return r;
	do{
		list=local_pinyin_get(pinyin);
		r=sg_add_local_result(r,pinyin,list);
		if(strict) break;
		len=strlen(pinyin);
		if(len==1) break;
		if(len==2 && pinyin[1]=='h' && strchr("zcs",pinyin[0])!=NULL)
			break;
		pinyin[len-1]=0;
	}while(1);
	
	return r;
}

static sg_cache_t *sg_cache_new(void)
{
	sg_cache_t *c;
	c=l_new0(sg_cache_t);
	c->t=L_HASH_TABLE_STRING(sg_res_t,q,SG_CACHE_SIZE);
	return c;
}

static void sg_cache_free(sg_cache_t *c)
{
	if(!c) return;
	l_hash_table_free(c->t,(LFreeFunc)sg_res_free);
	l_slist_free(c->cookie,(LFreeFunc)sg_cookie_free);
	l_free(c->format);
	l_free(c->proxy);
	l_free(c->proxy_auth);
	l_free(c->option);
	free(c);
}

sg_res_t *sg_cache_get(sg_cache_t *c,const char *s)
{
	return l_hash_table_lookup(c->t,s);
}

static char *url_get_auth(const char *url)
{
	char *p;
	char user[32],pass[32],auth[64];
	char temp[128];
	int len;
	if(!url)
		return NULL;
	if(!strncmp(url,"http://",7))
		url+=7;
	p=strchr(url,'@');
	if(!p)
		return NULL;
	user[0]=0;pass[0]=0;
	sscanf(url,"%31[^:]:%31[^@]",user,pass);
	if(!user[0])
		return NULL;
	len=sprintf(auth,"%s:%s",user,pass);
	l_base64_encode(temp,(const void*)auth,len);
	return l_strdup(temp);
}

static sg_res_t* bd_parse_res(sg_cache_t *c,char *s)
{
	char key[64],cand[256];
	sg_res_t *r;
	int i,n;
	char *p;
	char *res[20];
	int code[20];
	
	s=strstr(s,"[[[");
	if(!s)
		return NULL;
	//if(s[0]!='[' || s[1]!='[')
	//	return NULL;
	for(i=0,p=s+2;i<20;i++)
	{
#ifndef EMSCRIPTEN
		n=0;
		int ret=l_sscanf(p,"[\"%256[^\"]\",%d%*[^]]]%n",cand,code+i,&n);
		if(ret!=2 || n<=0)
		{
			for(n=0;n<i;n++)
				l_free(res[n]);
			return NULL;
		}
		res[i]=u2gb(cand);
		p+=n;
#else
		p+=2;
		for(n=0;n<255 && p[n]!='"';n++)
			cand[n]=p[n];
		cand[n]=0;
		p+=n+2;
		code[i]=atoi(p);
		res[i]=u2gb(cand);
		p=strchr(p,']')+1;
#endif
		if(p[0]==']')
		{
			i++;
			p++;
			break;
		}
		if(p[0]!=',')
		{
			for(n=0;n<i;n++)
				l_free(res[n]);
			return NULL;
		}
		p++;
	}
	if(!i)
	{
		return NULL;
	}
	r=l_new0(sg_res_t);
	r->c=i;
	r->cs=l_cnew0(r->c,sg_cand_t);
	for(i=0;i<r->c;i++)
	{
		r->cs[i].l=code[i];
		r->cs[i].s=res[i];
	}
	/*if(p[0]!=',' || p[1]!='\"')
	{
		sg_res_free(r);
		return NULL;
	}
	p+=2;*/
	p=strstr(s,"\"pinyin\":\"");
	if(!p)
	{
		sg_res_free(r);
		return NULL;
	}
	p+=10;
	for(i=0;i<63;i++)
	{
		while(*p=='\'') p++;
		if(*p=='\"') break;
		if(*p==0) break;
		key[i]=*p++;
	}
	key[i]=0;
	if(i==0)
	{
		sg_res_free(r);
		return NULL;
	}
	r->q=l_strdup(key);
	r->l=(unsigned short)strlen(key);
	return r;
}

static char *hu2gb(const char *s)
{
	char temp[256];
	int pos=0;
	uint32_t uc;
	int ret,n;
	int count;
	
	if(s[0]!='\\' || s[1]!='u')
	{
		l_utf8_to_gb(s,temp,sizeof(temp));
		return l_strdup(temp);
	}
	
	for(count=0;pos<sizeof(temp)-8;count++)
	{
		(void)count;
		ret=l_sscanf(s,"\\u%x%n",&uc,&n);
		if(ret!=1) break;
		s+=n;
		ret=l_unichar_to_gb(uc,(uint8_t*)temp+pos);
		if(ret<=0) break;
		temp[pos+ret]=0;
		pos+=ret;
	}
	if(pos==0)
		return NULL;
	temp[pos]=0;
	return l_strdup(temp);
}

static sg_res_t* gg_parse_res(sg_cache_t *c,char *s)
{
	char key[64],cand[512],*res[20];
	sg_res_t *r;
	int ret;
	s=strchr(s,'[');if(!s) return NULL;s++;
	if(strncmp(s,"\"SUCCESS\"",9))
		return NULL;
	s=strstr(s,"[[");if(!s) return NULL;s+=2;
	ret=l_sscanf(s,"\"%64[^\"]",key);
	if(ret!=1)
	{
		return NULL;
	}
	s=strchr(s,'[');if(!s) return NULL;s++;
	ret=l_sscanf(s,"\"%512[^\"]\"",cand);
	if(ret!=1)
	{
		return NULL;
	}
	res[0]=hu2gb(cand);
	if(!res[0])
	{
		return NULL;
	}
	r=l_new0(sg_res_t);
	r->c=1;
	r->cs=l_cnew0(r->c,sg_cand_t);
	r->cs[0].l=strlen(key);
	r->cs[0].s=res[0];
	r->q=l_strdup(key);
	r->l=(unsigned short)strlen(key);
	return r;
}

static sg_res_t* ek_parse_res(sg_cache_t *c,char *s)
{
	char key[64],cand[256];
	sg_res_t *r;
	if((s=strstr(s,"<S>"))==NULL)
		return NULL;
	s+=3;
	if(1!=sscanf(s,"%255[^<]",cand))
		return NULL;
	if((s=strstr(s,"<Q>"))==NULL)
		return NULL;
	s+=3;
	if(1!=sscanf(s,"%63[^<]",key))
		return NULL;
	r=l_new0(sg_res_t);
	r->l=(uint16_t)strlen(key);
	r->q=l_strdup(key);
	r->c=1;
	r->cs=l_cnew0(r->c,sg_cand_t);
	r->cs[0].l=r->l;
	r->cs[0].s=u2gb(cand);
	return r;
}

struct cloud_api sg_apis[]={
	{
		.name="baidu",
		.host="olime.baidu.com",
		.query_key=NULL,
		.query_res="/py?py=%s&fmt=JSON&rn=0&pn=20",
		.key_parse=NULL,
		.res_parse=bd_parse_res
	},
#ifdef EMSCRIPTEN
	{
		.name="google",
		.host="www.google.com",
		.query_key=NULL,
		.query_res="/transliterate?text=%s&langpair=en%%7Czh&tlqt=1&version=2&num=20&tl_app=3&uv=b%%3B0%%3B0&jsonp=_callbacks_._19glbuaa5f",
		.key_parse=NULL,
		.res_parse=gg_parse_res
	},
#else
	{
		.name="google",
		.host="www.google.cn",
		.query_key=NULL,
		.query_res="/inputtools/request?ime=pinyin&text=%s",
		.key_parse=NULL,
		.res_parse=gg_parse_res
	},
#endif
	{
		.name="engkoo",
		.host="s.p.msra.cn",
		.query_key=NULL,
		.query_res="/http/v2/7abc7712f9544664adb29271b2d79581/?q=%s",
		.key_parse=NULL,
		.res_parse=ek_parse_res
	}
};
struct cloud_api *sg_cur_api=&sg_apis[0];

static void sg_select_api(const char *name)
{
	int i;
	if(!name || !name[0])
		return;
	for(i=0;i<lengthof(sg_apis);i++)
	{
		if(!strcmp(sg_apis[i].name,name))
		{
			sg_cur_api=&sg_apis[i];
			return;
		}
	}
}

sg_res_t *sg_parse(sg_cache_t *c,char *s)
{
	sg_res_t *r;
	r=sg_cur_api->res_parse(c,s);
	if(!r)
		return NULL;

	if(r->c>0)
	{
		char *key=l_strndup(r->q,r->cs[r->c-1].l);
		r=sg_local(r,key,false);
		l_free(key);
	}

	CloudLock();
	sg_recc(c,2);
	sg_cache_add(c,r);
	CloudUnlock();

	return r;
}

static void sg_init(void)
{
}

static void sg_cleanup(void)
{
}

sg_cache_t *l_cache;
static sg_res_t *l_res;
static const void *l_user;

//static int PhraseListCount;
#define PhraseListCount EIM.CandWordTotal
static char CodeGet[MAX_CODE_LEN+1];
static int CodeGetLen;
static int AssistSeries;
static int AssistMode;

static sg_cand_t *CalcPhrase[64];
static int PhraseCalcCount;

static int SP=0;
static int ASSIST=0;

static int SGY_Init(const char *arg)
{
	char scheme[256];
	char *p;

	
	sg_init();
	l_cache=sg_cache_new();
	sg_select_api(arg);
	p=EIM.GetConfig(0,"proxy");
	if(p && p[0])
	{
		l_cache->proxy=l_strdup(p);
		l_cache->proxy_auth=url_get_auth(p);
	}
	p=EIM.GetConfig(0,"option");
	if(p && p[0]) l_cache->option=l_strdup(p);
	CloudInit();
	scheme[0]=0;
	p=EIM.GetConfig(0,"sp");
	if(p && p[0])
	{
		if(strcmp(p,"zrm"))
		{
			sprintf(scheme,"%s/%s.sp",EIM.GetPath("HOME"),p);
			if(!l_file_exists(scheme))
			{
				sprintf(scheme,"%s/%s.sp",EIM.GetPath("DATA"),p);
				if(!l_file_exists(scheme))
					scheme[0]=0;
			}
		}
		SP=1;
		EIM.Flag|=IM_FLAG_CAPITAL;
	}
	py_init(0,scheme);
	
	p=EIM.GetConfig(0,"pinyin");
	if(p)
	{
		local_load_pinyin(p);
	}
	p=EIM.GetConfig(0,"assist");
	if(p)
	{
		char **list=l_strsplit(p,' ');
		int pos=0;
		if(list[1]) pos=atoi(list[1]);
		local_load_assist(list[0],pos);
		l_strfreev(list);
		ASSIST=1;
	}
	p=EIM.GetConfig(0,"user");
	if(p)
	{
		local_load_user(p);
	}
	p=EIM.GetConfig(0,"assist_series");
	if(p && p[0] && p[0]!='0')
	{
		AssistSeries=1;
	}
	
	CloudWaitReady();
		
	return 0;
}

static int SGY_Destroy(void)
{
	if(!l_cache)
		return 0;
	CloudCleanup();
	sg_cache_free(l_cache);
	l_cache=0;
	sg_cleanup();
	
	local_free_all();
	return 0;
}

static void SGY_Reset(void)
{
	if(!l_cache)
		return;
	l_user=NULL;
	l_res=NULL;
	EIM.CodeInput[0]=0;
	EIM.CodeLen=0;
	EIM.CurCandPage=EIM.CandPageCount=EIM.CandWordCount=0;
	EIM.SelectIndex=0;
	CodeGetLen=0;
	PhraseListCount=0;
	PhraseCalcCount=0;
	AssistMode=0;
	CloudLock();
	l_cache->req[0]=0;
	l_cache->l=NULL;
	CloudUnlock();
}

static int PinyinDoSearch()
{
	char *p=EIM.CodeInput;
	char quan[128];
	l_user=local_phrase_set(p);
	if(l_user)
	{
		SGY_GetCandWords(PAGE_FIRST);
		return IMR_DISPLAY;
	}
	if(SP)
	{
		py_conv_from_sp(p,quan,sizeof(quan),0);
		p=quan;
	}
	CloudLock();
	l_res=sg_cache_get(l_cache,p);
	if(!l_res)
	{
		strcpy(l_cache->req,p);
		CloudSetSignal();
	}
	else
	{
		l_cache->l=l_res;
		l_cache->req[0]=0;
	}
	CloudUnlock();
	SGY_GetCandWords(PAGE_FIRST);
	return IMR_DISPLAY;
}

static void PinyinCodeRevert(void)
{
	CloudLock();
	EIM.CaretPos=strlen(CodeGet);
	strcat(CodeGet,EIM.CodeInput);
	strcpy(EIM.CodeInput,CodeGet);
	EIM.CodeLen=strlen(EIM.CodeInput);
	CodeGet[0]=0;
	CodeGetLen=0;
	EIM.StringGet[0]=0;
	CloudUnlock();
}

static char *SGY_GetCandWord(int index)
{
	int force=(index==-1);
	int pos;
	sg_cand_t *cand;
	
	if(index>=EIM.CandWordCount || index<-1)
		return 0;
	if(index==-1)
		index=EIM.SelectIndex;
	if(l_user)
	{
		strcat(EIM.StringGet,EIM.CandTable[index]);
		return EIM.StringGet;
	}
	if(!l_res || l_res!=l_cache->l)
		return 0;
	pos=EIM.CurCandPage*EIM.CandWordMax+index;
	if(PhraseCalcCount)
		cand=CalcPhrase[pos];
	else
		cand=&l_res->cs[pos];
	strcat(EIM.StringGet,cand->s);
	if(SP)
	{
		// FIXME: code is not always cand*2
		int len=l_gb_strlen((uint8_t*)cand->s,-1)*2;
		if(len>EIM.CodeLen) len=EIM.CodeLen;
		memcpy(CodeGet+CodeGetLen,EIM.CodeInput,len);
		CodeGetLen+=len;
		EIM.CodeLen-=len;
		CodeGet[CodeGetLen]=0;
		memmove(EIM.CodeInput,EIM.CodeInput+len,EIM.CodeLen+1);
		EIM.CaretPos-=len;
	}
	else
	{
		int len=cand->l;
		if(len>EIM.CodeLen) len=EIM.CodeLen;
		memcpy(CodeGet+CodeGetLen,EIM.CodeInput,cand->l);
		CodeGetLen+=len;
		EIM.CodeLen-=len;
		CodeGet[CodeGetLen]=0;
		memmove(EIM.CodeInput,EIM.CodeInput+len,EIM.CodeLen+1);
		EIM.CaretPos-=len;
	}
	if(EIM.CaretPos<0)
		EIM.CaretPos=0;
	PhraseCalcCount=0;
	if(0==EIM.CodeLen || force)
	{
		return EIM.StringGet;
	}
	else
	{
		PinyinDoSearch();
	}

	return 0;
}

static int SGY_GetCandWords(int mode)
{
	int max;
	int i,start;
	
	if(mode==PAGE_ASSOC)
		return IMR_PASS;
		
	max=EIM.CandWordMax;
	EIM.CandWordMaxReal=max;
	
	if(mode==PAGE_FIRST)
	{
		if(l_user)
		{
			PhraseListCount=local_phrase_count(l_user);
			PhraseCalcCount=0;
		}
		else
		{
			CloudLock();
			if(!l_cache->l)
			{
				l_res=0;
				PhraseListCount=0;
			}
			else
			{
				l_res=l_cache->l;
				PhraseListCount=l_res->c;
				PhraseCalcCount=0;
			}
			CloudUnlock();
		}
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	}
	
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

	start=EIM.CurCandPage*max;
	
	for(i=0;i<max;i++)
	{
		EIM.CodeTips[i][0]=0;
		EIM.CandTable[i][0]=0;
	}
	
	if(PhraseCalcCount)
	{
		for(i=0;i<EIM.CandWordCount;i++)
			strcpy(EIM.CandTable[i],CalcPhrase[start+i]->s);
	}
	else if(l_user)
	{
		local_phrase_get(l_user,start,EIM.CandWordCount,EIM.CandTable);
	}
	else if(l_cache->l) for(i=0;i<EIM.CandWordCount;i++)
	{
		sg_cand_t *cand=&l_cache->l->cs[start+i];
		strcpy(EIM.CandTable[i],cand->s);
	}
	return IMR_DISPLAY;
}

static int CloudMoveCaretTo(int key)
{
	int i,p;
	if(EIM.CodeLen<1)
		return 0;
	if(key>='A' && key<='Z')
		key='a'+(key-'A');
	for(i=0,p=EIM.CaretPos+1;i<EIM.CodeLen;i++)
	{
		if(p&0x01)
			p++;
		if(p>=EIM.CodeLen)
			p=0;
		if(EIM.CodeInput[p]==key)
		{
			EIM.CaretPos=p;
			break;
		}
		p++;
	}
	return 0;
}

static int SGY_DoInput(int key)
{
	int i;
	
	if(AssistMode && l_cache->l)
	{
		if(!AssistSeries)
			AssistMode=0;
		if(!l_user && local_is_assist_key(key))
		{
			int i=0;
			PhraseCalcCount=0;
			CloudLock();
			for(;i<l_cache->l->c;i++)
			{
				sg_cand_t *cand=&l_cache->l->cs[i];
				if(!local_assist_match(cand->s,key))
					continue;
				CalcPhrase[PhraseCalcCount]=cand;
				PhraseCalcCount++;
			}
			CloudUnlock();
			if(PhraseCalcCount)
			{
				PhraseListCount=PhraseCalcCount;
				EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+
						((PhraseListCount%EIM.CandWordMax)?1:0);
				EIM.CurCandPage=0;
				SGY_GetCandWords(PAGE_REFRESH);
				return IMR_DISPLAY;
			}
			return IMR_BLOCK;
		}
	}

	if(key==YK_BACKSPACE)
	{
		AssistMode=0;
		if(EIM.CodeLen==0 && CodeGetLen==0)
			return IMR_CLEAN_PASS;
		if(EIM.CaretPos==0)
		{
			if(CodeGetLen)
			{
				PinyinCodeRevert();
				return IMR_DISPLAY;
			}
			return IMR_BLOCK;
		}
		for(i=EIM.CaretPos;i<EIM.CodeLen;i++)
			EIM.CodeInput[i-1]=EIM.CodeInput[i];
		EIM.CodeLen--;
		EIM.CaretPos--;
		EIM.CodeInput[EIM.CodeLen]=0;
		if(EIM.CaretPos==0)
		{
			if(CodeGetLen)
			{
				PinyinCodeRevert();
				return IMR_DISPLAY;
			}
		}
	}
	else if(key==YK_DELETE)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		if(EIM.CodeLen!=EIM.CaretPos)
		{
			CloudLock();
			for(i=EIM.CaretPos;i<EIM.CodeLen;i++)
				EIM.CodeInput[i]=EIM.CodeInput[i+1];
			EIM.CodeLen--;
			EIM.CodeInput[EIM.CodeLen]=0;
			CloudUnlock();
		}
	}
	else if(key==YK_HOME)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		EIM.CaretPos=0;
	}
	else if(key==YK_END)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		EIM.CaretPos=EIM.CodeLen;
	}
	else if(key==YK_LEFT)
	{
		if(EIM.CodeLen==0 && !CodeGetLen)
			return IMR_PASS;
		if(EIM.CaretPos==0 && CodeGetLen)
		{
			PinyinCodeRevert();
			return IMR_DISPLAY;
		}
		if(EIM.CaretPos>0)
			EIM.CaretPos--;		
	}
	else if(key==YK_RIGHT)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		if(EIM.CaretPos<EIM.CodeLen)
			EIM.CaretPos++;
	}
	else if(key==YK_TAB)
	{
		if(EIM.CandWordCount && ASSIST!=0)
		{
			AssistMode=1;
			return IMR_BLOCK;
		}
		return IMR_NEXT;
	}
	else if(py_is_valid_input(SP,key,EIM.CaretPos))
	{
		if(CodeGetLen+EIM.CodeLen>=54)
			return IMR_BLOCK;
		PhraseCalcCount=0;
		CloudLock();
		if(EIM.CodeLen==0 && CodeGetLen==0)
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
		CloudUnlock();
	}
	else if(key>='A' && key<='Z')
	{
		if(EIM.CodeLen>=1)
			CloudMoveCaretTo(key);
		else
			return IMR_NEXT;
	}
	else if(key==YK_VIRT_REFRESH)
	{
		int i;
		for(i=0;i<EIM.CodeLen;i++)
		{
			if(!islower(EIM.CodeInput[i]))
				return IMR_BLOCK;
		}
		PinyinDoSearch();
		return SGY_GetCandWords(PAGE_FIRST);
	}
	else if(key>=YK_VIRT_CARET && key<=YK_VIRT_CARET+MAX_CODE_LEN+2)
	{
		int caret=key-YK_VIRT_CARET-1;
		if(caret<0 && CodeGetLen)
		{
			PinyinCodeRevert();
		}
		if(caret>=0 && caret<=EIM.CodeLen)
		{
			if(caret==EIM.CaretPos)
				return IMR_BLOCK;
			EIM.CaretPos=caret;
		}
	}
	else
	{
		return IMR_NEXT;
	}
	PinyinDoSearch();
	return IMR_DISPLAY;
}
