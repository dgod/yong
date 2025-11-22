#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"
#include "ui.h"

#include "im.h"
#include "xim.h"
#include "translate.h"

static char *eim_get_config(const char *section,const char *key)
{
	static char ret[512];
	char *tmp;
	
	if(section==NULL)
	{
		tmp=y_im_get_im_config_string(im.Index,key);
	}
	else
	{
		tmp=y_im_get_config_string(section,key);
	}
	if(!tmp)
		return NULL;
	if(strlen(tmp)>=sizeof(ret)-1)
	{
		l_free(tmp);
		return NULL;
	}
	strcpy(ret,tmp);		
	l_free(tmp);

	return ret;
}



static void eim_beep(int c)
{
	if((im.Beep & (1<<c))!=0)
		y_ui_beep(c);
}

static int eim_get_key(const char *s)
{
	return y_im_str_to_key(s,NULL);
}

static int eim_callback(int index,...)
{
	va_list ap;
	int res=-1;
	va_start(ap,index);
	switch(index){
		case EIM_CALLBACK_ASYNC_WRITE_FILE:
		{
			const char *file=va_arg(ap,const char *);
			LString *data=va_arg(ap,LString*);
			bool backup=va_arg(ap,int);
			res=y_im_async_write_file(file,data,backup);
			break;
		}
		case EIM_CALLBACK_SELECT_KEY:
		{
			int key=va_arg(ap,int);
			if(!key)
			{
				const char *keys=va_arg(ap,const char*);
				va_end(ap);
				if(!keys)
					return -1;
				for(int i=0;keys[i]!=0;i++)
				{
					int index=y_im_check_select(keys[i],0xf);
					if(index>=0)
						return index+1;
				}
				return -1;
			}
			va_end(ap);
			int index=y_im_check_select(key,0xf);
			if(index<0)
				return -1;
			return index+1;
		}
		case EIM_CALLBACK_SET_ASSIST_CODE:
		{
			const char *code=va_arg(ap,const char *);
			if(code==NULL || !code[0])
			{
				im.AssistCode[0]=0;
				break;
			}
			else if(strlen(code)>sizeof(im.AssistCode)-1)
			{
				res=-1;
				break;
			}
			strcpy(im.AssistCode,code);
			break;
		}
	}
	va_end(ap);
	return res;
}

int InitExtraIM(IM *im,EXTRA_IM *eim,const char *arg)
{
	eim->CodeInput=im->CodeInputEngine;
	eim->StringGet=im->StringGetEngine;
	eim->CandTable=im->CandTableEngine;
	eim->CodeTips=im->CodeTipsEngine;
	eim->GetSelect=y_ui.get_select;
	eim->GetPath=y_im_get_path;
	eim->CandWordMax=im->CandWord;
	eim->CandWordMaxReal=im->CandWord;
	eim->CaretPos=-1;
	eim->GetConfig=eim_get_config;
	eim->GetKey=eim_get_key;
	eim->OpenFile=(void*)y_im_open_file;
	eim->SendString=(void*)y_xim_send_string2;
	eim->Beep=eim_beep;
	eim->QueryHistory=y_im_history_query;
	eim->GetLast=y_im_history_get_last;
	eim->ShowTip=y_ui.show_tip;
	eim->Translate=y_translate_get;
	eim->Log=YongLogWrite;
	eim->Callback=eim_callback;
	
	if(eim->Flag & IM_FLAG_ASYNC)
	{
		eim->Request=y_ui.request;
	}

	// wait async write file done, avoid load old file
	y_im_async_wait(500);

	if(eim->Init(arg))
	{
		printf("eim: init %s fail\n",eim->Name);
		return -1;
	}

	return 0;
}

int y_im_load_extra(IM *im,const char *name)
{
	EXTRA_IM *eim;
	const char *p;
	CONNECT_ID id={.dummy=1};
	
#ifdef CFG_XIM_WEBIM
	extern EXTRA_IM EIM;
	eim=&EIM;
#else
	p=y_im_get_config_data(name,"engine");
	if(!p)
	{
		printf("eim: im can't found config %s\n",name);
		return -1;
	}
	if(im->eim && im->handle)
	{
		im->eim->Destroy();
		im->eim=NULL;
		y_im_module_close(im->handle);
		im->handle=NULL;
	}
	im->handle=y_im_module_open(p);
	if(!im->handle)
	{
		printf("eim: open %s fail\n",p);
		y_ui_show_tip(YT("加载动态库%s失败"),p);
		return -1;
	}
	if(!(eim=y_im_module_symbol(im->handle,"EIM")) ||
		!eim || !eim->Init)
	{
		printf("eim: bad im\n");
		y_im_module_close(im->handle);
		im->handle=NULL;
		return -1;
	}
#endif
	im->eim=eim;

	p=y_im_get_config_data(name,"arg");
	if(InitExtraIM(im,eim,p))
	{
		y_im_module_close(im->handle);
		im->handle=NULL;
		im->eim=NULL;
		return -1;
	}
	id.lang=LANG_CN;
	p=y_im_get_config_data(name,"biaodian");
	if(p && !strcmp(p,"en"))
	{
		im->Biaodian=LANG_EN;
		id.biaodian=LANG_EN;
	}
	else
	{
		im->Biaodian=LANG_CN;
		id.biaodian=LANG_CN;
	}
	p=y_im_get_config_string(name,"corner");
	if(p && !strcmp(p,"full"))
	{
		id.corner=CORNER_FULL;
	}
	else
	{
		id.corner=CORNER_HALF;
	}
	im->Trad=y_im_get_config_int(name,"trad");
	im->TradDef=y_im_get_config_int("IM","s2t")?!im->Trad:im->Trad;
	id.trad=im->TradDef;
	im->Bing=y_im_get_config_int(name,"bing");
	if(im->Bing)
	{
		im->BingSkip[0]=140;im->BingSkip[1]=100;
		p=y_im_get_config_data(name,"bing_p");
		if(p)
		{
			int skip1,skip2;
			int ret=l_sscanf(p,"%d %d",&skip1,&skip2);
			if(ret==2)
			{
				im->BingSkip[0]=skip1;
				im->BingSkip[1]=skip2;
			}
		}
	}
	im->Beep=0;
	p=y_im_get_config_data(name,"beep");
	if(p)
	{
		if(strstr(p,"empty"))
			im->Beep|=1<<YONG_BEEP_EMPTY;
		if(strstr(p,"multi"))
			im->Beep|=1<<YONG_BEEP_MULTI;
	}
	y_xim_put_connect(&id);
	YongUpdateMain(&id);
	return 0;
}

int LoadExtraIM(IM *im,const char *fn)
{
	if(im->CandWord<2 || im->CandWord>10)
		im->CandWord=5;
	im->EnglishMode=0;
	
	return y_im_load_extra(im,fn);
}

static const char *fullchar[]=
{
	"　","！","”","＃","＄","％","＆","’","（","）","＊","＋",
	"，","－","．","／","０","１","２","３","４","５","６","７","８","９","：","；","＜","＝","＞","？",
	"＠","Ａ","Ｂ","Ｃ","Ｄ","Ｅ","Ｆ","Ｇ","Ｈ","Ｉ","Ｊ","Ｋ","Ｌ","Ｍ","Ｎ","Ｏ","Ｐ","Ｑ","Ｒ","Ｓ",
	"Ｔ","Ｕ","Ｖ","Ｗ","Ｘ","Ｙ","Ｚ","〔","＼","〕","︿","ˍ",
	"‘","ａ","ｂ","ｃ","ｄ","ｅ","ｆ","ｇ","ｈ","ｉ","ｊ","ｋ","ｌ","ｍ",
	"ｎ","ｏ","ｐ","ｑ","ｒ","ｓ","ｔ","ｕ","ｖ","ｗ","ｘ","ｙ","ｚ","｛","｜","｝","～"
};

const char *YongFullChar(int key)
{
	if(key < ' ' || key > '~')
		return NULL;
	return fullchar[key-' '];
}

static char punc[32]=".,?\":;'<>\\!@$^*_()[]&";
static struct{
	char punc[2][11];
	int8_t which;
}PUNC[32]={
	{{"。",""},-1},
	{{"，",""},-1},
	{{"？",""},-1},
	{{"“","”"},0},
	{{"：",""},-1},
	{{"；",""},-1},
	{{"‘","’"},0},
	{{"《",""},-1},
	{{"》",""},-1},
	{{"、",""},-1},
	{{"！",""},-1},
	//{{"·",""},-1},
	{{"@",""},-1},
	{{"￥",""},-1},
	{{"……",""},-1},
	{{"×",""},-1},
	//{{"──",""},-1},
	{{"——",""},-1},
	{{"（",""},-1},
	{{"）",""},-1},
	{{"【","】"},0},
	{{"「","」"},0},
	{{"·",""},-1},
};

void y_im_load_punc(void)
{
	FILE *fp;
	char temp[256];
	char *bd_file;

	/* remove append biaodian first */
	memset(punc+21,0,sizeof(punc)-21);
		
	bd_file=y_im_get_config_string("IM","biaodian");
	strcpy(temp,bd_file?bd_file:"bd.txt");
	if(bd_file) l_free(bd_file);

	fp=y_im_open_file(temp,"rb");
	if(!fp) return;
	while(fgets(temp,sizeof(temp),fp))
	{
		char code[8],bd1[11],bd2[8];
		char *p;
		int ret;
		if(temp[0]=='#') continue;
		ret=l_sscanf(temp,"%8s %11s %8s",code,bd1,bd2);
		if(ret<2)
			break;
		if(ret==2) bd2[0]=0;
		if(strlen(code)!=1 || strlen(bd1)>10 || strlen(bd2)>4)
			break;
		if(code[0]<=0x20 || code[0]>=0x7f)
			break;
		p=strchr(punc,code[0]);
		if(p) ret=p-punc;
		else ret=strlen(punc);
		if(ret>=31) break;
		punc[ret]=code[0];
		strcpy(PUNC[ret].punc[0],bd1);
		strcpy(PUNC[ret].punc[1],bd2);
		PUNC[ret].which=bd2[0]?0:-1;
	}
	fclose(fp);
}

const char *YongGetPunc(int key,int bd,int peek)
{
	static char bd_en[2];
	char *t;
	int i;
	key&=~KEYM_SHIFT;
	if(key & KEYM_MASK)
		return NULL;
	t=strchr(punc,(char)key);
	if(!t)
		return NULL;
	i=(int)(t-punc);
	if(!strcmp(PUNC[i].punc[0],"NONE"))
		return NULL;
	if(bd==LANG_EN)
	{
		bd_en[0]=(char)key;
		return bd_en;
	}
	switch(PUNC[i].which)
	{
		case 0:
			if(!peek) PUNC[i].which=1;
			t=PUNC[i].punc[0];
			break;
		case 1:
			if(!peek) PUNC[i].which=0;
			t=PUNC[i].punc[1];
			break;
		case -1:
			t=PUNC[i].punc[0];
			break;
	}
	return t;
}

