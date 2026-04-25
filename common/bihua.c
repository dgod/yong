/*!
 * \author dgod
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "llib.h"

#include "bihua.h"
#include "common.h"

static const char *bihua_key=Y_BIHUA_KEY;

static const Y_BIHUA_INFO y_bihua_info[32]={
	{"É½","252"},	//a
	{0,0},			//b
	{0,0},			//c
	{"Ø¼","4"},		//d
	{0,0},			//e
	{0,0},			//f
	{0,0},			//g
	{"Ò»","1"},		//h
	{0,0},			//i
	{0,0},			//j
	{"¿Ú","251"},	//k
	{0,0},			//l
	{"Ä¾","1234"},	//m
	{"Ø¼","4"},		//n
	{0,0},			//o
	{"Ø¯","3"},		//p
	{0,0},			//q
	{0,0},			//r
	{"Ø­","2"},		//s
	{0,0},			//t
	{"Ø­","2"},		//u
	{"¥Õ","5"},		//v
	{0,0},			//w
	{0,0},			//x
	{0,0},			//y
	{"¥Õ","5"},		//z
	{"ãß","441"},	//;
	{0,0},			//'
	{0,0},			//,
	{0,0},			//.
	{0,0},			//
};
static const Y_BIHUA_INFO *bihua_info;

#define BIHUA_MAX	63
#define GROUP_MAX	63
#define BIHUA_VERSION	1

#define TRUNC_COUNT	5
#define TRUNC_SIZE	sizeof(struct bihua_trunc)

struct bihua_trunc{
	uint16_t count;
	uint16_t group;
	uint32_t offset[GROUP_MAX];
};

struct bihua_result{
	uint32_t orig;
	uint32_t offset;
	uint32_t count;
	uint32_t str[10];
};

static uint8_t bihua_version;
static struct bihua_trunc *trunc;
static char *base;
static struct bihua_result res;	

static void y_bihua_free(void)
{
	if(!trunc)
		return;

	l_free(trunc);
	trunc=NULL;
	base=NULL;
}

static int y_bihua_load(const char *fn)
{
	size_t data_size;
	y_bihua_free();
	trunc=(void*)l_file_get_contents(fn,&data_size,
#ifndef TOOLS_BIHUA
			y_im_get_path("HOME"),y_im_get_path("DATA"),
#endif
			NULL);
	if(!trunc || data_size<0x10000)
	{
		printf("yong: bihua file bad %s %p %d\n",fn,trunc,(int)data_size);
		l_free(trunc);
		data_size=0;
		trunc=NULL;
		return -1;
	}
	base=(char*)(trunc+TRUNC_COUNT);
	bihua_version=trunc->group>>14;
	if(bihua_version!=1)
	{
		y_bihua_free();
		return -1;
	}
	trunc->group&=0x3fff;

	return 0;
}

static char *bihua_escape(const char *bh,char temp[BIHUA_MAX+1])
{
	int i,len,p;
	char c,*code;
	for(i=0;(c=*bh++)!=0;)
	{
		code=strchr(bihua_key,c);
		if(!code)
			return NULL;
		p=code-bihua_key;
		code=bihua_info[p].code;
		if(!code)
			return NULL;
		len=strlen(code);
		if(i+len>BIHUA_MAX)
			return NULL;
		strcpy(temp+i,code);
		i+=len;
	}
	if(i==0)
		return NULL;
	temp[i]=0;
	return temp;
}

static int bihua_decompress(const char *bh,int len,char temp[BIHUA_MAX+1])
{
	int pos=0;
	for(int i=0;i<len;i++)
	{
		temp[pos++]='0'+((bh[i]>>4)&0xf);
		if((bh[i]&0xf)!=0)
			temp[pos++]='0'+(bh[i]&0xf);
	}
	temp[pos]=0;
	return pos;
}

static int bihua_test(const char *bh,const char *s,int len)
{
	int bhlen=bh[0];
	if(gb_is_gbk(bh+1))
	{
		bh+=3;
		bhlen-=3;
	}
	else
	{
		bh+=5;
		bhlen-=5;
	}
	char *temp=l_alloca(BIHUA_MAX+1);
	bhlen=bihua_decompress(bh,bhlen,temp);
	bh=temp;
	if(bhlen<len)
	{
		int ret=memcmp(bh,s,bhlen);
		return (ret<=0)?-1:1;
	}
	else
	{
		return memcmp(bh,s,len);
	}
}

static inline uint32_t bihua_next(uint32_t offset)
{
	return offset+base[offset];
}

static inline uint32_t group_size(struct bihua_trunc *t,int group)
{
	if(group==GROUP_MAX-1)
		return t->count-(t->group*GROUP_MAX-1);
	return t->group;
}

static int y_bihua_set(const char *s)
{
	struct bihua_trunc *t;
	int ret;
	int group[2];
	uint32_t offset[2];
	int count;
	char temp[BIHUA_MAX+1];

	s=bihua_escape(s,temp);
	int len=strlen(s);

	t=trunc+s[0]-'1';
	if(!s[1])
	{
		res.count=t->count;
		res.offset=t->offset[0];
		return res.count;
	}
	int group_max=t->count/t->group+((t->count%t->group)?1:0);
	group[0]=0;
	group[1]=group_max;
	for(int i=0;i<group_max;i++)
	{
		ret=bihua_test(base+t->offset[i],s,len);
		// printf("test %d %d count %d\n",i,ret,t->count);
		if(ret<0)
		{
			group[0]=i;
			continue;
		}
		if(ret>0)
		{
			group[1]=i;
			break;
		}
	}
	if(group[0]==group[1])
	{
		res.count=0;
		res.offset=0;
		return res.count;
	}
	count=0;
	for(int i=group[0];i<group[1]-1;i++)
	{
		count+=t->group;
	}
	offset[0]=t->offset[group[0]];
	for(int i=0;i<t->group;i++)
	{
		ret=bihua_test(base+offset[0],s,len);
		if(ret==0)
			break;
		if(ret>0)
			return 0;
		if(count>0)
			count--;
		offset[0]=bihua_next(offset[0]);
	}
	offset[1]=(group[1]-1>group[0])?t->offset[group[1]-1]:offset[0];
	int size=group_size(t,group[1]-1);
	for(int i=0;i<size;i++)
	{
		ret=bihua_test(base+offset[1],s,len);
		if(ret!=0)
			break;
		offset[1]=bihua_next(offset[1]);
		count++;
	}
	res.offset=res.orig=offset[0];
	res.count=count;
	return res.count;
}

static uint32_t *y_bihua_get(int at,int num)
{
	uint32_t offset;
	if(!res.count || num>10 || num==0)
		return NULL;
	offset=res.offset;
	for(int i=0;i<at;i++)
	{
		offset=bihua_next(offset);
	}
	for(int i=0;i<num;i++)
	{
		char *t=base+offset;
		res.str[i]=l_gb_to_char(t+1);
		offset=bihua_next(offset);
	}
	return res.str;
}



#ifndef TOOLS_BIHUA

#include "yong.h"
#include "llineedit.h"

// static int BihuaLen;
// static char BihuaInput[64];
static LLineEdit line;

static int PhraseListCount;
static int BihuaKey;
static int cand_a;

static int BihuaInit(const char *arg);
static void BihuaReset(void);
static char *BihuaGetCandWord(int index);
static int BihuaGetCandWords(int mode);
static int BihuaDestroy(void);
static int BihuaDoInput(int key);

static EXTRA_IM EIM={
	.Name="bihua",
	.Reset=BihuaReset,
	.DoInput=BihuaDoInput,
	.GetCandWords=BihuaGetCandWords,
	.GetCandWord=BihuaGetCandWord,
	.Init=BihuaInit,
	.Destroy=BihuaDestroy,
};

void *y_bihua_eim(void)
{
	return &EIM;
}

int y_bihua_good(void)
{
	return !!trunc;
}

static int BihuaDisplay(const char *b,char *s)
{
	int len=1,c;
	s[0]=BihuaKey;
	for(int i=1;(c=b[i])!=0;i++)
	{
		int j=l_chrpos(bihua_key,c);
		if(j<0)
			return -1;
		if(bihua_info[j].code==NULL)
			return -1;
		len+=sprintf(s+len,"%s",bihua_info[j].name);
	}
	s[len]=0;
	return len;
}

static void BihuaSetKeys(const Y_BIHUA_INFO *info)
{
	char allow[64];
	int len=1;
	allow[0]=BihuaKey;
	for(int i=0;i<32;i++)
	{
		if(info[i].code!=NULL)
		{
			allow[len++]=Y_BIHUA_KEY[i];
		}
	}
	allow[len]=0;
	l_line_edit_set_allow(&line,allow,true);
	l_line_edit_set_first(&line,BihuaKey,true);
}

static int BihuaInit(const char *arg)
{
	if(arg)
		bihua_info=(Y_BIHUA_INFO*)arg;
	else
		bihua_info=(Y_BIHUA_INFO*)y_bihua_info;
	cand_a=y_im_get_config_int("IM","cand_a");
	BihuaKey=y_im_get_key("bihua",-1,'`');
	if(!trunc)
	{
		int ret=y_bihua_load("bihua.bin");
		if(ret!=0)
			return ret;
	}

	l_line_edit_init(&line);
	l_line_edit_set_max(&line,63);
	BihuaSetKeys(bihua_info);

	return 0;
}

static int BihuaDestroy(void)
{
	bihua_info=NULL;
	return 0;
}

static void BihuaReset(void)
{
	l_line_edit_clear(&line);
	EIM.CandWordTotal=0;
	EIM.CodeInput[0]=0;
	EIM.StringGet[0]=0;
	EIM.CodeLen=0;
	EIM.CurCandPage=EIM.CandPageCount=EIM.CandWordCount=0;
	EIM.SelectIndex=0;
	PhraseListCount=0;
}

static char *BihuaGetCandWord(int index)
{	
	char *ret;
	if(index==-1) index=EIM.SelectIndex;
	if(index>=EIM.CandWordCount) return 0;
	ret=&EIM.CandTable[index][0];
	strcpy(EIM.StringGet,ret);
	return ret;
}

static int BihuaGetCandWords(int mode)
{
	int i,start,max=cand_a?cand_a:EIM.CandWordMax;
	uint32_t *res;

	if(EIM.CandPageCount==0)
		return IMR_NEXT;
	if(mode==PAGE_ASSOC)
		return IMR_NEXT;
	if(mode==PAGE_FIRST) EIM.CurCandPage=0;
	else if(mode==PAGE_NEXT) EIM.CurCandPage++;
	else if(mode==PAGE_PREV) EIM.CurCandPage--;
	if(EIM.CurCandPage>=EIM.CandPageCount || EIM.CurCandPage<0)
		EIM.CurCandPage=0;

	if(EIM.CurCandPage<EIM.CandPageCount-1)
		EIM.CandWordCount=max;
	else EIM.CandWordCount=PhraseListCount-max*(EIM.CandPageCount-1);

	start=EIM.CurCandPage*max;

	for(i=0;i<max;i++)
	{
		EIM.CodeTips[i][0]=0;
	}
	do{
		if(EIM.CodeLen==1)
		{
			if(YongGetPunc(EIM.CodeInput[0],LANG_CN,1))
			{
				sprintf(EIM.CandTable[0],"$BD(%c)",EIM.CodeInput[0]);
			}
			else
			{
				EIM.CandTable[0][0]=EIM.CodeInput[0];
				EIM.CandTable[0][1]=0;
			}
			break;
		}
		res=y_bihua_get(start,EIM.CandWordCount);
		if(!res)
		{
			printf("yong: bug at bihua get\n");
			return IMR_PASS;
		}
		for(i=0;i<EIM.CandWordCount;i++)
		{
			int len=l_char_to_gb(res[i],EIM.CandTable[i]);
			EIM.CandTable[i][len]=0;
		}
	}while(0);
	EIM.SelectIndex=0;	//reset the index to default position
	return IMR_DISPLAY;
}

static int BihuaDoSearch(void)
{
	const char *BihuaInput=l_line_edit_get_text(&line);
	int BihuaLen=l_line_edit_get_len(&line);
	int max=EIM.CandWordMax;
	if(BihuaLen==1)
	{
		EIM.CodeLen=BihuaDisplay(BihuaInput,EIM.CodeInput);
		PhraseListCount=1;
		EIM.CandWordTotal=PhraseListCount;
		EIM.CandPageCount=1;
	}
	else
	{
		int ret=y_bihua_set(BihuaInput+1);
		if(ret<=0)
		{
			l_line_edit_undo(&line);
			return IMR_BLOCK;
		}
		EIM.CodeLen=BihuaDisplay(BihuaInput,EIM.CodeInput);
		PhraseListCount=ret;
		EIM.CandWordTotal=PhraseListCount;
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	}
	BihuaGetCandWords(PAGE_FIRST);
	return IMR_DISPLAY;
}

static int BihuaDoInput(int key)
{
	if(!trunc)
		return IMR_NEXT;

	if(EIM.CodeLen==0)
	{
		Y_BIHUA_INFO *info;
		if(im.eim && im.eim->Bihua)
			info=(Y_BIHUA_INFO*)im.eim->Bihua;
		else
			info=(Y_BIHUA_INFO*)y_bihua_info;
		if(info!=bihua_info)
		{
			bihua_info=info;
			BihuaSetKeys(info);
		}
	}

	int ret=l_line_edit_push(&line,key);
	if(ret==1)
	{
		if(l_line_edit_get_len(&line)==0)
			return IMR_CLEAN;
		return BihuaDoSearch();
	}
	else
	{
		return IMR_NEXT;
	}
}

#endif
