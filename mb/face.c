#include "llib.h"
#include "yong.h"
#include "mb.h"
#include "bihua.h"
#include "gbk.h"
#include "pinyin.h"
#include "learn.h"
#include "assoc.h"
#include "cset.h"
#include "fuzzy.h"
#include "sentence.h"
#include "correct.h"

static int TableInit(const char *arg);
static void TableReset(void);
static char *TableGetCandWord(int index);
static int TableGetCandWords(int mode);
static int TableDestroy(void);
static int TableDoInput(int key);
static int TableCall(int index,...);

static void PinyinReset(void);
static char *PinyinGetCandWord(int index);
static int PinyinGetCandwords(int mode);
static int PinyinDoInput(int key);

static Y_BIHUA_INFO YongBihua[32]={
	{"山","252"},		//a
	{"八","34"},		//b
	{"艹","122"},		//c
	{"丶","4"},			//d
	{"阝","52"},		//e
	{"风","3534"},		//f
	{"工","121"},		//g
	{"一","1"},			//h
	{"长","3154"},		//i
	{"钅","31115"},		//j
	{"口","251"},		//k
	{"力","53"},		//l
	{"木","1234"},		//m
	{"女","531"},		//n
	{"月","3511"},		//o
	{"丿","3"},			//p
	{"犭","353"},		//q
	{"日","2511"},		//r
	{"纟","551"},		//s
	{"田","25121"},		//t
	{"丨","2"},			//u
	{"フ","5"},			//v
	{"夊","354"},		//w
	{"忄","442"},		//x
	{"雨","14524444"},	//y
	{"辶","454"},		//z
	{"氵","441"},		//;
	{0,0},				//'
	{0,0},				//,
	{0,0},				//.
	{"丿","3"},			//
};

L_EXPORT(EXTRA_IM EIM)={
	.Name			=	"",
	.Reset			=	TableReset,
	.DoInput		=	TableDoInput,
	.GetCandWords	=	TableGetCandWords,
	.GetCandWord	=	TableGetCandWord,
	.Init			=	TableInit,
	.Destroy		=	TableDestroy,
	.Call			=	TableCall,
};

static struct y_mb *mb;

static inline struct y_mb *Y_MB_ACTIVE(struct y_mb *mb)
{
	if(EIM.StringGet[0])
		return mb;
	const char *s=EIM.CodeInput;
	int c=s[0];
	if(!c || !s[1])
		return mb;
	if(mb->ass_mb && c==mb->ass_lead)
		return mb->ass_mb;
	if(mb->quick_mb && c==mb->quick_lead)
		return mb->quick_mb;
	return mb;
}

static char SimplePhrase[4][5];
static char SuperPhrase[MAX_CAND_LEN+1];
static uint8_t SuperCodeLen;
static char AutoPhrase[MAX_CAND_LEN+1];
static char CodeTips[10][MAX_TIPS_LEN+1];
static CSET cs;

static int InsertMode;
static int InsertWithSelect;

static int key_cnen;
static int key_select_conflict;

static int key_move_up;
static int key_move_down;

static int key_zi_switch;

static int key_backspace;
static int key_esc;
static int key_replace[8];

static int key_last;

static int key_clear_code;

static int py_switch;
static int py_switch_save;
static int py_assist_save;
static int py_assist_series;
static int py_space=' ';

static int hz_trad;
static int hz_filter;
static int hz_filter_strict;
static int hz_filter_show=0;
static int hz_filter_temp;
static int key_filter;

static short zi_mode;

static short auto_clear;
static short auto_move;
static short auto_move_len[2];
static short auto_english;
static short auto_add;

static int mb_flag;

static short assoc_mode;
static short assoc_count;
static short assoc_begin;
static short assoc_move;
static short assoc_hungry;
static short assoc_adjust;
static short assoc_select;
static void *assoc_handle;

static short tip_exist;
static short tip_simple;
static short tip_main;

static short zi_output;
static LArray *zi_output_codes;

//static int PhraseListCount;
#define PhraseListCount EIM.CandWordTotal

static int SP;

static int cand_a;

typedef struct{
	uint8_t begin;
	uint8_t end;
	uint8_t user;
#ifdef _WIN32
	// FIXME: bug of mingw or msvcrt, l_sscanf
	uint32_t resv0;
#endif
}AUTO_PHRASE_CONFIG;
static AUTO_PHRASE_CONFIG ap_conf;

static void ap_config_load(AUTO_PHRASE_CONFIG *c)
{
	const char *tmp;
	int ret;
	tmp=EIM.GetConfig(NULL,"auto_phrase");
	if(!tmp) return;
	if(strchr(tmp,','))
		ret=l_sscanf(tmp,"%hhd,%hhd,%hhd",&c->begin,&c->end,&c->user);
	else
		ret=l_sscanf(tmp,"%hhd %hhd %hhd",&c->begin,&c->end,&c->user);
	if(ret<2 || c->begin==1 || c->begin>c->end || c->end>19)
	{
		memset(c,0,sizeof(*c));
	}
}

typedef struct{
	char key[6];
	uint8_t mask;
	uint8_t action;
}BLOCK_CONFIG;
static BLOCK_CONFIG bc_conf;

static void block_config_load(BLOCK_CONFIG *c)
{
	const char *tmp;
	int ret;
	char mask[5];
	memset(c,0,sizeof(*c));
	tmp=EIM.GetConfig(NULL,"block");
	if(!tmp || !tmp[0])
		return;
	ret=l_sscanf(tmp,"%5s %4s %hhu",c->key,mask,&c->action);
	if(ret==1)
	{
		c->mask=0xff;
		return;
	}
	for(int i=0;mask[i]!=0;i++)
	{
		if(mask[i]>='1' && mask[i]<='8')
		{
			c->mask|=1<<(mask[i]-'1');
		}
	}
}

static int get_key(const char *name,int pos)
{
	int ret=0;
	const char *tmp=name;

	if(!tmp)
		return YK_NONE;
	if(!strcmp(tmp,"CTRL"))
		tmp="LCTRL RCTRL";
	else if(!strcmp(tmp,"SHIFT"))
		tmp="LSHIFT RSHIFT";
	if(pos==-1)
	{
		ret=EIM.GetKey(tmp);
	}
	else
	{
		const char *tok[pos+1];
		l_strtok(tmp,' ',tok,2);
		ret=EIM.GetKey(tok[pos]);
	}
	if(ret<0) ret=YK_NONE;
	return ret;
}

static int TableGetConfigInt(const char *section,const char *key,int def)
{
	const char *res=EIM.GetConfig(section,key);
	if(!res) return def;
	return atoi(res);
}

static int TableGetConfigKey(const char *key,int pos,int def)
{
	char *tmp=EIM.GetConfig("key",key);
	int res=get_key(tmp,pos);
	if(res<=0) res=def;
	return res;
}

static void TableInitReplaceKey(void)
{
	char *replace,**list;
	int i;
	memset(key_replace,0,sizeof(key_replace));
	replace=EIM.GetConfig("key","replace");
	if(!replace) return;
	list=l_strsplit(replace,' ');
	for(i=0;i<L_ARRAY_SIZE(key_replace) && list[i]!=NULL;i++)
	{
		key_replace[i]=EIM.GetKey(list[i]);
		if(!key_replace[i])
			break;
	}
	l_strfreev(list);
}

static inline int TableReplaceKey(int key)
{
	int t;
	if(EIM.WorkMode==EIM_WM_INSERT)
		return key;
	for(int i=0;i<L_ARRAY_SIZE(key_replace) && (t=key_replace[i])!=YK_NONE;i+=2)
	{
		if(key==t)
			return key_replace[i+1];
	}
	return key;
}

void y_mb_init_pinyin(struct y_mb *mb)
{
	char *name;
	if(!mb->pinyin)
		return;

	if(!EIM.GetConfig)
	{
		py_init(mb->split,NULL);
		if(mb->split=='\'')
			mb->trie=trie_tree_new(512*1024);
		return;
	}
	EIM.GetCandWord=PinyinGetCandWord;
	EIM.GetCandWords=PinyinGetCandwords;
	EIM.DoInput=PinyinDoInput;
	EIM.Reset=PinyinReset;
	
	py_switch=TableGetConfigKey("py_switch",-1,'\\');
	py_switch_save=TableGetConfigInt(NULL,"py_switch_save",0);
	py_assist_save=TableGetConfigInt(NULL,"py_assist_save",0);
	py_assist_series=TableGetConfigInt(NULL,"assist_series",0);
	py_space=y_mb_is_key(mb,' ')?'_':' ';
	
	name=EIM.GetConfig(0,"sp");
	if(name && name[0])
	{
		char schema[128];
		if(strcmp(name,"zrm"))
		{
			sprintf(schema,"%s/%s.sp",EIM.GetPath("HOME"),name);
			if(!l_file_exists(schema))
			{
				sprintf(schema,"%s/%s.sp",EIM.GetPath("DATA"),name);
				if(!l_file_exists(schema))
					schema[0]=0;
			}
		}
		else
		{
			strcpy(schema,"zrm");
		}
		py_init(mb->split,schema);
		if(mb->split=='\'' && schema[0])
		{
			SP=1;
			l_predict_sp=1;
			mb->ctx.sp=1;
		}
	}
	else
	{
		py_init(mb->split,NULL);
	}
	name=EIM.GetConfig(NULL,"simple");
	if(name)
	{
		l_predict_simple=atoi(name);
	}
	if(mb->split=='\'')
		mb->trie=trie_tree_new(512*1024);
}

static Y_BIHUA_INFO *TableGetBihuaConfig(void)
{
	if(mb->yong)
		return YongBihua;
	if(strspn(mb->bihua,Y_BIHUA_KEY)==5)
	{
		int i,p;
		char c,*bihua_key=Y_BIHUA_KEY;
		memset(YongBihua,0,sizeof(YongBihua));
		for(i=0;i<5;i++)
		{
			static char *name[5]={"一","丨","ノ","丶","フ"};
			static char *code[5]={"1","2","3","4","5"};
			c=mb->bihua[i];
			p=strchr(bihua_key,c)-bihua_key;
			YongBihua[p].code=code[i];
			YongBihua[p].name=name[i];
		}
		return YongBihua;
	}
	return NULL;
}

static int TableInitReal(const char *arg)
{
	char *name;
	struct y_mb_arg mb_arg;

	if(!arg)
		return -1;

	if(TableGetConfigInt("table","adict",0))
		mb_flag|=MB_FLAG_ADICT;

	if(EIM.GetConfig(NULL,"zi_mode"))
		zi_mode=TableGetConfigInt("table","zi_mode",0);
	else
		zi_mode=TableGetConfigInt("table","zi_mode",0);

	y_mb_init();
	memset(&mb_arg,0,sizeof(mb_arg));
	name=EIM.GetConfig("table","wildcard");
	if(name && !(name[0]&0x80))
	{
		mb_arg.wildcard=name[0];
	}
	mb_arg.dicts=l_strdupa(EIM.GetConfig(NULL,"dicts"));
	if(!mb_arg.dicts || !mb_arg.dicts[0])
		mb_arg.dicts=EIM.GetConfig("table","dicts");
	mb_arg.assist=l_strdupa(EIM.GetConfig(NULL,"assist"));
	mb=y_mb_new();
	if(EIM.GetConfig(NULL,"auto_phrase") || EIM.GetConfig(NULL,"auto_sentence"))
	{
		y_mb_error_init(mb);
		zi_output_codes=l_array_new(20,2);
	}
	char fn[256];
	l_utf8_to_gb(arg,fn,sizeof(fn));
	if(0!=y_mb_load_to(mb,fn,mb_flag,&mb_arg))
	{
		y_mb_free(mb);
		mb=NULL;
		return -1;
	}
	if(mb->code_hint && !zi_output_codes)
	{
		zi_output_codes=l_array_new(20,2);
	}
	if(mb->cancel)
	{
		return -1;
	}
	if(mb->pinyin)
	{
		name=EIM.GetConfig(NULL,"predict");
		if(name!=NULL)
			y_mb_learn_load(mb,name);
	}
	
	strcpy(EIM.Name,mb->name);
		
	y_mb_load_fuzzy(mb,EIM.GetConfig(0,"fuzzy"));
	y_mb_load_quick(mb,EIM.GetConfig(0,"quick"));
	y_mb_load_pin(mb,EIM.GetConfig(0,"pin"));
	
	key_cnen=TableGetConfigKey("CNen",-1,YK_LCTRL);
	key_select_conflict=EIM.Callback(EIM_CALLBACK_SELECT_KEY,0,mb->key+1);
	if(key_select_conflict<0)
		key_select_conflict=0;
	key_move_up=TableGetConfigKey("move",0,YK_NONE);
	key_move_down=TableGetConfigKey("move",1,YK_NONE);
	
	hz_trad=TableGetConfigInt(0,"trad",0);

	name=EIM.GetConfig("IM","filter");
	if(name && name[0]=='1')
	{
		hz_filter=1;
		if(name[1]==',' && name[2]=='1')
			hz_filter_strict=1;

	}
	hz_filter_temp=hz_filter;
	hz_filter_show=TableGetConfigInt("IM","filter_show",0);
	if(hz_filter)
		key_filter=TableGetConfigKey("filter",-1,'\\');
	auto_clear=TableGetConfigInt(0,"auto_clear",mb->auto_clear);
	auto_english=TableGetConfigInt(0,"auto_english",-1);
	if(auto_english<0)
		auto_english=TableGetConfigInt("table","auto_english",0);
	auto_move=TableGetConfigInt("IM","auto_move",mb->auto_move);
	// auto_move_len=TableGetConfigInt("IM","auto_move_len",0);
	if(auto_move)
	{
		name=EIM.GetConfig("IM","auto_move_len");
		if(name!=NULL)
		{
			int t0=0,t1=99;
			if(strchr(name,','))
				l_sscanf(name,"%d,%d",&t0,&t1);
			else
				l_sscanf(name,"%d %d",&t0,&t1);
			auto_move_len[0]=t0;
			auto_move_len[1]=t1;
		}
		else
		{
			auto_move_len[0]=0;
			auto_move_len[1]=99;
		}
	}
	auto_add=TableGetConfigInt("IM","auto_add",mb->pinyin);
	cand_a=TableGetConfigInt("IM","cand_a",0);
	key_zi_switch=TableGetConfigKey("zi_switch",-1,YK_NONE);
	key_backspace=TableGetConfigKey("backspace",-1,YK_NONE);
	key_esc=TableGetConfigKey("esc",-1,YK_NONE);
	key_clear_code=TableGetConfigKey("clear_code",-1,YK_NONE);
	TableInitReplaceKey();
	assoc_count=TableGetConfigInt(0,"assoc_len",0);
	if(assoc_count>0)
	{
		int assoc_save;
		assoc_begin=TableGetConfigInt(0,"assoc_begin",0);
		assoc_hungry=TableGetConfigInt(0,"assoc_hungry",0);
		assoc_move=(short)TableGetConfigInt(0,"assoc_move",0);
		assoc_save=TableGetConfigInt(0,"assoc_save",0);
		assoc_adjust=TableGetConfigInt(0,"assoc_adjust",0);
		assoc_select=TableGetConfigInt(0,"assoc_select",0);
		name=EIM.GetConfig(0,"assoc_dict");
		if(name && name[0])
		{
			assoc_handle=y_assoc_new(name,assoc_save);
		}
	}
	
	tip_exist=TableGetConfigInt(0,"tip_exist",0);
	tip_simple=TableGetConfigInt(0,"tip_simple",0);
	tip_main=TableGetConfigInt("main","tip",0);

	int auto_save=TableGetConfigInt(0,"auto_save",0);
	if(auto_save>0)
		mb->dirty_max=auto_save;

	EIM.Bihua=TableGetBihuaConfig();
	if(!mb->english)
	{
		if(SP || (mb->pinyin && mb->split==2) || mb->capital)
		{
			EIM.Flag|=IM_FLAG_CAPITAL;
		}
		else if(mb->split=='\'' && mb->pinyin)
		{
			EIM.Flag|=IM_FLAG_CAPITAL;
		}
	}
	if(!mb->english && !mb->pinyin)
	{
		if(mb->rule)
			ap_config_load(&ap_conf);
		block_config_load(&bc_conf);
	}
	sentence_init();
	correct_init();
	cset_init(&cs);

	return 0;
}

static uint8_t TableReady=0;
#if !defined(__EMSCRIPTEN__)
static int init_thread(void *arg)
{
	TableInitReal(arg);
	l_free(arg);
	TableReady=1;
	return 0;
}
#endif

static int TableInit(const char *arg)
{
#if defined(__EMSCRIPTEN__)
	TableReady=1;
	return TableInitReal(arg);
#else
	int thread=TableGetConfigInt(NULL,"thread",0);
	if(!thread)
	{
		TableReady=1;
		return TableInitReal(arg);
	}
	else
	{
		l_thrd_t th;
		l_thrd_create(&th,init_thread,l_strdup(arg));
		l_thrd_detach(th);
		return 0;
	}
#endif
}

static int TableDestroy(void)
{
	if(!TableReady)
	{
		if(mb)
		{
			mb->cancel=1;
			if(mb->ass_mb)
				mb->ass_mb->cancel=1;
		}
		do{
			l_thrd_sleep_ms(10);
		}while(!TableReady);
	}
	cset_destroy(&cs);
	y_mb_learn_free(NULL);
	y_mb_free(mb);
	mb=NULL;
	sentence_destroy();
	correct_destroy();
	y_assoc_free(assoc_handle);
	assoc_handle=NULL;
	l_array_free(zi_output_codes,NULL);
	zi_output_codes=NULL;
	y_mb_cleanup();
	return 0;
}

static void TableReset(void)
{
	EIM.CodeInput[0]=0;
	EIM.CodeLen=0;
	EIM.CurCandPage=EIM.CandPageCount=EIM.CandWordCount=0;
	EIM.SelectIndex=0;
	PhraseListCount=0;
	InsertMode=0;
	SuperPhrase[0]=0;
	hz_filter_temp=hz_filter;
	if(TableReady)
	{
		/* clean left info at mb */
		if(mb)
		{
			if(!zi_mode)
			{
				y_mb_set_zi(mb,0);
			}
			y_mb_context_reset(mb);
			y_mb_context_reset(mb->ass_mb);
			y_mb_context_reset(mb->quick_mb);
		}

		cset_reset(&cs);
	}
	key_last=0;
	assoc_mode=0;
}

static void DoTipWhenCommit(void)
{
	if(ap_conf.begin==0 && ap_conf.user)
	{
		// auto_phrase的第三个参数可用于保存手机联系人到用户词库
		struct y_mb_ci *c;
		c=y_mb_ci_exist(mb,EIM.StringGet,Y_MB_DIC_TEMP);
		if(c!=NULL)
		{
			c->dic=Y_MB_DIC_USER;
			mb->dirty++;
			y_mb_save_user(mb);
			if(EIM.ShowTip!=NULL)
				EIM.ShowTip("临时词转为用户词：%s",EIM.StringGet);
		}
	}
	if(ap_conf.begin!=0 || (tip_exist && EIM.ShowTip!=NULL) || mb->error || mb->code_hint)
	{
		int len=l_gb_strlen(EIM.StringGet,-1);
		if(len==1)
		{
			int m=MAX(ap_conf.end,9);
			zi_output++;
			if(zi_output>m) zi_output=m;
			if(zi_output_codes!=NULL)
			{
				if(l_array_length(zi_output_codes)>=20)
					l_array_remove(zi_output_codes,0);
				l_array_append(zi_output_codes,EIM.CodeInput);
			}
		}
		else
		{
			zi_output=0;
			if(zi_output_codes!=NULL)
			{
				l_array_clear(zi_output_codes,NULL);
			}
			
			if(ap_conf.user && len>=ap_conf.begin && len<=ap_conf.end)
			{
				struct y_mb_ci *c;
				c=y_mb_ci_exist(mb,EIM.StringGet,Y_MB_DIC_TEMP);
				if(c!=NULL)
				{
					c->dic=Y_MB_DIC_USER;
					mb->dirty++;
					y_mb_save_user(mb);
					if(EIM.ShowTip!=NULL)
						EIM.ShowTip("临时词转为用户词：%s",EIM.StringGet);
				}
			}
		}
	}
	/* 对连续输出单字提示已经存在的词组 */
	if(tip_exist && EIM.ShowTip!=NULL && zi_output>1)
	{
		int i;
		const char *s;
		for(i=zi_output-1;i>=1;i--)
		{
			char temp[Y_MB_DATA_SIZE+Y_MB_KEY_SIZE+3];
			char code[Y_MB_KEY_SIZE+1];
			int pos;
			s=EIM.GetLast(i);
			if(!s) continue;
			pos=sprintf(temp,"%s%s",s,EIM.StringGet);
			if(y_mb_get_exist_code(mb,temp,code)>0)
			{
				sprintf(temp+pos,"(%s)",code);
				EIM.ShowTip("词组存在：%s",temp);
				break;
			}
		}
	}
	if(ap_conf.begin!=0)
	{
		int i;
		const char *s;
		//char tip[256];
		//tip[0]=0;
		for(i=zi_output-1;i>=ap_conf.begin-1;i--)
		{
			char temp[64];
			if(i>ap_conf.end-1)
				continue;
			s=EIM.GetLast(i);
			if(!s) continue;
			sprintf(temp,"%s%s",s,EIM.StringGet);
			if(y_mb_error_has(mb,temp))
			{
				continue;
			}
			if(y_mb_get_exist_code(mb,temp,NULL)<=0)
			{
				char code[Y_MB_KEY_SIZE+1];
				char *outs[]={code,NULL};
				char hint[i+1][2];
				memcpy(hint[0],l_array_nth(zi_output_codes,zi_output-i-1),2*(i+1));
				if(0==y_mb_code_by_rule(mb,temp,strlen(temp),outs,hint) &&
					0==y_mb_add_phrase(mb,code,temp,Y_MB_APPEND,Y_MB_DIC_TEMP))
				{
					/*if(tip[0]==0)
					{
						strcpy(tip,EIM.Translate("添加临时词："));
						strcat(tip,temp);
					}
					else
					{
						strcat(tip,"，");
						strcat(tip,temp);
					}*/
				}
			}
		}
		//if(tip[0]!=0 && EIM.ShowTip!=NULL)
		//	EIM.ShowTip(tip);
	}
	if(tip_simple && EIM.ShowTip!=NULL)
	{
		if(l_gb_strlen(EIM.StringGet,-1)==1)
		{
			int ret;
			char temp[MAX_CODE_LEN+1];
			ret=y_mb_find_simple_code(mb,EIM.StringGet,EIM.CodeInput,temp,hz_filter,tip_simple);
			if(ret==1)
			{
				EIM.ShowTip("简码存在：%s",temp);
			}
		}
	}
}

static char *TableGetCandWord(int index)
{
	char *ret,*tip;

	if(index>=EIM.CandWordCount)
		return NULL;
	if(index==-1)
		index=EIM.SelectIndex;

	if(unlikely(index>=0))
	{
		ret=&EIM.CandTable[index][0];
		tip=&CodeTips[index][0];
	}
	else
	{
		ret=l_alloca(MAX_CAND_LEN+1);
		tip=l_alloca(MAX_CODE_LEN+1);
		if(1!=cset_output(&cs,-index-2,1,(void*)ret,(void*)tip))
			return NULL;
	}

	// 手工加词	
	if(InsertMode)
	{
		if(index!=EIM.SelectIndex)
		{
			EIM.SelectIndex=index;
			TableGetCandWords(PAGE_REFRESH);
		}
		else if(EIM.CodeInput[0])
		{
			y_mb_add_phrase(mb,EIM.CodeInput,ret,Y_MB_APPEND,Y_MB_DIC_USER);
			TableReset();
			EIM.StringGet[0]=0;
			y_mb_error_del(mb,ret);
		}
		return NULL;
	}
	strcpy(EIM.StringGet,ret);

	// 联想词组调频
	if(assoc_mode && assoc_handle && assoc_move)
	{
		y_assoc_move(assoc_handle,ret);
	}

	// 词组自动调频
	if(auto_move && 
			EIM.CodeLen>=auto_move_len[0] && EIM.CodeLen<=auto_move_len[1] && 
			(EIM.CurCandPage!=0 || index!=0) && 
			EIM.CodeInput[0]!=mb->ass_lead)
	{
		char code[Y_MB_KEY_SIZE+1];
		if(y_mb_has_wildcard(mb,EIM.CodeInput))
		{
			strcpy(code,tip);
		}
		else
		{
			strcpy(code,EIM.CodeInput);
			strcat(code,tip);
		}
		y_mb_auto_move(mb,code,EIM.StringGet,auto_move);
	}
	
	DoTipWhenCommit();

	return EIM.StringGet;
}

static int TableUpdateAssoc(int *mode)
{
	const char *from;
	int from_len;
	from=EIM.StringGet;
	if(from[0]=='$' && from[1]=='[')
	{
		from=y_mb_skip_display(from,-1);
		if(!from)
			return IMR_BLOCK;
		from++;
	}
	from_len=l_gb_strlen(from,-1);
	if(from_len<assoc_begin)
	{
		if(assoc_hungry>=assoc_begin)
			from_len=assoc_begin;
		else
			return IMR_BLOCK;
	}
	int i=MAX(assoc_hungry,from_len);
	CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
	if(mode!=NULL)
	{
		for(;i>=from_len && !g->count;i--)
		{
			from=EIM.GetLast(i);
			if(!from) continue;
			if(assoc_handle)
			{
				g->count=y_assoc_get(
					assoc_handle,from,strlen(from),
					assoc_count,g->phrase,g->size);
			}
			else
			{
				g->count=y_mb_get_assoc(
					mb,from,strlen(from),
					assoc_count,g->phrase,Y_MB_DATA_CALC);
			}
		}
		if(!assoc_hungry)
			g->count+=EIM.QueryHistory(EIM.StringGet,g->phrase+g->count,Y_MB_DATA_CALC-g->count);
	}
	else
	{
		for(;i>=from_len && !g->count;i--)
		{
			const char *h=EIM.GetLast(i-from_len);
			if(!h)
				continue;
			char temp[256];
			sprintf(temp,"%s%s",h,from);
			if(assoc_handle)
			{
				g->count=y_assoc_get(
					assoc_handle,temp,strlen(temp),
					assoc_count,g->phrase,Y_MB_DATA_CALC);
			}
			else
			{
				g->count=y_mb_get_assoc(
					mb,temp,strlen(temp),
					assoc_count,g->phrase,Y_MB_DATA_CALC);
			}
		}
	}
	if(!g->count)
		return IMR_BLOCK;
	if(mode!=NULL)
	{
		EIM.CodeLen=0;
		EIM.CodeInput[0]=0;
		strcpy(EIM.StringGet,EIM.Translate(hz_trad?"相關字詞：":"联想："));
		PhraseListCount=g->count;
		EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+((PhraseListCount%EIM.CandWordMax)?1:0);
		*mode=PAGE_FIRST;
		assoc_mode=1;
	}
	cset_prepend(&cs,(CSET_GROUP*)g);
	if(assoc_adjust)
	{
		cset_set_assoc(&cs,g->phrase,g->count);
	}
	return IMR_NEXT;
}

static int TableGetCandWords(int mode)
{
	struct y_mb *active;
	int i,start,max;
	
	if(mode==PAGE_ASSOC)
		active=mb;
	else
		active=Y_MB_ACTIVE(mb);

	if(active==mb->ass_mb || active==mb->quick_mb)
		EIM.WorkMode=EIM_WM_ASSIST;
	max=EIM.CandWordMax;
	if((mb->ass_mb==active || mb->quick_mb==active) && cand_a)
		max=cand_a;
	EIM.CandWordMaxReal=max;
	if(mode==PAGE_FIRST)
	{
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	}
	if(mode==PAGE_ASSOC)
	{
		int ret=TableUpdateAssoc(&mode);
		if(ret!=IMR_NEXT)
			return ret;
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
		CodeTips[i][0]=0;
		EIM.CodeTips[i][0]=0;
	}

	cset_output(&cs,start,EIM.CandWordCount,EIM.CandTable,CodeTips);

	if(EIM.SelectIndex>=EIM.CandWordCount)
		EIM.SelectIndex=0;
	
	if(active->hint!=0)
	for(i=0;i<EIM.CandWordCount;i++)
		strcpy(EIM.CodeTips[i],CodeTips[i]);
	if(active==mb->ass_mb) /* it's assist mb */
	{
		/* hint the main mb's code */
		for(i=0;i<EIM.CandWordCount;i++)
		{
			int len;
			char *data=EIM.CandTable[i];
			EIM.CodeTips[i][0]=0;
			len=strlen(data);
			if((len==2 && gb_is_gbk((uint8_t*)data)) ||
					(len==4 && gb_is_gb18030_ext((uint8_t*)data)))
			{
				y_mb_get_full_code(mb,data,EIM.CodeTips[i]);
			}
			else
			{
				y_mb_get_exist_code(mb,data,EIM.CodeTips[i]);
			}
		}
	}
	else if(active==mb->quick_mb)
	{
		for(i=0;i<EIM.CandWordCount;i++)
		{
			EIM.CodeTips[i][0]=0;
		}
	}

	if(active==mb && mb->sloop)
	{
		int prev_tip_len=-1;
		int sloop=EIM.CandWordCount;
		if(mb->sloop>1)
			sloop=MIN(EIM.CandWordCount,mb->sloop);
		SimplePhrase[EIM.CodeLen][0]=0;
		for(i=0;i<sloop;i++)
		{
			int len;
			char *data=EIM.CandTable[i];
			char *tip=EIM.CodeTips[i];
			len=strlen(data);
			if(!(len==2 && gb_is_gbk((uint8_t*)data)))
				break;
			len=strlen(tip);
			if(!prev_tip_len && len)
				break;
		}
		if(i==1 || (i>1 && EIM.CodeLen==1))
		{
			strcpy(SimplePhrase[EIM.CodeLen-1],EIM.CandTable[0]);
		}
		else if(i>0)
		{
			int count=i,j;
			for(i=0;i<count;i++)
			{
				for(j=0;j<EIM.CodeLen-1;j++)
				{
					if(!strcmp(SimplePhrase[j],EIM.CandTable[i]))
					{
						break;
					}
				}
				if(j==EIM.CodeLen-1) /* if not prev out */
				{
					/* not self and  c[0] is not full or c[j] is full here */
					//printf("%s %s\n",EIM.CodeTips[0],EIM.CodeTips[i]);
					if(i!=0 && (EIM.CodeTips[0][0] || !EIM.CodeTips[i][0]))
					{
						char temp[MAX_CAND_LEN+1];
						strcpy(temp,EIM.CandTable[i]);
						strcpy(EIM.CandTable[i],EIM.CandTable[0]);
						strcpy(EIM.CandTable[0],temp);
						strcpy(temp,EIM.CodeTips[i]);
						strcpy(EIM.CodeTips[i],EIM.CodeTips[0]);
						strcpy(EIM.CodeTips[0],temp);
					}
					break;
				}
			}
			strcpy(SimplePhrase[EIM.CodeLen-1],EIM.CandTable[0]);
		}
	}
	if(mb->yong && (EIM.CodeLen==2 || EIM.CodeLen==3))
	{
		for(i=!EIM.CurCandPage;i<EIM.CandWordCount;i++)
		{
			if(CodeTips[i][0]) continue;
			y_mb_calc_yong_tip(mb,EIM.CodeInput,EIM.CandTable[i],EIM.CodeTips[i]);
		}
	}
	if(InsertMode)
	{
		CSET_GROUP_CALC *g=(CSET_GROUP_CALC*)cset_get_group_by_type(&cs,CSET_TYPE_CALC);
		assert(g!=NULL);
		int pos=EIM.SelectIndex>=0?EIM.CurCandPage*EIM.CandWordMax+EIM.SelectIndex:-EIM.SelectIndex-2;
		char *outs[]={EIM.CodeInput,NULL};
		int len=l_gb_strlen(g->phrase[pos],-1);
		if((pos!=0 || InsertWithSelect==0) && zi_output_codes && zi_output>0 && len<20)
		{
			char hint[20][2]={0};
			if(len>=zi_output)
				memcpy(hint[len-zi_output],l_array_nth(zi_output_codes,0),zi_output*2);
			else
				memcpy(hint[0],l_array_nth(zi_output_codes,zi_output-len),len*2);
			if(0==y_mb_code_by_rule(mb,g->phrase[pos],strlen(g->phrase[pos]),outs,hint))
				EIM.CodeLen=strlen(EIM.CodeInput);
		}
		else
		{
			if(0==y_mb_code_by_rule(mb,g->phrase[pos],strlen(g->phrase[pos]),outs,NULL))
				EIM.CodeLen=strlen(EIM.CodeInput);
		}
	}

	return IMR_DISPLAY;
}

static int TableOnVirtQuery(const char *ph)
{
	int count;
	char tmp[Y_MB_DATA_SIZE+1];
	
	if(!ph || strpbrk(ph,"\r\n\t\v\b") || strlen(ph)>64)
	{
		return IMR_BLOCK;
	}
	
	count=l_gb_strlen(ph,-1);
	strcpy(tmp,ph);

	CSET_GROUP_CALC *g=(CSET_GROUP_CALC*)cset_calc_group_new(&cs);
	if(count==1)
		g->count=y_mb_find_code(mb,tmp,g->phrase,Y_MB_DATA_CALC);
	else if(count>1)
		g->count=y_mb_get_exist_code(mb,tmp,g->phrase[0]);
	else
		g->count=0;
	if(g->count)
	{
		EIM.WorkMode=EIM_WM_QUERY;
		strcpy(EIM.StringGet,EIM.Translate("反查："));
		strcpy(EIM.CodeInput,tmp);
		EIM.CaretPos=-1;
		PhraseListCount=g->count;
		cset_prepend(&cs,(CSET_GROUP*)g);
		TableGetCandWords(PAGE_FIRST);
		return IMR_DISPLAY;
	}
	else
	{

		return IMR_BLOCK;
	}
}

static int TableOnVirtAdd(const char *ph)
{
	CSET_GROUP_CALC *g=(CSET_GROUP_CALC*)cset_calc_group_new(&cs);
	if(ph && (strpbrk(ph,"\r\n\t\v\b") || strlen(ph)>64))
	{
		ph=NULL;
	}
	if(ph)
	{
		strcpy(g->phrase[g->count++],ph);
	}
	InsertWithSelect=(ph!=NULL);
	if(EIM.DoInput==TableDoInput)
	{
		int i;
		for(i=2;i<10;i++)
		{
			const char *p=EIM.GetLast(i);
			if(!p) break;
			if(ph && !strcmp(ph,p))
				continue;
			strcpy(g->phrase[g->count++],p);
		}
	}
	
	if(g->count)
	{
		strcpy(EIM.StringGet,EIM.Translate(hz_trad?"編碼：":"编码："));
		cset_prepend(&cs,(CSET_GROUP*)g);
		PhraseListCount=cset_count(&cs);
		InsertMode=1;
		EIM.GetCandWords(PAGE_FIRST);
		EIM.WorkMode=EIM_WM_INSERT;
		return IMR_DISPLAY;
	}
	else
	{
		return IMR_NEXT;
	}
}

static struct y_mb *ShouldEnterAssistMode(int key)
{
	if(EIM.StringGet[0])
		return NULL;
	const char *s=EIM.CodeInput;
	if(s[0]==0)
	{
		if(key==mb->ass_lead)
			return mb->ass_mb;
		if(key==mb->quick_lead)
			return mb->quick_mb;
		return NULL;
	}
	if(s[0]==mb->ass_lead)
	{
		if(key && !y_mb_is_key(mb->ass_mb,key))
			return NULL;
		return mb->ass_mb;
	}
	else if(s[0]==mb->quick_lead)
	{
		if(key && !y_mb_is_key(mb->quick_mb,key))
			return NULL;
		return mb->quick_mb;
	}
	return NULL;
}

static int TableAssistDoSearch(struct y_mb *active_mb)
{
	PhraseListCount=y_mb_set(active_mb,EIM.CodeInput+1,EIM.CodeLen-1,0);
	cset_mb_group_set(&cs,active_mb,PhraseListCount);
	PhraseListCount=cset_count(&cs);
	int max=EIM.CandWordMax;
	EIM.CandWordMaxReal=max;
	EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	TableGetCandWords(PAGE_FIRST);
	char *p=EIM.CandTable[0];
	if(PhraseListCount==1 && l_str_has_suffix(p,"$SPACE"))
	{
		size_t len=strlen(p);
		if(len!=6 && !strstr(p,"$$"))
		{
			p[len-6]=0;
			strcpy(EIM.StringGet,p);
			return IMR_COMMIT;
		}
	}
	return IMR_DISPLAY;
}

static int TableDoInput(int key)
{
	int ret=IMR_DISPLAY;
	struct y_mb *active_mb;
	int bing,space=0;
	
	if(!TableReady)
		return IMR_NEXT;

	bing=key&KEYM_BING;
	key&=~KEYM_BING;
	if(bing && key_last==key)
		space=1;
	key_last=key;
	if(key==key_backspace)
		key='\b';
	key=TableReplaceKey(key);
	if(key==YK_BACKSPACE || key==(YK_BACKSPACE|KEYM_SHIFT))
	{
		if(assoc_mode || EIM.WorkMode==EIM_WM_QUERY)
			return IMR_CLEAN;
		if(EIM.CodeLen==0)
		{
			if(InsertMode)
			{
				InsertMode=0;
				return IMR_CLEAN;
			}
			return IMR_NEXT;
		}
		EIM.CodeLen--;
		if(EIM.CodeInput[EIM.CodeLen]&0x80 && EIM.CodeLen>0)
			EIM.CodeLen--;
		EIM.CodeInput[EIM.CodeLen]=0;
		if(EIM.CodeLen==0 && EIM.StringGet[0]==0)
			return IMR_CLEAN;
		if(hz_filter_show) hz_filter_temp=hz_filter;
		active_mb=Y_MB_ACTIVE(mb);
		if(active_mb!=mb && active_mb)
			return TableAssistDoSearch(active_mb);
		goto LIST;
	}
	else if(key==YK_ENTER && InsertMode)
	{
		y_mb_error_del(mb,EIM.CandTable[EIM.SelectIndex]);
		y_mb_add_phrase(mb,EIM.CodeInput,EIM.CandTable[EIM.SelectIndex],Y_MB_APPEND,Y_MB_DIC_USER);
		TableReset();
		return IMR_CLEAN;
	}
	else if(InsertMode && (key==YK_UP || key==YK_DOWN))
	{
		if(key==YK_UP)
		{
			if(EIM.SelectIndex>0)
			{
				EIM.SelectIndex=EIM.SelectIndex-1;
			}
			else if(EIM.CurCandPage>0)
			{
				EIM.SelectIndex=EIM.CandWordMax-1;
				TableGetCandWords(PAGE_PREV);
			}
		}
		else if(key==YK_DOWN)
		{
			if(EIM.SelectIndex<EIM.CandWordCount-1)
			{
				EIM.SelectIndex=EIM.SelectIndex+1;
			}
			else if(EIM.CurCandPage<EIM.CandPageCount-1)
			{
				EIM.SelectIndex=0;
				TableGetCandWords(PAGE_NEXT);
			}
		}
		TableGetCandWords(PAGE_REFRESH);
	}
	else if(InsertMode && key==key_clear_code && EIM.CodeLen>0)
	{
		EIM.CodeLen=0;
		EIM.CaretPos=0;
		EIM.CodeInput[0]=0;
		goto LIST;
	}
	else if(key==YK_VIRT_REFRESH)
	{
		if(y_mb_is_keys(mb,EIM.CodeInput))
			goto LIST;
		return IMR_NEXT;
	}
	else if(key==YK_VIRT_ADD)
	{
		char *ph=NULL;
		if(InsertMode || EIM.CodeLen)
		{
			return IMR_BLOCK;
		}
		if(EIM.GetSelect!=NULL)
		{
			ph=EIM.GetSelect(TableOnVirtAdd);
			if(ph==NULL)
				return IMR_BLOCK;
		}
		return TableOnVirtAdd(ph);
	}
	else if(key==YK_VIRT_DEL)
	{
		char code[Y_MB_KEY_SIZE+1];
		if(InsertMode)
			return IMR_BLOCK;
		if(EIM.CodeLen==0)
			return IMR_NEXT;
		if(y_mb_has_wildcard(mb,EIM.CodeInput))
		{
			strcpy(code,CodeTips[EIM.SelectIndex]);
		}
		else
		{
			strcpy(code,EIM.CodeInput);
			strcat(code,CodeTips[EIM.SelectIndex]);
		}
		int d0=sentence_del(EIM.CandTable[EIM.SelectIndex]);
		int d1=y_mb_del_phrase(mb,code,EIM.CandTable[EIM.SelectIndex]);
		if(d0!=0 && d1!=0)
			return IMR_BLOCK;
		TableReset();
		return IMR_CLEAN;
	}
	else if(key==YK_VIRT_QUERY)
	{
		char *ph=NULL;
		if(EIM.CandWordCount)
			ph=EIM.CandTable[EIM.SelectIndex];
		else if(!EIM.CodeLen && EIM.GetSelect)
		{
			char *temp=EIM.GetSelect(TableOnVirtQuery);
			if(temp==NULL)
				return IMR_BLOCK;
			ph=l_strdupa(temp);
			l_free(temp);
		}
		return TableOnVirtQuery(ph);
	}
	else if(key==YK_VIRT_TIP)
	{
		int i=0;
		for(i=0;i<EIM.CandWordCount;i++)
		{
			if(l_gb_strlen(EIM.CandTable[i],-1)!=1)
				continue;
			y_mb_get_full_code(mb,EIM.CandTable[i],EIM.CodeTips[i]);
		}
		return IMR_DISPLAY;
	}
	else if(key==key_move_up || key==key_move_down)
	{
		char code[Y_MB_KEY_SIZE+1];
		int ret;
		if(InsertMode)
			return IMR_BLOCK;
		if(EIM.CandWordCount==0)
			return IMR_NEXT;
		if(y_mb_has_wildcard(mb,EIM.CodeInput))
		{
			strcpy(code,CodeTips[EIM.SelectIndex]);
		}
		else
		{
			strcpy(code,EIM.CodeInput);
			strcat(code,CodeTips[EIM.SelectIndex]);
		}
		ret=y_mb_move_phrase(mb,code,EIM.CandTable[EIM.SelectIndex],(key==key_move_up)?-1:1);
		if(ret==-1)
			return IMR_BLOCK;
		if(CodeTips[EIM.SelectIndex][0]=='\0')
		{
			char phrase[Y_MB_DATA_SIZE+1];
			int pos=EIM.CandWordMaxReal*EIM.CurCandPage+EIM.SelectIndex;
			y_mb_get(mb,pos,1,&phrase,&code);
			if(strcmp(EIM.CandTable[EIM.SelectIndex],phrase))
			{
				if(key==key_move_up)
				{
					if(EIM.SelectIndex>0)
					{
						EIM.SelectIndex--;
					}
					else
					{
						EIM.CurCandPage--;
						EIM.SelectIndex=EIM.CandWordMaxReal-1;
					}
				}
				else
				{
					if(EIM.SelectIndex==EIM.CandWordMaxReal-1)
					{
						EIM.SelectIndex=0;
						EIM.CurCandPage++;
					}
					else
					{
						EIM.SelectIndex++;
					}
				}
			}
		}
		TableGetCandWords(PAGE_REFRESH);
		return IMR_DISPLAY;
	}
	else if(key==key_filter && (EIM.CodeLen!=0 || key>=0x80) && !InsertMode)
	{
		if(EIM.CodeLen==0 && key>=0x80)
		{
			hz_filter=!hz_filter;
			hz_filter_temp=hz_filter;
			EIM.ShowTip("汉字过滤：%s",hz_filter?"开启":"关闭");
			return IMR_BLOCK;
		}
		hz_filter=1;
		if(!hz_filter_temp)
			hz_filter_temp=hz_filter;
		else
			hz_filter_temp=0;
		goto LIST;
	}
	else if(mb->yong && bing && space && EIM.CodeLen==3)
	{
		char code[2]={EIM.CodeInput[EIM.CodeLen-1],0};
		EIM.CodeInput[--EIM.CodeLen]=0;
		ret=y_mb_get_simple(mb,EIM.CodeInput,EIM.StringGet,EIM.CodeLen);
		if(ret<0) return IMR_CLEAN;
		ret=y_mb_get_simple(mb,code,EIM.StringGet+strlen(EIM.StringGet),1);
		if(ret<0) return IMR_CLEAN;
		return IMR_COMMIT;
	}
	else if(mb->yong && key==YK_TAB && EIM.CodeLen>0 && !InsertMode)
	{
		if(mb->commit_which && mb->commit_which<EIM.CodeLen && EIM.CodeLen==mb->len)
		{
			active_mb=Y_MB_ACTIVE(mb);
			goto commit_simple;
		}
		if(mb->suffix!=YK_TAB && mb->suffix!=-1)
			return IMR_NEXT;
		goto commit_suffix;
	}
	else if(key==mb->suffix && EIM.CurCandPage==0 && EIM.CodeLen>=1 && EIM.CodeLen<8 && !InsertMode)
	{
		char code[8];
		struct y_mb_context ctx;
		char temp[2][MAX_CAND_LEN+1];
		int ret;
commit_suffix:
		temp[0][0]=temp[1][0]=0;
		y_mb_push_context(mb,&ctx);	
		EIM.StringGet[0]=0;	
		if(EIM.CodeLen>1)
		{
			int clen=EIM.CodeLen-1;
			l_strncpy(code,EIM.CodeInput,clen);
			ret=y_mb_set(mb,code,clen,hz_filter_temp);
			if(ret==0)
			{
				y_mb_pop_context(mb,&ctx);
				return IMR_BLOCK;
			}
			y_mb_get(mb,0,1,&temp[0],NULL);
		}
		code[0]=EIM.CodeInput[EIM.CodeLen-1];
		if(key!=mb->wildcard && key>=YK_SPACE)
		{
			code[1]=key;
			code[2]=0;
			ret=y_mb_set(mb,code,2,hz_filter_temp);
		}
		else
		{
			ret=0;
		}
		if(ret>0)
		{
			y_mb_get(mb,0,1,&temp[1],NULL);
		}
		else
		{
			code[1]='\0';
			ret=y_mb_set(mb,code,1,hz_filter_temp);
			if(ret==0)
			{
				y_mb_pop_context(mb,&ctx);
				return IMR_BLOCK;
			}
			y_mb_get(mb,0,1,&temp[1],NULL);
		}
		y_mb_pop_context(mb,&ctx);
		snprintf(EIM.StringGet,MAX_CAND_LEN,"%s%s",temp[0],temp[1]);
		return IMR_COMMIT;
	}
	else if(y_mb_is_key(mb,key) && y_mb_is_keys(mb,EIM.CodeInput) &&
			!(EIM.CodeLen>=1 && (EIM.CodeInput[0]==mb->quick_lead || EIM.CodeInput[0]==mb->ass_lead)))
	{
		int count;

		if(EIM.CodeLen>=MAX_CODE_LEN)
			return IMR_BLOCK;
		if(!EIM.CodeLen && !InsertMode)
		{
			if(assoc_mode && key_select_conflict && assoc_select)
			{
				int select=EIM.Callback(EIM_CALLBACK_SELECT_KEY,key);
				if(select>=0 && EIM.CandWordCount>=select)
				{
					if(assoc_select==1 || (assoc_select==2 && !strchr(";\',./",key)))
					{
						return IMR_NEXT;
					}
					if(assoc_select==2)
					{
						TableReset();
					}
				}
			}
			cset_clear(&cs,CSET_TYPE_CALC);
			EIM.StringGet[0]=0;
			EIM.SelectIndex=0;
			mb->ctx.input[0]=0;
			assoc_mode=0;
		}
		/* make sure ? will first to be the biaodian */
		if(key==mb->wildcard && key=='?' && EIM.CodeLen==0)
			return IMR_NEXT;

		active_mb=Y_MB_ACTIVE(mb);
		
		if(mb==active_mb && mb->yong && !InsertMode)
		{
			if(EIM.CodeLen==4 && key=='\'')
			{
				count=mb->ctx.result_count_zi;
				if(!count)
					return IMR_BLOCK;
				if(count>=2 && (mb->ctx.result_filter_zi || mb->ctx.result_count==count))
				{
					strcpy(EIM.StringGet,EIM.CandTable[1]);
					return IMR_COMMIT;
				}
				y_mb_set_zi(mb,1);
				cset_mb_group_set(&cs,active_mb,count);
				PhraseListCount=cset_count(&cs);
				TableGetCandWords(PAGE_FIRST);
				return IMR_DISPLAY;
			}
			else if(EIM.CodeLen==4)
			{
				strcpy(SuperPhrase,EIM.CandTable[EIM.SelectIndex]);
				SuperCodeLen=4;
				CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
				g->count=y_mb_super_get(mb,g->phrase,Y_MB_DATA_CALC,key);
				CSET_GROUP *st=sentence_get(EIM.CodeInput,mb,key);
				if(g->count>0 || st!=NULL)
				{
					EIM.CodeInput[EIM.CodeLen++]=key;
					EIM.CodeInput[EIM.CodeLen]=0;
					cset_clear(&cs,CSET_TYPE_MB);
					cset_clear(&cs,CSET_TYPE_SENTENCE);
					if(g->count>0)
						cset_prepend(&cs,(CSET_GROUP*)g);
					if(st!=NULL)
						cset_append(&cs,st);
					PhraseListCount=cset_count(&cs);
					TableGetCandWords(PAGE_FIRST);
					if(mb->commit_mode==3 && PhraseListCount==1)
					{
						strcpy(EIM.StringGet,EIM.CandTable[0]);
						return IMR_COMMIT;
					}
					return IMR_DISPLAY;
				}
			}
			else if(EIM.CodeLen==5)
			{
				char temp_code[3];
				int temp_count;
				temp_code[0]=EIM.CodeInput[4];temp_code[1]=key;temp_code[2]=0;
				if((temp_code[0]==mb->quick_lead && mb->quick_lead0) ||
						(temp_code[0]==mb->ass_lead && mb->ass_lead0))
				{
					temp_count=0;
				}
				else
				{
					temp_count=y_mb_set(mb,temp_code,2,hz_filter_temp);
				}
				if(temp_count)
				{
					strcpy(EIM.StringGet,SuperPhrase);
					EIM.CodeInput[0]=EIM.CodeInput[4];
					EIM.CodeInput[1]=0;
					EIM.CodeLen=1;
				}
				else
				{
					int select=key_select_conflict?EIM.Callback(EIM_CALLBACK_SELECT_KEY,key):0;
					if((select>0 && EIM.CandWordCount>=select) || strchr(mb->key0,key))
					{
						return IMR_NEXT;
					}
					strcpy(EIM.StringGet,EIM.CandTable[EIM.SelectIndex]);
					EIM.CodeLen=0;
				}
				cset_clear(&cs,CSET_TYPE_CALC);
				hz_filter_temp=hz_filter;
				ret=IMR_COMMIT_DISPLAY;
				DoTipWhenCommit();
			}
			else
			{
				y_mb_set_zi(mb,0);
			}	
		}
		if(InsertMode)
		{
			if(key==mb->wildcard)
				return IMR_BLOCK;
		}

		EIM.CodeInput[EIM.CodeLen++]=key;
		EIM.CodeInput[EIM.CodeLen]=0;

		if(mb->fuzzy)
			EIM.CodeLen=fuzzy_correct(mb->fuzzy,EIM.CodeInput,EIM.CodeLen);

LIST:
		if(InsertMode)
		{
			EIM.CaretPos=EIM.CodeLen;
			return IMR_DISPLAY;
		}
		active_mb=Y_MB_ACTIVE(mb);
		y_mb_set_zi(mb,zi_mode & 0x01);
		y_mb_set_ci_ext(mb,zi_mode & 0x01);
	
		mb->ctx.result_filter_ext=0;
		if(hz_filter_show && hz_filter && !hz_filter_temp)
		{
			/* 只显示非常用汉字，这里使用result_match以获得最精确的匹配 */
			int c1,c2;
			if(mb->match) mb->ctx.result_match=1;
			c1=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,1);
			c2=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,0);
			mb->ctx.result_match=0;
			if(c2>c1)
			{
				count=c2-c1;
				mb->ctx.result_filter_ext=1;
			}
			else if(c2==0)
			{
				/* 如果发生空码，则在普通match=1下查询，以便后续非常用字能打出 */
				c1=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,1);
				c2=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,0);
				if(c2>c1)
				{
					count=c2-c1;
					mb->ctx.result_filter_ext=1;
				}
				else
				{
					count=c2;
					mb->ctx.result_filter_ext=0;
				}
			}
			else
			{
				count=c2;
				mb->ctx.result_filter_ext=0;
			}
		}
		else /* normal mode */
		{
			count=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,hz_filter_temp);
		}
		if(hz_filter_temp && !count && !hz_filter_strict)
		{
			hz_filter_temp=0;
			count=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,hz_filter_temp);
			if(!count && hz_filter_show)
				hz_filter_temp=hz_filter;
		}
		if(count==0 && assoc_adjust && cset_has_assoc(&cs,EIM.CodeInput))
		{
			cset_mb_group_set(&cs,active_mb,0);
			PhraseListCount=cset_count(&cs);
			TableGetCandWords(PAGE_FIRST);
			return IMR_DISPLAY;
		}
		if(count==0 && EIM.CodeLen==1 && ShouldEnterAssistMode(0))
		{
			PhraseListCount=0;
			TableGetCandWords(PAGE_FIRST);
			return IMR_DISPLAY;
		}
		if(count==0 && correct_enabled())
		{
			correct_run(EIM.CodeInput,mb,hz_filter_temp,&count);
			if(hz_filter_temp && !count && !hz_filter_strict)
			{
				hz_filter_temp=0;
				correct_run(EIM.CodeInput,mb,hz_filter_temp,&count);
				if(!count && hz_filter_show)
					hz_filter_temp=hz_filter;
			}
		}
		if(count<=0 && key==YK_VIRT_REFRESH)
		{
			return IMR_ENGLISH;
		}
		{
			CSET_GROUP *st=sentence_get(EIM.CodeInput,mb,0);
			cset_clear(&cs,CSET_TYPE_SENTENCE);
			if(st)
			{
				cset_append(&cs,st);
				goto DISPLAY;
			}
		}
		while(count<=0)
		{
			if(key==key_esc && EIM.CodeLen>1)
			{
				return IMR_CLEAN;
			}
			if(bc_conf.key[0] && strchr(bc_conf.key,key) && (bc_conf.mask&(1<<(EIM.CodeLen-1))))
			{
				EIM.CodeInput[--EIM.CodeLen]=0;
				return bc_conf.action;
			}
			int full=y_mb_is_full(mb,EIM.CodeLen);
			int select=key_select_conflict?EIM.Callback(EIM_CALLBACK_SELECT_KEY,key):0;
			if((select>0 && EIM.CandWordCount>=select) || strchr(mb->key0,key))
			{
				EIM.CodeInput[--EIM.CodeLen]=0;
				return IMR_NEXT;
			}
			if(y_mb_has_wildcard(mb,EIM.CodeInput) && EIM.CodeLen<=mb->len)
			{
				PhraseListCount=0;
				TableGetCandWords(PAGE_FIRST);
				ret=IMR_COMMIT_DISPLAY;
				DoTipWhenCommit();
				break;
			}
			if(auto_english && EIM.CodeLen==1)
			{
				EIM.CodeInput[--EIM.CodeLen]=0;
				return IMR_NEXT;
			}
			if(auto_english && (auto_english==2 || EIM.CodeLen<=mb->len))
			{
				if(EIM.Beep) EIM.Beep(YONG_BEEP_EMPTY);
				ret=IMR_CHINGLISH;
				break;
			}
			if(mb->english && key!=YK_SPACE)
			{
				ret=IMR_CHINGLISH;
				break;
			}
			if(auto_clear>1)
			{
				 if(EIM.CodeLen<auto_clear)
				 {
					if(EIM.CodeLen==1)
						return IMR_CLEAN_PASS;
					if(EIM.Beep) EIM.Beep(YONG_BEEP_EMPTY); 
					PhraseListCount=0;
					TableGetCandWords(PAGE_FIRST);
					return IMR_DISPLAY;
				}
				else if(EIM.CodeLen==auto_clear && auto_clear==mb->len)
				{
					if(EIM.Beep) EIM.Beep(YONG_BEEP_EMPTY);
					if(mb->commit_mode!=1)
					{
						return IMR_CLEAN;
					}
					else
					{
						PhraseListCount=0;
						TableGetCandWords(PAGE_FIRST);
						return IMR_DISPLAY;
					}
				}
				else if (EIM.CodeLen==auto_clear && auto_clear==mb->len+1)
				{
					TableReset();
					EIM.CodeInput[EIM.CodeLen++]=key;
					EIM.CodeInput[EIM.CodeLen]=0;
					goto LIST;
				}
			}
			if(full>=3 && mb->commit_which<EIM.CodeLen)
			{
				int which=SuperCodeLen;
				int len=EIM.CodeLen-which;
				char temp_code[len+1];
				int temp_count;
				strcpy(temp_code,EIM.CodeInput+which);
				temp_count=y_mb_set(mb,temp_code,len,hz_filter_temp);
				if(temp_count)
				{
					strcpy(EIM.CodeInput,temp_code);
					EIM.CodeLen=len;
					count=temp_count;
					strcpy(EIM.StringGet,SuperPhrase);
					SuperPhrase[0]=0;
					cset_clear(&cs,CSET_TYPE_CALC);
					ret=IMR_COMMIT_DISPLAY;
					DoTipWhenCommit();
					if(assoc_adjust)
					{
						struct y_mb_context ctx;
						char *string_get=l_strdupa(EIM.StringGet);
						y_mb_push_context(mb,&ctx);
						TableReset();
						y_mb_pop_context(mb,&ctx);
						strcpy(EIM.CodeInput,temp_code);
						EIM.CodeLen=len;
						strcpy(EIM.StringGet,string_get);
						TableUpdateAssoc(NULL);	
					}
					break;
				}
			}
			EIM.CodeInput[--EIM.CodeLen]=0;
			if(EIM.CodeLen==0)
			{
				ret=IMR_NEXT;
			}
			else if((mb->commit_len && EIM.CodeLen<mb->commit_len) || 
					(auto_clear && !mb->commit_len && EIM.CodeLen<mb->len))
			{
				if(EIM.Beep) EIM.Beep(YONG_BEEP_EMPTY);
				if(auto_clear)
				{
					EIM.CodeInput[0]=0;
					EIM.CodeLen=0;
					ret=IMR_CLEAN;
				}
				else
				{
					ret=IMR_BLOCK;
				}
			}
			else if(full<=1 && mb->commit_which && mb->commit_which<EIM.CodeLen)
			{
				char temp[MAX_CODE_LEN+1];
				int temp_count;
				int temp_len;
				struct y_mb_context ctx;
				int commit_prev;
commit_simple:
				commit_prev=0;
				strcpy(temp,EIM.CodeInput+mb->commit_which);
				temp_len=EIM.CodeLen-mb->commit_which;
				if(key!=YK_TAB)
				{
					temp[temp_len++]=key;
					temp[temp_len]=0;
				}
				temp_count=y_mb_set(mb,temp,temp_len,hz_filter_temp);
				if(!temp_count)
				{
					if(key!=YK_TAB)
					{
						temp[0]=key;
						temp[1]=0;
						temp_count=y_mb_set(mb,temp,1,hz_filter_temp);
						if(temp_count>0)
						{
							temp_len=1;
							commit_prev=1;
						}
					}
					if(!temp_count)
						return IMR_BLOCK;
				}
				cset_clear(&cs,CSET_TYPE_CALC);
				ret=IMR_COMMIT_DISPLAY;
				if(commit_prev)
				{
					strcpy(EIM.StringGet,EIM.CandTable[EIM.SelectIndex]);
				}
				else if(mb->commit_which<=2)
				{
					y_mb_get_simple(mb,EIM.CodeInput,EIM.StringGet,mb->commit_which);
				}
				else
				{
					strcpy(EIM.StringGet,AutoPhrase);
				}
	
				y_mb_push_context(mb,&ctx);
				TableReset();
				y_mb_pop_context(mb,&ctx);
				strcpy(EIM.CodeInput,temp);
				EIM.CodeLen=temp_len;
				count=temp_count;
				
				DoTipWhenCommit();
			}
			else
			{
				char temp[2];
				temp[0]=(char)key;
				temp[1]=0;
				count=y_mb_set(mb,temp,1,hz_filter_temp);
				if(!count)
				{
					strcpy(EIM.StringGet,EIM.CandTable[EIM.SelectIndex]);
					ret=IMR_PUNC;
				}
				else
				{
					struct y_mb_context ctx;
					cset_clear(&cs,CSET_TYPE_CALC);
					if(EIM.CandWordCount>0)
					{
						ret=IMR_COMMIT_DISPLAY;
						hz_filter_temp=hz_filter;
						strcpy(EIM.StringGet,EIM.CandTable[EIM.SelectIndex]);
						if(EIM.SelectIndex!=0 && auto_move && EIM.CodeLen>=auto_move_len[0] && EIM.CodeLen<=auto_move_len[1])
						{
							y_mb_auto_move(mb,EIM.CodeInput,EIM.StringGet,auto_move);
						}
						DoTipWhenCommit();
					}
					else
					{
						ret=IMR_DISPLAY;
					}
					int apply_assoc=(ret==IMR_COMMIT_DISPLAY && assoc_adjust);
					char *string_get=apply_assoc?l_strdupa(EIM.StringGet):NULL;
					y_mb_push_context(mb,&ctx);
					TableReset();
					y_mb_pop_context(mb,&ctx);
					EIM.CodeInput[EIM.CodeLen++]=key;
					EIM.CodeInput[EIM.CodeLen]=0;
					if(apply_assoc)
					{
						strcpy(EIM.StringGet,string_get);
						TableUpdateAssoc(NULL);	
					}
				}
			}
			break;
		}
		if(count>0)
		{
DISPLAY:
			if(count<0) count=0;
			int dext=hz_filter_strict && mb->ctx.result_filter;
			if(hz_filter && hz_filter_temp==0 && hz_filter_show) dext=2;
			cset_clear(&cs,CSET_TYPE_CALC);
			cset_mb_group_set(&cs,active_mb,count);
			PhraseListCount=cset_count(&cs);
			TableGetCandWords(PAGE_FIRST);
			if(mb->commit_which==EIM.CodeLen)
			{
				strcpy(AutoPhrase,EIM.CandTable[EIM.SelectIndex]);
			}
			int full=y_mb_is_full(mb,EIM.CodeLen);
			if(full)
			{
				if(full==1 || EIM.CodeLen==mb->commit_which)
				{
					strcpy(SuperPhrase,EIM.CandTable[EIM.SelectIndex]);
					SuperCodeLen=EIM.CodeLen;
				}
				// is not exact match, set full to 0
				if(CodeTips[EIM.SelectIndex][0])
					full=0;
			}
			int stop=0;
			if(count==1 && !CodeTips[0][0] && !y_mb_has_next(mb,dext))
			{
				if(y_mb_is_stop(mb,key,EIM.CodeLen))
				{
					stop=1;
				}
				if(!stop && y_mb_is_pull(mb,EIM.CodeInput[0]))
				{
					stop=1;
				}
				char *p=EIM.CandTable[0];
				if(l_str_has_suffix(p,"$SPACE"))
				{
					size_t len=strlen(p);
					if(len!=6 && !strstr(p,"$$"))
					{
						stop=1;
						p[len-6]=0;
					}
				}
				if(p[0]=='$' && !strcmp(p,"$ENGLISH"))
				{
					return IMR_ENGLISH;
				}
			}
			/*
			 * not wildcard and only one cand and not assist mode and no left code to input
			 * 1 stop!=0 there is pull or push char
			 * 2 super mode only at CodeLen==mb->len
			 * 3 in not super mode and CodeLen>=mb->len
			 */
			if((stop || (full==1 && active_mb->commit_mode==0) ||
					(full>=1 && active_mb->commit_mode==2)) &&
					PhraseListCount==1 && !mb->ctx.result_wildcard && !y_mb_before_assist(mb) && !y_mb_has_next(mb,dext))
			{
				const char *p=EIM.CandTable[EIM.SelectIndex];
				if(p[0]=='$' && p[1] && EIM.StringGet[0])
				{
					EIM.SendString(EIM.StringGet,0);
					strcpy(EIM.StringGet,p);
				}
				else
				{
					strcat(EIM.StringGet,p);
				}
				ret=IMR_COMMIT;
				DoTipWhenCommit();
			}
			if(ret==IMR_DISPLAY && PhraseListCount>1 && EIM.CodeLen>=mb->len && active_mb==mb)
			{
				if(EIM.Beep) EIM.Beep(YONG_BEEP_MULTI);
			}
		}
	}
	else if((active_mb=ShouldEnterAssistMode(key))!=NULL)
	{
		if(EIM.CodeLen<active_mb->len)
		{
			EIM.CodeInput[EIM.CodeLen++]=key;
			EIM.CodeInput[EIM.CodeLen]=0;
		}
		return TableAssistDoSearch(active_mb);
	}
	else if(bing && mb && key=='+' && EIM.CodeLen==2)
	{
		strcat(EIM.StringGet,EIM.CandTable[EIM.SelectIndex]);
		if(mb->yong && l_gb_strlen(EIM.StringGet,-1)==1)
		{
			int count;
			EIM.CodeInput[2]='\'';
			EIM.CodeInput[3]=0;
			EIM.CodeLen=3;
			count=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,0);
			if(count>0)
			{
				cset_clear(&cs,CSET_TYPE_CALC);
				cset_mb_group_set(&cs,mb,count);
				PhraseListCount=cset_count(&cs);
				//max=EIM.CandWordMax;
				//EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
				TableGetCandWords(PAGE_FIRST);
				strcpy(EIM.StringGet,EIM.CandTable[0]);
			}
		}
		ret=IMR_COMMIT;
	}
	else if((key==SHIFT_ENTER || key=='\r') && EIM.CandWordCount && mb->english)
	{
		int c;
		strcpy(EIM.StringGet,EIM.CandTable[EIM.SelectIndex]);
		c=EIM.StringGet[0];
		if(key!='\r' && c>='a' && c<='z')
			EIM.StringGet[0]=c-'a'+'A';
		ret=IMR_COMMIT;
	}
	else if(key==key_zi_switch && (EIM.CodeLen>0 || (key&KEYM_MASK)))
	{
		//int max;
		int count=mb->ctx.result_count_zi;
		if(zi_mode & 0x02)
		{
			zi_mode&=0x01;
			zi_mode=!zi_mode;
			if(tip_main)
			{
				if(zi_mode)
					EIM.ShowTip("进入单字模式");
				else
					EIM.ShowTip("离开单字模式");
			}
			zi_mode|=0x02;
			return IMR_CLEAN;
		}
		if(!count)
		{
			return IMR_BLOCK;
		}
		y_mb_set_zi(mb,1);
		cset_mb_group_set(&cs,mb,count);
		PhraseListCount=cset_count(&cs);
		//max=EIM.CandWordMax;
		//EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
		TableGetCandWords(PAGE_FIRST);
		return IMR_DISPLAY;
	}
	else if(EIM.CandWordCount==0 && EIM.CodeLen>0 && auto_clear>1 && key==' ')
	{
		ret=IMR_CLEAN;
	}
	else if(key==key_esc && EIM.CodeLen>0)
	{
		ret=IMR_CLEAN;
	}
	else
	{
		ret=IMR_NEXT;
	}
	return ret;
}

static int TableCall(int index,...)
{
	int res=-1;
	va_list ap;
	if(!mb || mb->english)
		return -1;
	va_start(ap,index);
	switch(index){
	case EIM_CALL_ADD_PHRASE:
	{
		const char *p=va_arg(ap,const char *);
		char code[64];
		char *codes[]={code,NULL};
		if(p==NULL)
		{
			res=0;
			break;
		}
		if(y_mb_error_has(mb,p))
		{
			break;
		}
		if(!mb->rule)
		{
			res=-2;
			break;
		}
		if(0==y_mb_code_by_rule(mb,p,strlen(p),codes,NULL))
		{
			y_mb_add_phrase(mb,code,p,Y_MB_APPEND,Y_MB_DIC_TEMP);
			res=0;
		}
		break;
	}
	case EIM_CALL_ADD_SENTENCE:
	{
		const char *p=va_arg(ap,const char *);
		char code[64];
		char *codes[]={code,NULL};
		char hint[20][2]={0};
		if(p==NULL)
		{
			res=0;
			break;
		}
		if(y_mb_ci_exist(mb,p,-1))
			break;
		if(y_mb_error_has(mb,p))
		{
			break;
		}
		if(!mb->rule)
		{
			res=-2;
			break;
		}
		if(zi_output_codes && zi_output>0)
		{
			int len=l_gb_strlen(p,-1);
			if(len>20)
				break;
			if(len>=zi_output)
			{
				memcpy(hint[len-zi_output],l_array_nth(zi_output_codes,0),zi_output*2);
			}
		}
		if(0==y_mb_code_by_rule(mb,p,strlen(p),codes,hint))
		{
			sentence_add(code,p);
			res=0;
		}
		break;
	}
	case EIM_CALL_IDLE:
	{
		if(!mb)
			break;
		y_mb_save_user(mb);
		sentence_save();
		break;
	}
	case EIM_CALL_GET_CANDWORDS:
	{
		if(!mb || PhraseListCount<=0)
		{
			res=-2;
			break;
		}
		int at=va_arg(ap,int);
		int num=va_arg(ap,int);
		if(at<0 || num<=0)
		{
			res=-3;
			break;
		}
		if(at>=PhraseListCount)
		{
			res=-4;
			break;
		}
		if(at+num>PhraseListCount)
			num=PhraseListCount-at;
		char (*cand)[MAX_CAND_LEN+1]=va_arg(ap,void*);
		char (*tip)[MAX_TIPS_LEN+1]=va_arg(ap,void*);
		if(at==-1)
			at=EIM.CurCandPage*EIM.CandWordMaxReal;
		res=cset_output(&cs,at,num,cand,tip);
		if(mb->yong && (EIM.CodeLen==2 || EIM.CodeLen==3))
		{
			for(int i=0;i<res;i++)
			{
				if(i==0 && at==0)
					continue;
				if(tip[i][0]) continue;
				y_mb_calc_yong_tip(mb,EIM.CodeInput,cand[i],tip[i]);
			}
		}
		struct y_mb *active=Y_MB_ACTIVE(mb);
		if(active==mb->ass_mb)
		{
			/* hint the main mb's code */
			for(int i=0;i<res;i++)
			{
				int len;
				char *data=cand[i];
				tip[i][0]=0;
				len=strlen(data);
				if((len==2 && gb_is_gbk(data)) || (len==4 && gb_is_gb18030_ext(data)))
				{
					y_mb_get_full_code(mb,data,tip[i]);
				}
				else
				{
					y_mb_get_exist_code(mb,data,tip[i]);
				}
			}
		}
		break;
	}
	default:
		res=-99;
		break;
	}
	va_end(ap);
	return res;
}

/**********************************************************************
 * 拼音接口
 *********************************************************************/
static char CodeGet[MAX_CODE_LEN+1];		// 已经被选择的拼音
static int CodeGetLen;						// 被选择拼音的长度
static int CodeGetCount;					// 选择了多少此拼音
static int CodeMatch;						// 匹配的拼音长度
static int AssistMode;						// 是否处于输入间接辅助码状态
static char AssistCode[4];					// 当前使用的间接辅助码
static uint8_t PinyinStep[MAX_CODE_LEN];	// 拼音切分的长度
uint8_t PySwitch;							// 拼音是否经过手工切分

typedef struct {
	CSET_GROUP;
	int mark;								// 间接辅助码结果中Extra的开始位置
	char pinyin[16];						// 额外候选的拼音
	char codetip[16];						// 字的编码提示
	struct y_mb_item *zi;					// 额外的候选
	LArray *arr;							// 模糊音候选
}EXTRA_ZI;
static EXTRA_ZI ExtraZi;
static int PredictCalcMark;					// 间接辅助码结果中Predict的结束位置

static void ExtraZiReset(void)
{
	ExtraZi.count=0;
	ExtraZi.mark=0;
	ExtraZi.zi=NULL;
	ExtraZi.pinyin[0]=0;
	ExtraZi.codetip[0]=0;
	if(ExtraZi.arr)
	{
		l_ptr_array_free(ExtraZi.arr,NULL);
		ExtraZi.arr=NULL;
	}
}

static int ExtraZiGet(int cur,int count,char CandTable[][MAX_CAND_LEN+1],int key)
{
	struct y_mb_ci *c;
	int i=0;
	if(ExtraZi.zi)
	{
		for(i=0,c=ExtraZi.zi->phrase;c && i<cur;c=c->next)
		{
			if(c->del || !c->zi) continue;
			if(hz_filter_temp && c->ext) continue;
			if(y_mb_in_result(mb,c)) continue;
			i++;
		}
		for(i=0;c && i<count;c=c->next)
		{
			if(c->del || !c->zi) continue;
			if(hz_filter_temp && c->ext) continue;
			if(y_mb_in_result(mb,c)) continue;
			if(key && !y_mb_assist_test(mb,c,key,0,0)) continue;
			y_mb_ci_string2(c,CandTable[i]);
			i++;
		}
	}
	if(ExtraZi.arr)
	{
		int j;
		for(j=0;j<ExtraZi.arr->len && i<count;j++)
		{
			c=l_ptr_array_nth(ExtraZi.arr,cur+j);
			if(c->del || !c->zi) continue;
			if(hz_filter_temp && c->ext) continue;
			if(y_mb_in_result(mb,c)) continue;
			if(key && !y_mb_assist_test(mb,c,key,0,0)) continue;
			y_mb_ci_string2(c,CandTable[i]);
			i++;
		}
	}
	return i;
}

static int ExtraZiOutput(void *unsed,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	int i;
	for(i=0;i<num;i++)
	{
		strcpy(tip[i],ExtraZi.codetip);
	}
	return ExtraZiGet(at,num,cand,0);
}

static void ExtraZiFill(const char *code,int len,int res)
{
	ExtraZi.type=CSET_TYPE_EXTRA_ZI;
	ExtraZi.get=(void*)ExtraZiOutput;
	if(mb->fuzzy)
	{
		struct y_mb_context ctx;
		int ret;
		y_mb_push_context(mb,&ctx);
		mb->ctx.result_match=1;
		ret=y_mb_set(mb,code,len,hz_filter_temp);
		if(ret>0)
		{
			if(mb->ctx.result_ci)
			{
				ExtraZi.arr=mb->ctx.result_ci;
				mb->ctx.result_ci=NULL;
			}
			else
			{
				ExtraZi.zi=mb->ctx.result_first;
			}
		}
		y_mb_pop_context(mb,&ctx);
	}
	else
	{
		ExtraZi.zi=y_mb_get_zi(mb,code,len,hz_filter_temp);
	}
	if(ExtraZi.zi)
	{
		struct y_mb_ci *p;
		for(ExtraZi.count=0,p=ExtraZi.zi->phrase;p;p=p->next)
		{
			if(p->del) continue;
			if(hz_filter_temp && p->ext) continue;
			if(!p->zi) continue;
			if(res && y_mb_in_result(mb,p)) continue;
			ExtraZi.count++;
		}
	}
	else if(ExtraZi.arr)
	{
		struct y_mb_ci *p;
		int i;
		for(ExtraZi.count=0,i=0;i<ExtraZi.arr->len;i++)
		{
			p=l_ptr_array_nth(ExtraZi.arr,i);
			if(p->del) continue;
			if(hz_filter_temp && p->ext) continue;
			if(!p->zi) continue;
			if(res && y_mb_in_result(mb,p)) continue;
			ExtraZi.count++;
		}
	}
	if(ExtraZi.count<=0)
		ExtraZiReset();

	if(ExtraZi.count>0)
	{
		cset_remove(&cs,(CSET_GROUP*)&ExtraZi);
		cset_append(&cs,(CSET_GROUP*)&ExtraZi);
	}
}

static void PinyinSetAssistCode(const char *s);
static void PinyinResetPart(void)
{
	CodeGetLen=0;
	CodeGet[0]=0;
	CodeMatch=0;
	CodeGetCount=0;
	AssistMode=0;
	PinyinSetAssistCode(NULL);

	l_predict_simple_mode=-1;

	ExtraZiReset();
}
static void PinyinReset(void)
{
	TableReset();
	PinyinResetPart();
}

static void PinyinStripInput(void)
{
	if(py_space!=' ')
	{
		char *to=EIM.CodeInput,*from=EIM.CodeInput;
		int *caret=&EIM.CaretPos;
		int count=0;
		for(int i=0;*from!=0;i++)
		{
			if(*from==py_space)
			{
				from++;
				if(*caret>i)
					*caret=*caret-1;
				continue;
			}
			*to++=*from++;
			count++;
		}
		*to='\0';
		EIM.CodeLen=count;
		return;
	}
	EIM.CodeLen=py_prepare_string(EIM.CodeInput,EIM.CodeInput,&EIM.CaretPos);
}

static void PinyinAddSpace(void)
{
	if(CodeMatch<=0)
		return;
	if(CodeMatch==EIM.CaretPos)
		return;
	if(CodeMatch==EIM.CodeLen)
		return;
	memmove(EIM.CodeInput+CodeMatch+1,EIM.CodeInput+CodeMatch,
		EIM.CodeLen-CodeMatch+1);
	EIM.CodeLen++;
	if(EIM.CaretPos>CodeMatch)
		EIM.CaretPos++;	
	EIM.CodeInput[CodeMatch]=py_space;
}

static int AdjustPrevStep(int start,int cur,int test)
{
	int i;
	int len;
	int step=!test;
	if(cur==0)
		return 0;
	for(i=start,len=0;PinyinStep[i];i++)
	{
		len+=PinyinStep[i];
		if(test && len==cur)
		{
			return cur;
		}
		if(len>=cur)
		{
			/* step len to previous */
			step=PinyinStep[i]-(len-cur);
			/* if the first step, one step only */
			if(!test && step==cur)
				step=1;
			/* if only one step and test use it */
			else if(test && i==start)
				step=0;
			break;
		}
	}
	return cur-step;
}

// 在不包含空格的情况下进行切分
static int PinyinSegmentRecursive(const char *code,int len,int filter,uint8_t steps[],int *pos)
{
	int good=len,less;
	uint8_t temp[len];
	int tpos;
	int best=0;
	int count=0;

	while(good>0)
	{
		int match=y_mb_max_match(mb,code,good,-1,filter,&good,&less);
		if(code[match]==0)
		{
			// 我们在句尾
			temp[0]=match;
			best=match;
			count=1;
			break;
		}
		if(good==0)
			break;
		temp[0]=good;
		int ret=PinyinSegmentRecursive(code+good,len-good,filter,steps,&tpos);
		if(tpos+good>best)
		{
			memcpy(temp+1,steps,ret);
			best=tpos+good;
			count=1+ret;
		}
		if(tpos+good==len)
			break;
		good=less;
	}
	if(pos)
		*pos=best;
	if(count>0)
	{
		memcpy(steps,temp,count);
	}
	return count;
}

static int PinyinPredict(CSET_GROUP_PREDICT *g,const char *code,int len,int filter)
{
	uint8_t steps[len];
	int count;
	int space_index=array_index(code,len,(char)py_space);
	int rlen=0;
	char *res=g->phrase;
	char *codetip=g->codetip;
	if(space_index>0)
	{
		steps[0]=space_index;
		count=1;
		count+=PinyinSegmentRecursive(code+space_index+1,len-space_index-1,filter,steps+1,NULL);
	}
	else
	{
		count=PinyinSegmentRecursive(code,len,filter,steps,NULL);;
	}
	int saved_match=mb->ctx.result_match;
	mb->ctx.result_match=0;
	for(int i=0;i<count;i++)
	{
		if(code[0]==py_space)
			code++;
		y_mb_set(mb,code,steps[i],filter);
		struct y_mb_ci *c=y_mb_get_first(mb,NULL,codetip);
		if(c->data[0]=='$' && c->data[1]=='[')
		{
			const char *s=y_mb_skip_display((const char*)c->data,c->len);
			int len=(int)(size_t)((const char*)c->data+c->len-s);
			memcpy(res+rlen,s,len);
			rlen+=len;
		}
		else
		{
			memcpy(res+rlen,c->data,c->len);
			rlen+=c->len;
		}
		code+=steps[i];
	}
	res[rlen]='\0';
	mb->ctx.result_match=saved_match;
	return rlen;
}

static int PinyinMoveCaretTo(int key)
{
	int i,p;
	if(EIM.CodeLen<1)
		return 0;
	if(key>='A' && key<='Z')
		key='a'+(key-'A');

	if(mb && mb->pinyin && mb->split=='\'' && !SP)
	{
		if(key=='i' || key=='u')
			return 0;
		py_item_t input[PY_MAX_TOKEN];
		int count=py_parse_string(EIM.CodeInput,input,-1,NULL,NULL);
		const char *space=strchr(EIM.CodeInput,py_space);
		int p=EIM.CaretPos;
		char dis[256];
		py_build_string(dis,input,count);
		for(i=0;i<count;i++)
		{
			p=py_caret_next_item(input,count,p);
			if(p<0)
				return 0;
			if(space!=NULL && EIM.CodeInput+p>=space)
				p++;
			if(EIM.CodeInput[p]==key)
			{
				EIM.CaretPos=p;
				return 0;
			}
			if(space!=NULL && EIM.CodeInput+p>=space)
				p--;
		}
		return 0;
	}
	for(i=0,p=EIM.CaretPos+1;i<EIM.CodeLen;i++)
	{
		if(p>CodeMatch && EIM.CaretPos>CodeMatch && !(p&0x01))
			p++;
		else if(p<=CodeMatch && (p&0x01))
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

static int PinyinAssistDoSearch(struct y_mb *active)
{
	int max;
	CodeMatch=EIM.CodeLen;
	PhraseListCount=y_mb_set(active,EIM.CodeInput+1,EIM.CodeLen-1,0);
	cset_mb_group_set(&cs,active,PhraseListCount);
	PhraseListCount=cset_count(&cs);
	max=EIM.CandWordMax;
	EIM.CandWordMaxReal=max;
	EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	cset_clear(&cs,CSET_TYPE_PREDICT);
	cset_clear(&cs,CSET_TYPE_CALC);
	PinyinGetCandwords(PAGE_FIRST);
	return PhraseListCount;
}

static int SPDoSearch(int adjust)
{
	int max;
	char temp[128];
	char code[128];
	int GoodMatch=0;
	int CodeReal;
	int HaveResult=0;

	PinyinStripInput();
	memset(PinyinStep,2,sizeof(PinyinStep));

	cset_clear(&cs,CSET_TYPE_PREDICT);
	ExtraZiReset();
	y_mb_set_zi(mb,0);

	if(mb->fuzzy && CodeGetLen==0 && EIM.CaretPos==EIM.CodeLen)
		EIM.CodeLen=EIM.CaretPos=fuzzy_correct(mb->fuzzy,EIM.CodeInput,EIM.CodeLen);

	if(l_predict_simple_mode==1)
	{
		l_predict_simple_mode=2;
		struct y_mb_context ctx;
		y_mb_push_context(mb,&ctx);
		CSET_GROUP_PREDICT *g=cset_predict_group_new(&cs);
		g->count=y_mb_predict_by_learn(mb,
				EIM.CodeInput,EIM.CodeLen,
				g,CodeGetLen==0);
		y_mb_pop_context(mb,&ctx);
		if(g->count)
		{
			cset_prepend(&cs,(CSET_GROUP*)g);
			PhraseListCount=g->count;
			max=EIM.CandWordMax;
			EIM.CandWordMaxReal=max;
			EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
			TableGetCandWords(PAGE_FIRST);
			return PhraseListCount;
		}
	}

	// 双拼情况下打全拼码表的自定义编码
	if(!AssistMode && CodeGetLen==0 && EIM.CodeLen>=2 && EIM.CodeLen<=4 && 
		EIM.CaretPos==EIM.CodeLen &&
		!py_is_valid_quanpin(EIM.CodeInput) &&				// 不能是合法的全拼
		!(EIM.CodeLen==2 && py_is_valid_sp(EIM.CodeInput)))	// 不能是合法的双拼
	{
		mb->ctx.result_match=1;
		mb->pinyin=0;
		void *old_fuzzy=mb->fuzzy;
		mb->fuzzy=NULL;
		int count=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,hz_filter_temp);
		if(count>0)
		{
#if 0
			CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
			if(count>Y_MB_DATA_CALC) count=Y_MB_DATA_CALC;
			y_mb_get(mb,0,count,g->phrase,NULL);
			g->count=count;
			PhraseListCount=g->count;
			cset_prepend(&cs,(CSET_GROUP*)g);
#else
			cset_clear(&cs,CSET_TYPE_ALL);
			cset_mb_group_set(&cs,mb,count);
			PhraseListCount=cset_count(&cs);
#endif
			CodeMatch=EIM.CodeLen;
			EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+
					((PhraseListCount%EIM.CandWordMax)?1:0);
			TableGetCandWords(PAGE_FIRST);
			
			mb->pinyin=1;
			mb->fuzzy=old_fuzzy;
		
			return PhraseListCount;
		}
		
		mb->pinyin=1;
		mb->fuzzy=old_fuzzy;
		mb->ctx.result_match=0;
	}

	// 处理单字和词组辅助码，即使CodeLen!=0时也要处理，生成句子时无法很好处理单字辅助码的情况
	if(!AssistMode && /*CodeGetLen==0 && */(EIM.CodeLen&1) &&
			EIM.CaretPos==EIM.CodeLen && mb->ass_mb &&
			(EIM.CodeLen==3 || (EIM.CodeLen>=5  && CodeMatch==EIM.CodeLen-1)))
	{
		int clen;

		if(EIM.CodeLen==3)
			y_mb_set_zi(mb,1);
		
		strcpy(temp,EIM.CodeInput);temp[EIM.CodeLen-1]=0;
		clen=py_conv_from_sp(temp,code,sizeof(code),0);
		if(clen>0)
		{
			int count=y_mb_set(mb,code,clen,hz_filter_temp);
			if(count>0)
			{
				int end=(EIM.CodeLen==3?0:1);
				CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
				g->count=y_mb_assist_get(mb,g->phrase,Y_MB_DATA_CALC,EIM.CodeInput[EIM.CodeLen-1],end);
				if(g->count)
				{
					cset_prepend(&cs,(CSET_GROUP*)g);
					CodeMatch=EIM.CodeLen;
					PhraseListCount=g->count;
					EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+
							((PhraseListCount%EIM.CandWordMax)?1:0);
					TableGetCandWords(PAGE_FIRST);
					return PhraseListCount;
				}
			}
		}
	}
	// 双拼双辅
	if(AssistMode && CodeGetLen==0 && EIM.CodeLen==4 && EIM.CaretPos==4 && mb->ass_mb)
	{
		int clen;
		strcpy(temp,EIM.CodeInput);temp[2]=0;
		clen=py_conv_from_sp(temp,code,sizeof(code),0);
		if(clen>0)
		{
			int count=y_mb_set(mb,code,clen,hz_filter_temp);
			if(count>0)
			{
				CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
				g->count=y_mb_assist_get2(mb,g->phrase,Y_MB_DATA_CALC,EIM.CodeInput+2,0);
				if(g->count)
				{
					cset_prepend(&cs,(CSET_GROUP*)g);
					CodeMatch=4;
					PhraseListCount=g->count;
					EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+
							((PhraseListCount%EIM.CandWordMax)?1:0);
					TableGetCandWords(PAGE_FIRST);
					AssistMode=2;
					return PhraseListCount;
				}
			}
		}
	}
	// 码表中带$的候选
	if(CodeGetLen==0 && EIM.CodeLen>=2 && EIM.CodeLen<=4)
	{
		struct y_mb_ci *c;
		if(y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,0)>0 && mb->ctx.result_first)
		{
			c=mb->ctx.result_first->phrase;
			CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
			for(g->count=0;c!=NULL && g->count<=Y_MB_DATA_CALC;c=c->next)
			{
				char *s=g->phrase[g->count];
				y_mb_ci_string2(c,s);
				if(strchr(s,'$'))
				{
					g->count++;
				}
			}
			if(g->count>0)
			{
				cset_prepend(&cs,(CSET_GROUP*)g);
				PhraseListCount=g->count;
				EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+
						((PhraseListCount%EIM.CandWordMax)?1:0);
				TableGetCandWords(PAGE_FIRST);
				CodeMatch=EIM.CodeLen;
				return PhraseListCount;
			}
		}
	}
	
	CodeReal=EIM.CaretPos?EIM.CaretPos:EIM.CodeLen;
	if(CodeReal<=0)
	{
		CodeMatch=0;
		PhraseListCount=0;
		EIM.CandPageCount=0;
	}
	else
	{
		int good,len=CodeReal;
		if(adjust && CodeMatch>=1)
		{
			len=AdjustPrevStep(0,CodeMatch,0);
		}
		do{
			int clen;
			strcpy(temp,EIM.CodeInput);temp[len]=0;
			clen=py_conv_from_sp(temp,code,sizeof(code),'\'');
			if(clen>=2 && code[clen-1]=='\'')
			{
				code[--clen]=0;
			}
			CodeMatch=y_mb_max_match(mb,code,clen,-1,hz_filter_temp,&good,NULL);
			if(len==0) CodeMatch=0;
			GoodMatch=py_pos_of_sp(temp,good);
			// printf("%d %d %s\n",CodeMatch,clen,code);
			if(CodeMatch==clen)
			{
				CodeMatch=len;
				break;
			}
			CodeMatch=py_pos_of_sp(temp,CodeMatch);
			if(CodeMatch>=len)
				CodeMatch=len-1;
			if(CodeMatch<=0)
			{
				CodeMatch=0;
				code[0]=0;
				break;
			}
			len=AdjustPrevStep(0,CodeMatch,1);
		}while(CodeMatch>1);
		mb->ctx.result_match=(CodeMatch<EIM.CodeLen);
		len=strlen(code);
		if(CodeMatch==2 && py_sp_unlikely_jp(temp))
		{
			y_mb_set_zi(mb,1);
			PhraseListCount=y_mb_set(mb,code,len,hz_filter_temp);
		}
		else
		{
			PhraseListCount=y_mb_set(mb,code,len,hz_filter_temp);
		}
		cset_mb_group_set(&cs,mb,PhraseListCount);
		PhraseListCount=cset_count(&cs);
		HaveResult=PhraseListCount>0;
		max=EIM.CandWordMax;
		EIM.CandWordMaxReal=max;
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	}
	PinyinAddSpace();
	PinyinGetCandwords(PAGE_FIRST);
	CodeReal=EIM.CaretPos?EIM.CaretPos:EIM.CodeLen;
	if(CodeReal && GoodMatch<CodeReal)
	{
		struct y_mb_context ctx;
		y_mb_push_context(mb,&ctx);
		CSET_GROUP_PREDICT *g=cset_predict_group_new(&cs);
		g->count=y_mb_predict_by_learn(mb,
				EIM.CodeInput,CodeReal,
				g,CodeGetLen==0);
		y_mb_pop_context(mb,&ctx);
		if(g->count)
			cset_prepend(&cs,(CSET_GROUP*)g);
		PhraseListCount+=g->count;
		max=EIM.CandWordMax;
		EIM.CandWordMaxReal=max;
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	}
	if(CodeMatch>2)
	{
		memcpy(temp,EIM.CodeInput,2);temp[2]=0;
		py_conv_from_sp(temp,code,sizeof(code),0);
		ExtraZiFill(code,strlen(code),HaveResult);
		if(ExtraZi.count>0)
		{
			strcpy(ExtraZi.pinyin,temp);
			PhraseListCount+=ExtraZi.count;
			max=EIM.CandWordMax;
			EIM.CandWordMaxReal=max;
			EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
		}
	}
	PinyinGetCandwords(PAGE_FIRST);
	return PhraseListCount;
}

static int PinyinDoSearch(int adjust)
{
	struct y_mb *active=Y_MB_ACTIVE(mb);
	int max;
	int GoodMatch=0;
	int CodeReal;
	int HaveResult=0;

	cset_clear(&cs,CSET_TYPE_CALC);
	
	(void)GoodMatch;

	if(active!=mb)
	{
		return PinyinAssistDoSearch(active);
	}
	
	PySwitch=adjust;
	if(adjust)
		l_predict_simple_mode=0;
	if(SP==1)
		return SPDoSearch(adjust);

	if(l_predict_simple_mode==1)
	{
		l_predict_simple_mode=2;
		struct y_mb_context ctx;
		y_mb_push_context(mb,&ctx);
		CSET_GROUP_PREDICT *g=cset_predict_group_new(&cs);
		g->count=y_mb_predict_by_learn(mb,
				EIM.CodeInput,EIM.CodeLen,
				g,CodeGetLen==0);
		y_mb_pop_context(mb,&ctx);
		if(g->count)
		{
			cset_prepend(&cs,(CSET_GROUP*)g);
			PhraseListCount=g->count;
			max=EIM.CandWordMax;
			EIM.CandWordMaxReal=max;
			EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
			TableGetCandWords(PAGE_FIRST);
			return PhraseListCount;
		}
	}

	PinyinStripInput();
	py_string_step(EIM.CodeInput,EIM.CaretPos,
			PinyinStep,sizeof(PinyinStep));
	cset_clear(&cs,CSET_TYPE_PREDICT);
	ExtraZiReset();

	if(mb->fuzzy && CodeGetLen==0 && EIM.CaretPos==EIM.CodeLen)
		EIM.CodeLen=EIM.CaretPos=fuzzy_correct(mb->fuzzy,EIM.CodeInput,EIM.CodeLen);

	while(!adjust && !AssistMode && //(mb->ass || mb->yong) &&
			CodeGetLen==0 && mb->split>=2 && (EIM.CodeLen%mb->split==1) && 
			EIM.CaretPos==EIM.CodeLen &&
			(EIM.CodeLen==mb->split+1 || (EIM.CodeLen>=2*mb->split+1 && CodeMatch==EIM.CodeLen-1)))
	{
		char code[128];
		int clen=EIM.CodeLen-1;
		int count;

		if(mb->yong && EIM.CodeLen<mb->split*2+1)
			break;
			
		// 现在我们需要精确匹配的模式
		mb->ctx.result_match=1;
		
		// 如果码表中有对应的编码，那么我们就不处理直接辅助码的情况
		count=y_mb_set(mb,EIM.CodeInput,EIM.CodeLen,hz_filter_temp);
		if(count>0)
		{
			if(EIM.CodeLen==mb->split+1)
			{
				// 如果在单字加辅助码的位置码表中直接有编码，那么直接返回，不要再找得第一个码位的单字了
				CodeMatch=EIM.CodeLen;
				cset_mb_group_set(&cs,mb,count);
				PhraseListCount=cset_count(&cs);
				cset_clear(&cs,CSET_TYPE_CALC);
				EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+
						((PhraseListCount%EIM.CandWordMax)?1:0);
				TableGetCandWords(PAGE_FIRST);
				return PhraseListCount;
			}
			break;
		}
		
		strcpy(code,EIM.CodeInput);code[clen]=0;
		count=y_mb_set(mb,code,clen,hz_filter_temp);
		if(count>0)
		{
			int end=(EIM.CodeLen==mb->split+1?0:1);
			CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
			g->count=y_mb_assist_get(mb,g->phrase,Y_MB_DATA_CALC,EIM.CodeInput[EIM.CodeLen-1],end);
			if(g->count)
			{
				cset_prepend(&cs,(CSET_GROUP*)g);
				CodeMatch=EIM.CodeLen;
				PhraseListCount=g->count;
				EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+
						((PhraseListCount%EIM.CandWordMax)?1:0);
				TableGetCandWords(PAGE_FIRST);
				return PhraseListCount;
			}
		}
		break;
	}
	CodeReal=EIM.CaretPos?EIM.CaretPos:EIM.CodeLen;
	if(CodeReal<=0)
	{
		CodeMatch=0;
		PhraseListCount=0;
		EIM.CandPageCount=0;
	}
	else
	{
		int len=CodeReal;
		if(adjust && CodeMatch>=1)
		{
			len=AdjustPrevStep(0,CodeMatch,0);
		}
		do{
			if(mb->split<=1)
			{
				uint8_t steps[len];
				int count=PinyinSegmentRecursive(EIM.CodeInput,len,hz_filter_temp,steps,NULL);
				if(count>0)
				{
					GoodMatch=steps[0];
					CodeMatch=steps[0];					
					break;
				}
			}
			int good,less;
			CodeMatch=y_mb_max_match(mb,EIM.CodeInput,len,-1,hz_filter_temp,&good,&less);
			if(len==0) CodeMatch=0;
			if(CodeMatch<EIM.CaretPos && CodeMatch>good)
				CodeMatch=good;
			GoodMatch=good;
			if(CodeMatch==EIM.CodeLen)
				break;
			len=AdjustPrevStep(0,CodeMatch,1);
			if(len==CodeMatch)
				break;
			CodeMatch=less;
			len=AdjustPrevStep(0,CodeMatch,1);
			if(len==CodeMatch)
				break;
		}while(CodeMatch>1);
		mb->ctx.result_match=(CodeMatch<EIM.CodeLen);
		{
			PhraseListCount=y_mb_set(mb,EIM.CodeInput,CodeMatch,hz_filter_temp);
			cset_mb_group_set(&cs,mb,PhraseListCount);
			PhraseListCount=cset_count(&cs);
		}
		HaveResult=PhraseListCount>0;
		max=EIM.CandWordMax;
		EIM.CandWordMaxReal=max;
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	}
	PinyinAddSpace();
	PinyinGetCandwords(PAGE_FIRST);
	
	CodeReal=EIM.CaretPos?EIM.CaretPos:EIM.CodeLen;
	if(CodeReal && GoodMatch<CodeReal)
	{
		struct y_mb_context ctx;
		y_mb_push_context(mb,&ctx);
		CSET_GROUP_PREDICT *g=cset_predict_group_new(&cs);
		if(mb->split>1 /*&& auto_move!=1*/)
		{
			g->count=y_mb_predict_by_learn(mb,EIM.CodeInput,
						CodeReal,g,CodeGetLen==0);
		}
		else
		{
			PinyinPredict(g,EIM.CodeInput,CodeReal,hz_filter_temp);
			g->count=g->phrase[0]?1:0;
		}
		y_mb_pop_context(mb,&ctx);
		if(g->count>0)
			cset_prepend(&cs,(CSET_GROUP*)g);
		PhraseListCount+=g->count;
		max=EIM.CandWordMax;
		EIM.CandWordMaxReal=max;
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
	}

	if(CodeMatch>1)
	{
		int i=CodeMatch-1;
		if(mb->split=='\'' || mb->split==1)
		{
			i=MIN(6,i);
			for(;i>0;i--)
			{
				ExtraZiFill(EIM.CodeInput,i,HaveResult);
				if(ExtraZi.count>0)
					break;
			}
		}
		else if(mb->split>1 && i>=mb->split)
		{
			i=mb->split;
			ExtraZiFill(EIM.CodeInput,i,HaveResult);
		}
		else if(mb->split==CodeMatch && mb->simple && (CodeGetCount>0 || EIM.CodeLen>CodeMatch))
		{
			// 在组句选字情况下，不考虑simple参数
			i=mb->split;
			ExtraZiFill(EIM.CodeInput,i,0);
			if(ExtraZi.count>0)
			{
				PhraseListCount=cset_predict_group_count(&cs);
				HaveResult=0;
				mb->ctx.result_count=0;
				mb->ctx.result_count_zi=0;
				mb->ctx.result_first=0;
			}
		}
		
		if(ExtraZi.count>0)
		{
			memcpy(ExtraZi.pinyin,EIM.CodeInput,i);
			ExtraZi.pinyin[i]=0;
			PhraseListCount+=ExtraZi.count;
			max=EIM.CandWordMax;
			EIM.CandWordMaxReal=max;
			EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
		}
	}
	PinyinGetCandwords(PAGE_FIRST);
	return PhraseListCount;
}

static void PinyinCodeRevert(void)
{
	PinyinStripInput();
	EIM.CaretPos=strlen(CodeGet);
	strcat(CodeGet,EIM.CodeInput);
	strcpy(EIM.CodeInput,CodeGet);
	EIM.CodeLen=strlen(EIM.CodeInput);
	CodeGet[0]=0;
	CodeGetLen=0;
	EIM.StringGet[0]=0;
	CodeGetCount=0;
	AssistMode=0;
	PinyinDoSearch(0);
}

static void AddExtraSplit(void)
{
	if(mb->split!='\'' || SP)
		return;
	if(CodeGetLen<=0)
		return;
	if(EIM.CodeInput[0]=='\'' || !strncmp(EIM.CodeInput," '",2))
	{
		CodeGet[CodeGetLen++]='\'';
		CodeGet[CodeGetLen]=0;
		PinyinStripInput();
		//printf("%s %s\n",CodeGet,EIM.CodeInput);
	}
}

static char *PinyinGetCandWord(int index)
{
	int pos;
	int force=(index==-1);
	char *ret,*tip;

	if(index>=EIM.CandWordCount)
		return 0;
	if(index==-1) index=EIM.SelectIndex;

	if(index>=0)
	{
		pos=EIM.CurCandPage*EIM.CandWordMax+index;
		ret=&EIM.CandTable[index][0];
		tip=&EIM.CodeTips[index][0];
	}
	else
	{
		pos=-index-2;
		ret=l_alloca(MAX_CAND_LEN+1);
		tip=l_alloca(MAX_CODE_LEN+1);
		if(1!=cset_output(&cs,pos,1,(void*)ret,(void*)tip))
			return NULL;
	}
	if(ExtraZi.count>0 && ((cset_calc_group_count(&cs)==0 && pos>=PhraseListCount-ExtraZi.count) ||
			(cset_calc_group_count(&cs)>0 && pos>=ExtraZi.mark)))
	{
		// 输出ExtraZi的情况
		int step;
		if(strlen(EIM.StringGet)+strlen(ret)>Y_MB_DATA_SIZE)
			return NULL;
		strcat(EIM.StringGet,ret);
		step=strlen(ExtraZi.pinyin);
		strcat(CodeGet+CodeGetLen,ExtraZi.pinyin);
		CodeGetLen+=step;
		CodeGetCount++;
		memmove(EIM.CodeInput,EIM.CodeInput+step,
					EIM.CodeLen-step+1);
		EIM.CaretPos-=step;
		if(EIM.CaretPos<0) EIM.CaretPos=0;
		EIM.CodeLen-=step;
		AddExtraSplit();
	}
	else if(CodeMatch)
	{
		if(strlen(EIM.StringGet)+strlen(ret)>Y_MB_DATA_SIZE)
			return NULL;
		strcat(EIM.StringGet,ret);
		if((cset_calc_group_count(&cs)>0 && pos<PredictCalcMark) ||
				(cset_calc_group_count(&cs)==0 && pos<cset_predict_group_count(&cs)))
		{
			// 输出整句的情况
			int CodeReal;
			CodeGetCount++;
			PinyinStripInput();
			CodeReal=EIM.CaretPos?EIM.CaretPos:EIM.CodeLen;
			memcpy(CodeGet+CodeGetLen,EIM.CodeInput,CodeReal);
			CodeGetLen+=CodeReal;
			EIM.CodeLen-=CodeReal;
			CodeGet[CodeGetLen]=0;
			memmove(EIM.CodeInput,EIM.CodeInput+EIM.CaretPos,EIM.CodeLen+1);
			EIM.CaretPos=0;
			if(auto_move)
			{
				char *code=ExtraZi.pinyin;
				if(SP==1)
				{
					char temp[MAX_CODE_LEN+1];
					py_conv_from_sp(code,temp,sizeof(temp),0);
					y_mb_auto_move(mb,temp,ret,auto_move);
				}
				else
				{
					y_mb_auto_move(mb,code,ret,auto_move);
				}
			}
		}
		else
		{
			int gblen=l_gb_strlen(ret,-1);
			if((l_predict_sp || mb->split==2) && PredictCalcMark==0 &&
					CodeGetLen>0 && CodeMatch>=3 && (CodeMatch&0x01) &&
					pos<cset_calc_group_count(&cs) &&
					gblen*2==CodeMatch-1)
			{
				memcpy(CodeGet+CodeGetLen,EIM.CodeInput,CodeMatch-1);
				CodeGetLen+=CodeMatch-1;
			}
			else
			{
				memcpy(CodeGet+CodeGetLen,EIM.CodeInput,CodeMatch);
				CodeGetLen+=CodeMatch;
			}
			CodeGet[CodeGetLen]=0;
			CodeGetCount++;
			memmove(EIM.CodeInput,EIM.CodeInput+CodeMatch,
					EIM.CodeLen-CodeMatch+1);
			EIM.CaretPos-=CodeMatch;
			if(EIM.CaretPos<0) EIM.CaretPos=0;
			EIM.CodeLen-=CodeMatch;
			if(auto_move)
			{
				char *code=CodeGet+CodeGetLen-CodeMatch;
				if(SP==1)
				{
					char sp[MAX_CODE_LEN+1];	
					char temp[MAX_CODE_LEN+1];
					l_strncpy(sp,code,gblen*2);
					py_conv_from_sp(sp,temp,sizeof(temp),0);
					y_mb_auto_move(mb,temp,ret,auto_move);
				}
				else
				{
					y_mb_auto_move(mb,code,ret,auto_move);
				}
			}
		}
		AddExtraSplit();
		if(0==EIM.CodeLen)
		{
			CSET_GROUP_PREDICT *g=cset_get_group_by_type(&cs,CSET_TYPE_PREDICT);
			strcat(CodeGet,tip);
			CodeMatch=0;
			if((CodeGetCount>1 || (PySwitch && py_switch_save) || (py_assist_save && g && g->ptype==PREDICT_ASSIST)) && auto_add)
			{
				if(g && g->ptype==PREDICT_ASSIST)
				{
					CodeGet[strlen(CodeGet)-1]=0;
				}
				if(SP==1)
				{
					char temp[MAX_CODE_LEN+1];
					py_conv_from_sp(CodeGet,temp,sizeof(temp),0);
					y_mb_add_phrase(mb,temp,EIM.StringGet,Y_MB_APPEND,Y_MB_DIC_USER);
				}
				else
				{
					y_mb_add_phrase(mb,CodeGet,EIM.StringGet,Y_MB_APPEND,Y_MB_DIC_USER);
				}
			}
			CodeGet[0]=0;
		}
	}
	else
	{
		if(EIM.CandWordCount)
		{
			strcpy(EIM.StringGet,ret);
			force=1;
			
			if(PySwitch && py_switch_save&& auto_add)
			{
				if(SP==1)
				{
					char temp[MAX_CODE_LEN+1];
					py_conv_from_sp(EIM.CodeInput,temp,sizeof(temp),0);
					y_mb_add_phrase(mb,temp,EIM.StringGet,Y_MB_APPEND,Y_MB_DIC_USER);
				}
				else
				{
					y_mb_add_phrase(mb,EIM.CodeInput,EIM.StringGet,Y_MB_APPEND,Y_MB_DIC_USER);
				}
			}

			if(assoc_mode && assoc_handle && assoc_move)
			{
				y_assoc_move(assoc_handle,EIM.StringGet);
			}
		}
	}
	
	EIM.SelectIndex=0;
	
	/* in simple or assist mode, we always commit
	 * if -1 select we commit too
	 * if mb->pinyin!=3, we can commit when no code left
	 */
	if(0==EIM.CodeLen || force ||
			(mb->ass_lead && EIM.CodeInput[0]==mb->ass_lead))
	{
		return EIM.StringGet;
	}
	else
	{
		PinyinDoSearch(0);
		return 0;
	}
}

static int PinyinGetCandwords(int mode)
{
	struct y_mb *active;
	int i,start,max;

	if(mode==PAGE_ASSOC)
	{
		PinyinResetPart();
		//CodeGetLen=0;
		//CodeGet[0]=0;
	}
	
	if(mode==PAGE_ASSOC)
		active=mb;
	else
		active=Y_MB_ACTIVE(mb);

	max=EIM.CandWordMax;
	EIM.CandWordMaxReal=max;

	if(mode==PAGE_ASSOC)
	{
		const char *from=EIM.StringGet;
		if(l_gb_strlen(from,-1)<assoc_begin)
			return IMR_BLOCK;
		CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
		if(assoc_handle)
		{
			g->count=y_assoc_get(
				assoc_handle,from,strlen(from),
				assoc_count,g->phrase,Y_MB_DATA_CALC);
		}
		else
		{
			g->count=y_mb_get_assoc(
				mb,from,strlen(from),
				assoc_count,g->phrase,Y_MB_DATA_CALC);
		}
		if(!assoc_hungry)
			g->count+=EIM.QueryHistory(EIM.StringGet,g->phrase+g->count,Y_MB_DATA_CALC-g->count);
		if(!g->count)
			return IMR_BLOCK;
		cset_prepend(&cs,(CSET_GROUP*)g);
		EIM.CodeLen=0;
		EIM.CodeInput[0]=0;
		strcpy(EIM.StringGet,EIM.Translate(hz_trad?"相關字詞：":"联想："));
		ExtraZiReset();
		PhraseListCount=g->count;
		EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
		mode=PAGE_FIRST;
		assoc_mode=1;
	}
	else
	{
		if(mode==PAGE_FIRST)
		{
			EIM.CandPageCount=PhraseListCount/max+((PhraseListCount%max)?1:0);
		}
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
		CodeTips[i][0]=0;
		EIM.CodeTips[i][0]=0;
	}

	cset_output(&cs,start,EIM.CandWordCount,EIM.CandTable,CodeTips);
	if(EIM.SelectIndex>=EIM.CandWordCount)
		EIM.SelectIndex=0;
	if(active->hint!=0)
	for(i=0;i<EIM.CandWordCount;i++)
		strcpy(EIM.CodeTips[i],CodeTips[i]);
	if(active==mb->ass_mb) /* it's assist mb */
	{
		/* hint the main mb's code */
		for(i=0;i<EIM.CandWordCount;i++)
		{
			int len;
			char *data=EIM.CandTable[i];
			EIM.CodeTips[i][0]=0;
			len=strlen(data);
			if((len==2 && gb_is_gbk((uint8_t*)data)) ||
					(len==4 && gb_is_gb18030_ext((uint8_t*)data)))
			{
				y_mb_get_full_code(mb,data,EIM.CodeTips[i]);
			}
			else
			{
				y_mb_get_exist_code(mb,data,EIM.CodeTips[i]);
			}
		}
	}
	else if(active==mb->quick_mb)
	{
		for(i=0;i<EIM.CandWordCount;i++)
		{
			EIM.CodeTips[i][0]=0;
		}
	}

	if(InsertMode)
	{
		CSET_GROUP_CALC *g=cset_get_group_by_type(&cs,CSET_TYPE_CALC);
		assert(g!=NULL);
		int pos=EIM.SelectIndex>=0?EIM.CurCandPage*EIM.CandWordMax+EIM.SelectIndex:-EIM.SelectIndex-2;
		char *outs[]={EIM.CodeInput,NULL};
		if(0==y_mb_code_by_rule(mb,g->phrase[pos],strlen(g->phrase[pos]),outs,NULL))
		{
			EIM.CodeLen=strlen(EIM.CodeInput);
			EIM.CaretPos=EIM.CodeLen;
		}
	}

	return IMR_DISPLAY;
}

#define ASSIST_MODE_INDICATOR	"^"
static void PinyinSetAssistCode(const char *s)
{
	bool dirty=false;
	if(!s || !s[0])
	{
		if(AssistCode[0])
		{
			AssistCode[0]=0;
			dirty=true;
		}
	}
	else
	{
		strcpy(AssistCode,s);
		dirty=true;
	}
	if(dirty)
	{
		EIM.Callback(EIM_CALLBACK_SET_ASSIST_CODE,AssistCode);
	}
}

static void PinyinInsertKey(int key)
{
	for(int i=EIM.CodeLen-1;i>=EIM.CaretPos;i--)
		EIM.CodeInput[i+1]=EIM.CodeInput[i];
	EIM.CodeInput[EIM.CaretPos]=key;
	EIM.CodeLen++;
	EIM.CaretPos++;
	EIM.CodeInput[EIM.CodeLen]=0;
}

static int PinyinDoInput(int key)
{
	struct y_mb *active_mb=Y_MB_ACTIVE(mb);
	bool del_key_used=false;

	if(!TableReady)
		return IMR_NEXT;

	key&=~KEYM_BING;

	if(key==key_backspace)
		key='\b';
	key=TableReplaceKey(key);

	if((AssistMode || AssistCode[0]) && key==YK_BACKSPACE)
	{
		AssistMode=0;
		PinyinSetAssistCode(NULL);
		PinyinDoSearch(0);
		return IMR_DISPLAY;
	}
	PinyinSetAssistCode(NULL);
	if(AssistMode)
	{
		if(!py_assist_series && AssistMode)
		{
			AssistMode=0;
		}
		if(key==YK_BACKSPACE)
		{
			PinyinSetAssistCode(NULL);
			PinyinDoSearch(0);
			return IMR_DISPLAY;
		}
		if(y_mb_is_assist_key(mb,key))
		{
			if(SP)
			{
				// reset SP 2+2 mode first
				int old=AssistMode;
				AssistMode=0;
				PinyinDoSearch(0);
				AssistMode=old;
			}
			CSET_GROUP_CALC *g=cset_calc_group_new(&cs);
			CSET_GROUP_PREDICT *predict=cset_get_group_by_type(&cs,CSET_TYPE_PREDICT);
			if(predict && predict->count>1)
			{
				int i;
				for(i=1;i<predict->count;i++)
				{
					const char *s=y_mb_predict_nth(predict->phrase,i);
					char temp[sizeof(struct y_mb_ci)+4];
					struct y_mb_ci *c=(struct y_mb_ci *)temp;
					c->len=4;
					strcpy((char*)c->data,s);
					if(!y_mb_assist_test(mb,c,key,0,0))
						continue;
					strcpy(&g->phrase[g->count][0],s);
					g->count++;
				}
			}
			PredictCalcMark=g->count;
			if(g->count<Y_MB_DATA_CALC)
			{
				g->count+=y_mb_assist_get(mb,
						g->phrase+g->count,
						Y_MB_DATA_CALC-g->count,
						key,0);
			}
			ExtraZi.mark=g->count;
			if(g->count<Y_MB_DATA_CALC && ExtraZi.count)
			{
				g->count+=ExtraZiGet(0,Y_MB_DATA_CALC-g->count,g->phrase+g->count,key);
			}
			if(g->count)
			{
				cset_prepend(&cs,(CSET_GROUP*)g);
				PhraseListCount=g->count;
				EIM.CandPageCount=PhraseListCount/EIM.CandWordMax+
						((PhraseListCount%EIM.CandWordMax)?1:0);
				PinyinGetCandwords(PAGE_FIRST);
			}
			char temp[2]={key,0};
			PinyinSetAssistCode(temp);
			return IMR_DISPLAY;
		}
	}
	if(key==YK_BACKSPACE || key==(YK_BACKSPACE|KEYM_SHIFT))
	{
		if(AssistMode)
		{
			AssistMode=0;
			PinyinSetAssistCode(NULL);
		}
		if(assoc_mode || EIM.WorkMode==EIM_WM_QUERY)
			return IMR_CLEAN;
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
		for(int i=EIM.CaretPos;i<EIM.CodeLen;i++)
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
		if(EIM.CodeLen==0 && EIM.StringGet[0]==0)
			return IMR_CLEAN;
		del_key_used=true;
	}
	else if(key==py_switch)
	{
		if(EIM.CodeLen==0)
			return IMR_NEXT;
		if(InsertMode)
			return IMR_NEXT;
		l_predict_simple_mode=0;
		if(CodeMatch>=1)
			PinyinDoSearch(-1);
		else
			PinyinDoSearch(0);
		if(EIM.CodeInput[0]!=mb->ass_lead)			// if assist, search phrase
			return IMR_DISPLAY;
	}
	else if(key==YK_DELETE)
	{
		if(EIM.CodeLen==0)
			return IMR_PASS;
		if(EIM.CodeLen!=EIM.CaretPos)
		{
			for(int i=EIM.CaretPos;i<EIM.CodeLen;i++)
				EIM.CodeInput[i]=EIM.CodeInput[i+1];
			EIM.CodeLen--;
			EIM.CodeInput[EIM.CodeLen]=0;
			del_key_used=true;
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
		if(EIM.CaretPos<0)
			return IMR_BLOCK;
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
		if(EIM.CaretPos<0)
			return IMR_BLOCK;
		if(EIM.CaretPos<EIM.CodeLen)
			EIM.CaretPos++;
		else
			EIM.CaretPos=0;
	}
	else if(key==YK_SPACE && py_space==' ')
	{
		if(EIM.CandWordCount==0 && CodeGetLen)
		{
			if(EIM.CodeLen)
				return IMR_BLOCK;
			if(auto_add)
				y_mb_add_phrase(mb,CodeGet,EIM.StringGet,0,Y_MB_DIC_USER);
			return IMR_COMMIT;
		}
		//if(CodeMatch || (!CodeMatch && !EIM.CodeLen) || EIM.CodeInput[0]==mb->lead)
			return IMR_NEXT;
	}
	else if(key==YK_ENTER)
	{
		if(InsertMode)
		{
			y_mb_add_phrase(mb,EIM.CodeInput,EIM.CandTable[EIM.SelectIndex],0,Y_MB_DIC_USER);
			TableReset();
			return IMR_CLEAN;
		}
		PinyinStripInput();
		return IMR_NEXT;
	}
	else if(key==key_cnen)
	{
		PinyinStripInput();
		return IMR_ENGLISH;
	}
	else if(key==YK_VIRT_REFRESH)
	{
		if(!y_mb_is_keys(mb,EIM.CodeInput))
			return IMR_NEXT;
	}
	else if(key==YK_VIRT_QUERY)
	{
		char *ph=NULL;
		if(EIM.CandWordCount)
			ph=EIM.CandTable[EIM.SelectIndex];
		else if(!EIM.CodeLen && EIM.GetSelect)
		{
			char *temp=EIM.GetSelect(TableOnVirtQuery);
			if(temp==NULL)
				return IMR_BLOCK;
			ph=l_strdupa(temp);
			l_free(temp);
		}
		return TableOnVirtQuery(ph);
	}
	else if(key==YK_VIRT_TIP)
	{
		int i=0;
		for(i=0;i<EIM.CandWordCount;i++)
		{
			if(l_gb_strlen(EIM.CandTable[i],-1)!=1)
				continue;
			y_mb_get_full_code(mb,EIM.CandTable[i],EIM.CodeTips[i]);
		}
		return IMR_DISPLAY;
	}
	else if(key==YK_VIRT_DEL)
	{
		char code[Y_MB_KEY_SIZE+1];
		int ret;
		if(EIM.CodeLen==0)
			return IMR_NEXT;
		if(InsertMode || CodeGetLen>0 || CodeMatch!=EIM.CodeLen)
			return IMR_BLOCK;
		if(SP)
		{
			py_conv_from_sp(EIM.CodeInput,code,sizeof(code),0);
			strcat(code,CodeTips[EIM.SelectIndex]);
		}
		else
		{
			strcpy(code,EIM.CodeInput);
			strcat(code,CodeTips[EIM.SelectIndex]);
		}
		ret=y_mb_del_phrase(mb,code,EIM.CandTable[EIM.SelectIndex]);
		if(ret==-1)
			return IMR_BLOCK;
		TableReset();
		return IMR_CLEAN;
	}
	else if(key==YK_VIRT_ADD)
	{
		char *ph=NULL;
		if(InsertMode || EIM.CodeLen)
			return IMR_BLOCK;
		if(EIM.GetSelect!=NULL)
		{
			ph=EIM.GetSelect(TableOnVirtAdd);
			if(ph==NULL)
				return IMR_BLOCK;
		}
		return TableOnVirtAdd(ph);
	}
	else if(key==key_move_up || key==key_move_down)
	{
		char code[Y_MB_KEY_SIZE+1];
		int ret;
		int pos;
		if(InsertMode)
			return IMR_BLOCK;
		if(EIM.CandWordCount==0)
			return IMR_NEXT;
		pos=EIM.CurCandPage*EIM.CandWordMax+EIM.SelectIndex;
		if(ExtraZi.count>0 && cset_calc_group_count(&cs)==0 && pos>=PhraseListCount-ExtraZi.count)
		{
			strcpy(code,ExtraZi.pinyin);
		}
		else if(ExtraZi.count>0 && cset_calc_group_count(&cs)>0 && pos>=ExtraZi.mark)
		{
			strcpy(code,ExtraZi.pinyin);
		}
		else
		{
			strcpy(code,EIM.CodeInput);
			strcat(code,CodeTips[EIM.SelectIndex]);
		}
		if(SP==1)
		{
			char temp[MAX_CODE_LEN+1];
			py_conv_from_sp(code,temp,sizeof(temp),0);
			strcpy(code,temp);
		}
		ret=y_mb_move_phrase(mb,code,EIM.CandTable[EIM.SelectIndex],
			(key==key_move_up)?-1:1);
		if(ret==-1)
			return IMR_BLOCK;
		PinyinGetCandwords(PAGE_REFRESH);
		return IMR_DISPLAY;
	}
	else if(InsertMode && key==key_clear_code && EIM.CodeLen>0)
	{
		EIM.CodeLen=0;
		EIM.CaretPos=0;
		EIM.CodeInput[0]=0;
		return IMR_DISPLAY;
	}
	else if(key==key_filter && (EIM.CodeLen!=0 || key>=0x80) && !InsertMode)
	{
		if(EIM.CodeLen==0 && key>=0x80)
		{
			hz_filter=!hz_filter;
			hz_filter_temp=hz_filter;
			EIM.ShowTip("汉字过滤：%s",hz_filter?"开启":"关闭");
			return IMR_BLOCK;
		}
		hz_filter=1;
		if(!hz_filter_temp)
			hz_filter_temp=hz_filter;
		else
			hz_filter_temp=0;
	}
	else if(key==mb->suffix && EIM.CurCandPage==0 && EIM.CodeLen>=1 && !InsertMode && EIM.CaretPos==EIM.CodeLen)
	{
		char code[4];		
		int ret;
		code[0]=EIM.CodeInput[EIM.CodeLen-1];
		code[1]=code[2]=0;
		EIM.CodeInput[--EIM.CodeLen]=0;
		EIM.CaretPos=EIM.CodeLen;
		int got=strlen(EIM.StringGet);
		int cur;
		if(EIM.CodeLen>0)
		{
			PinyinDoSearch(0);
			cur=strlen(EIM.CandTable[0]);
			if(got+cur>MAX_CODE_LEN)
				return IMR_CLEAN;
			got+=cur;
			strcat(EIM.StringGet,EIM.CandTable[0]);
		}
		if(key>0x20)
		{
			code[1]=key;
			ret=y_mb_set(mb,code,2,hz_filter_temp);
		}
		else
		{
			ret=0;
		}
		if(ret==0)
		{
			code[1]='\0';
			ret=y_mb_set(mb,code,1,hz_filter_temp);
			if(ret==0)
				return IMR_CLEAN;
		}
		y_mb_get(mb,0,1,EIM.CandTable,NULL);
		cur=strlen(EIM.CandTable[0]);
		if(got+cur>MAX_CODE_LEN)
			return IMR_CLEAN;
		got+=cur;
		strcat(EIM.StringGet,EIM.CandTable[0]);
		return IMR_COMMIT;
	}
	else if(active_mb==mb && 
			(y_mb_is_key(mb,key) ||
			(key==mb->split && !SP && key=='\'') ||
				(SP==1 && key==';' && EIM.CaretPos &&
				 (py_sp_has_semi() || (EIM.CaretPos>=2 && mb->ass_mb && y_mb_is_key(mb->ass_mb,';')))
				)
			))
	{
		if(CodeGetLen+EIM.CodeLen>=64)
			return IMR_BLOCK;
		if(key==mb->wildcard)
			return IMR_NEXT;
		if(EIM.CodeLen==0 && CodeGetLen==0)
		{
			EIM.CaretPos=0;
			cset_clear(&cs,CSET_TYPE_CALC);
			if(!InsertMode)
				EIM.StringGet[0]=0;
			EIM.SelectIndex=0;
			mb->ctx.input[0]=0;
			assoc_mode=0;
		}
		PinyinInsertKey(key);
		l_predict_simple_mode=-1;
	}
	else if(CodeGetLen==0 && ShouldEnterAssistMode(key)!=NULL)
	{
		if(EIM.CodeLen<active_mb->len)
		{
			if(EIM.CodeLen==0)
				EIM.CaretPos=0;
			PinyinInsertKey(key);
		}
		if(EIM.CodeLen==1)
			return IMR_DISPLAY;
		active_mb=Y_MB_ACTIVE(mb);
	}
	else if(active_mb==mb &&
			(SP || (mb->split!='\'' && !y_mb_is_key(mb,key))) &&
			key=='\'' && CodeGetLen==0 && EIM.CodeLen>=2 && l_predict_simple && EIM.CaretPos==EIM.CodeLen)
	{
		l_predict_simple_mode=!(l_predict_simple_mode>0);
	}
	else if(active_mb==mb &&
			!SP && key==(KEYM_CTRL|'\'')  && CodeGetLen==0 && EIM.CodeLen>=2 && l_predict_simple && EIM.CaretPos==EIM.CodeLen)
	{
		l_predict_simple_mode=!(l_predict_simple_mode>0);
	}
	else if(active_mb==mb && key==YK_TAB)
	{
		if(EIM.CandWordCount && (mb->ass_mb || mb->yong) &&
				!InsertMode)
		{
			if(SP && EIM.CodeLen>=4 && EIM.CodeLen<=5)
			{
				AssistMode=1;
				l_predict_simple_mode=-1;
				PinyinDoSearch(0);
				
				// 如果没有发现符合的2+2直接辅助码，那么保持间接辅助码形式
				if(cset_calc_group_count(&cs)==0)
				{
					AssistMode=1;
					PinyinSetAssistCode(ASSIST_MODE_INDICATOR);
					return IMR_DISPLAY;
				}

				// 有四码的词的话，保持辅助码模式，这样后续可以继续进行词间接辅助码
				if(mb->ctx.result_first)
				{
					struct y_mb_ci *c;
					for(c=mb->ctx.result_first->phrase;c!=NULL;c=c->next)
					{
						if(c->del) continue;
						AssistMode=1;
						PinyinSetAssistCode(ASSIST_MODE_INDICATOR);
						return IMR_DISPLAY;
					}
				}
				AssistMode=0;
				PinyinSetAssistCode(ASSIST_MODE_INDICATOR);
				return IMR_DISPLAY;
			}
			AssistMode=1;
			PinyinSetAssistCode(ASSIST_MODE_INDICATOR);
			return IMR_DISPLAY;
		}
		return IMR_NEXT;
	}
	else if(key>='A' && key<='Z')
	{
		if(EIM.CodeLen>=1)
			PinyinMoveCaretTo(key);
		else
			return IMR_NEXT;
	}
	else if(key>=YK_VIRT_CARET && key<=YK_VIRT_CARET+MAX_CODE_LEN)
	{
		int i=key-YK_VIRT_CARET;
		if(i<EIM.CodeLen)
		{
			EIM.CaretPos=i;
		}
	}
	else
	{
		return IMR_NEXT;
	}
	if(!InsertMode)
	{
		PinyinDoSearch(0);
		if(PhraseListCount==0 && CodeGetLen+EIM.CodeLen<=1 && Y_MB_ACTIVE(mb)==mb)
		{
			if(!del_key_used)
			{
				TableReset();
				return IMR_NEXT;
			}
			return IMR_CLEAN;
		}
		else if(PhraseListCount>=1 && !AssistMode && CodeGetLen==0 && CodeMatch==EIM.CodeLen && !CodeTips[0][0])
		{
			if(l_str_has_suffix(EIM.CandTable[0],"$SPACE"))
			{
				char *p=EIM.CandTable[0];
				size_t len=strlen(p);
				if(len!=6 && !strstr(p,"$$"))
				{
					p[len-6]=0;
					strcpy(EIM.StringGet,p);
					return IMR_COMMIT;
				}
			}
		} 
	}
	return IMR_DISPLAY;
}

#ifndef CFG_XIM_ANDROID
L_EXPORT(int tool_save_user(void *arg,void **out))
{
	if(!mb)
		return -1;
	y_mb_save_user(mb);
	return 0;
}

L_EXPORT(int tool_get_file(void *arg,void **out))
{
	if(!mb || !arg || !out)
		return -1;
	if(!strcmp(arg,"main"))
		*out=mb->main;
	else if(!strcmp(arg,"user"))
		*out=mb->user;
	else
		return -1;
	return 0;
}

L_EXPORT(int tool_optimize(void *arg,void **out))
{
	char temp[256];
	FILE *fp;
	struct y_mb_arg mb_arg;
	if(!mb)	return -1;
	
	strcpy(temp,mb->main);
	y_mb_free(mb);
	memset(&mb_arg,0,sizeof(mb_arg));
	mb_arg.dicts=EIM.GetConfig(NULL,"dicts");
	if(!mb_arg.dicts || !mb_arg.dicts[0])
		mb_arg.dicts=EIM.GetConfig("table","dicts");
	/* MB_FLAG_SLOW 会在大码表下变得很慢 */
	mb=y_mb_load(temp,MB_FLAG_SLOW|MB_FLAG_NOUSER|MB_FLAG_NODICTS,NULL);
	if(!mb)
		return -1;
	fp=y_mb_open_file(mb->main,(mb->encrypt?"wb":"wb"));
	if(!fp)
		return -1;
	y_mb_dump(mb,fp,MB_DUMP_MAIN|MB_DUMP_HEAD|MB_DUMP_ADJUST,
			MB_FMT_YONG,0);
	fclose(fp);
	y_mb_free(mb);
	mb=y_mb_load(temp,mb_flag,&mb_arg);
	y_mb_load_pin(mb,EIM.GetConfig(0,"pin"));
	return 0;
}

L_EXPORT(int tool_merge_user(void *arg,void **out))
{
	FILE *fp;
	if(!mb)	return -1;
		
	fp=y_mb_open_file(mb->main,(mb->encrypt?"wb":"w"));
	if(!fp) return -1;
	y_mb_dump(mb,fp,MB_DUMP_MAIN|MB_DUMP_USER|MB_DUMP_HEAD|MB_DUMP_ADJUST,
			MB_FMT_YONG,0);
	fclose(fp);
	return 0;
}
#endif
