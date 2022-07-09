/*!
 * \author dgod
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "gbk.h"
#include "bihua.h"
#include "mapfile.h"

#include "ltricky.h"

static const char *bihua_key=Y_BIHUA_KEY;

Y_BIHUA_INFO y_bihua_info[32]={
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
static Y_BIHUA_INFO *bihua_info=y_bihua_info;

#define BIHUA_MAX	63
#define GROUP_MAX	63

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
	char str[10*4+1];
};

static void *map_file;

static int data_size;
static struct bihua_trunc *trunc;
static char *base;
static struct bihua_result res;

int y_bihua_load(const char *fn)
{
	y_bihua_free();
	map_file=y_mmap_new(fn);
	if(!map_file)
	{
		printf("yong: mmap bihua data fail\n");
		return -1;
	}
	data_size=y_mmap_length(map_file);
	trunc=(struct bihua_trunc*)y_mmap_addr(map_file);
	if(!trunc || data_size<0x10000)
	{
		y_mmap_free(map_file);
		map_file=0;data_size=0;trunc=NULL;
		printf("yong: bihua file bad\n");
		return -1;
	}
	base=(char*)(trunc+TRUNC_COUNT);

	return 0;
}

void y_bihua_free(void)
{
	if(!trunc) return;

	y_mmap_free(map_file);
	map_file=NULL;
	trunc=NULL;
	base=NULL;
	data_size=0;
}

static char *bihua_escape(char *bh)
{
	static char temp[BIHUA_MAX+1];
	int i,len,p;
	char c,*code;
	for(i=0;(c=*bh++)!=0;)
	{
		code=strchr(bihua_key,c);
		if(!code) return NULL;
		p=code-bihua_key;
		code=bihua_info[p].code;
		if(!code) return NULL;
		len=strlen(code);
		if(i+len>BIHUA_MAX) return NULL;
		strcpy(temp+i,code);
		i+=len;
	}
	if(i==0) return NULL;
	temp[i]=0;
	return temp;
}

static int bihua_test(char *bh,char *s,int len)
{
	uint16_t code;
	int bhlen;
	int ret;
	bhlen=bh[1];
	code=GBK_MAKE_CODE(bh[2],bh[3]);
	if(GBK_IS_VALID(code))
	{
		bh+=4;
		bhlen-=4;
	}
	else
	{
		bh+=6;
		bhlen-=6;
	}
#if 0
	char temp[64];
	strncpy(temp,bh,bhlen);
	temp[bhlen]=0;
	printf("%s %s %d\n",temp,s,bhlen);
#endif
	if(bhlen<len)
	{
		ret=strncmp(bh,s,bhlen);
		return (ret<=0)?-1:1;
		
	}
	else
	{
		return strncmp(bh,s,len);
	}
}

static inline uint32_t bihua_next(uint32_t offset)
{
	return offset+base[offset+1];
}

static inline uint32_t bihua_prev(uint32_t offset)
{
	return offset-base[offset];
}

int y_bihua_set(char *s)
{
	struct bihua_trunc *t;
	int i,ret,len;
	int group[2];
	uint32_t offset[2];
	int count;
	int group_max;

	s=bihua_escape(s);
	len=strlen(s);

	t=trunc+s[0]-'1';
	if(!s[1])
	{
		res.count=t->count;
		res.offset=t->offset[0];
		return res.count;
	}
	group_max=t->count/t->group+((t->count%t->group)?1:0);
	group[0]=0;
	group[1]=group_max;
	for(i=1;i<group_max;i++)
	{
		ret=bihua_test(base+t->offset[i],s,len);
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
	//printf("%d %d\n",group[0],group[1]);
	offset[0]=t->offset[group[0]];
	if(group[1]!=group_max)
	{
		count=t->group*(group[1]-group[0]);
		offset[1]=t->offset[group[1]];
		offset[1]=bihua_prev(offset[1]);
	}
	else
	{
		uint32_t next;
		int i;
		offset[1]=t->offset[group_max-1];
		count=t->count-t->group*group[0];
		for(i=0;i<t->group;i++)
		{
			next=bihua_next(offset[1]);
			if(next>=data_size || bihua_prev(next)==next)
				break;
			offset[1]=next;
		}
	}
	while(count>0)
	{
		ret=bihua_test(base+offset[0],s,len);
		if(ret==0) break;
		if(ret>0) return 0;
		count--;
		offset[0]=bihua_next(offset[0]);
	}
	while(offset[1] && count>0)
	{
		ret=bihua_test(base+offset[1],s,len);
		if(ret==0) break;
		if(ret<0) return 0;
		offset[1]=bihua_prev(offset[1]);
		count--;
	}
	res.offset=res.orig=offset[0];
	res.count=count;
	return res.count;
}

char *y_bihua_get(int at,int num)
{
	int i;
	int len;
	uint32_t offset;
	if(!res.count || num>10 || num==0)
		return NULL;
	offset=res.offset;
	for(i=0;i<at;i++)
	{
		offset=bihua_next(offset);
	}
	len=0;
	for(i=0;i<num;i++)
	{
		char *t=base+offset;
		int code=GBK_MAKE_CODE(t[2],t[3]);
		res.str[len++]=t[2];
		res.str[len++]=t[3];
		if(!GBK_IS_VALID(code))
		{
			res.str[len++]=t[4];
			res.str[len++]=t[5];
		}
		offset=bihua_next(offset);
	}
	res.str[len]=0;
	return res.str;
}



#ifndef TOOLS_BIHUA

#include "yong.h"
#include "common.h"

static int BihuaLen;
static char BihuaInput[64];

static int PhraseListCount;
static int BihuaKey;

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

static int BihuaInit(const char *arg)
{
	char temp[256];
	const char *data;

	if(arg)
		bihua_info=(Y_BIHUA_INFO*)arg;
	else
		bihua_info=(Y_BIHUA_INFO*)y_bihua_info;

	data=EIM.GetPath("DATA");
	sprintf(temp,"%s/bihua.bin",data);
	if(!l_file_exists(temp))
	{
		sprintf(temp,"%s/bihua.bin",EIM.GetPath("HOME"));
	}
	
	BihuaKey=y_im_get_key("bihua",-1,'`');
	return y_bihua_load(temp);
}

static int BihuaDestroy(void)
{
	bihua_info=y_bihua_info;
	y_bihua_free();
	return 0;
}

static void BihuaReset(void)
{
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
	int i,start,max=EIM.CandWordMax;
	char *res;

	if(EIM.CandPageCount==0)
		return IMR_NEXT;
	if(mode==PAGE_LEGEND)
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
			uint16_t code=GBK_MAKE_CODE(res[0],res[1]);
			EIM.CandTable[i][0]=*res++;
			EIM.CandTable[i][1]=*res++;
			if(!GBK_IS_VALID(code))
			{
				EIM.CandTable[i][2]=*res++;
				EIM.CandTable[i][3]=*res++;
				EIM.CandTable[i][4]=0;
			}
			else
			{
				EIM.CandTable[i][2]=0;
			}
		}
	}while(0);
	EIM.SelectIndex=0;	//reset the index to default position
	return IMR_DISPLAY;
}

static int BihuaDoSearch(void)
{
	int max=EIM.CandWordMax;
	if(EIM.CodeLen==1)
	{
		PhraseListCount=1;
		EIM.CandWordTotal=PhraseListCount;
		EIM.CandPageCount=1;
	}
	else
	{
		int ret=y_bihua_set(BihuaInput);
		if(ret<=0)
		{
			BihuaInput[--BihuaLen]=0;
			EIM.CodeLen-=2;
			EIM.CodeInput[EIM.CodeLen]=0;
			return IMR_BLOCK;
		}
		PhraseListCount=ret;
		EIM.CandWordTotal=PhraseListCount;
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	}
	BihuaGetCandWords(PAGE_FIRST);
	return IMR_DISPLAY;
}

static int BihuaDoInput(int key)
{
	int ret=IMR_DISPLAY;
	char *p;
	
	if(!trunc)
		return IMR_NEXT;

	if(key=='\b')
	{
		if(EIM.CodeLen==0)
			return IMR_CLEAN_PASS;
		EIM.CodeLen--;
		if(EIM.CodeInput[EIM.CodeLen]&0x80 && EIM.CodeLen>0)
			EIM.CodeLen--;
		EIM.CodeInput[EIM.CodeLen]=0;
		if(BihuaLen)
		{
			BihuaLen--;
			BihuaInput[BihuaLen]=0;
		}
		if(EIM.CodeLen==0)
			return IMR_CLEAN;
		return BihuaDoSearch();
	}
	else if(key==BihuaKey && (!EIM.CodeInput[0] || EIM.CodeLen==0))
	{
		EIM.CodeInput[EIM.CodeLen++]=key;
		EIM.CodeInput[EIM.CodeLen]=0;	
		BihuaLen=0;
		return BihuaDoSearch();
	}
	else if(EIM.CodeInput[0]==BihuaKey && EIM.CodeLen>0 && (p=strchr(bihua_key,key))!=NULL)
	{
		int i;
		i=p-bihua_key;
		if(bihua_info[i].code==NULL)
			return IMR_NEXT;
		strcpy(EIM.CodeInput+EIM.CodeLen,bihua_info[i].name);
		EIM.CodeLen+=2;
		BihuaInput[BihuaLen++]=key;
		BihuaInput[BihuaLen]=0;
		return BihuaDoSearch();
	}
	else
	{
		ret=IMR_NEXT;
	}
	return ret;
}

#endif
