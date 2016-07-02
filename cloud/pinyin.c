#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pinyin.h"
#include "trie.h"
#include "ltricky.h"
#include "gbk.h"
#include "llib.h"

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define MAX_PYLEN		6
#define MAX_DISPLAY		512
#define PY_VAL(a,b) ((a)<<8|(b))
#define PY_ITEM(a,b,c,d) {(a)<<8|(b),(c),0,(d)}

struct py_item{
	uint16_t val;
	uint8_t yun:2;
	uint8_t len:3;
	const char *quan;
};

static int py_split='\'';
static char py_split_s[2]="'";
static char sp_semicolon=0;
static int py_type=0;

static struct py_item py_caret=
	PY_ITEM(0,1,0,0);
static struct py_item py_split_item=
	PY_ITEM(0,0,1,py_split_s);

static struct py_item py_all[]={
	PY_ITEM('a','a',0,"a"),
	PY_ITEM('a','i',0,"ai"),
	PY_ITEM('a','n',0,"an"),
	PY_ITEM('a','h',0,"ang"),
	PY_ITEM('a','o',0,"ao"),
	
	PY_ITEM('b','a',1,"ba"),
	PY_ITEM('b','l',1,"bai"),
	PY_ITEM('b','j',1,"ban"),
	PY_ITEM('b','h',1,"bang"),
	PY_ITEM('b','k',1,"bao"),
	PY_ITEM('b','z',1,"bei"),
	PY_ITEM('b','f',1,"ben"),
	PY_ITEM('b','g',1,"beng"),
	PY_ITEM('b','i',1,"bi"),
	PY_ITEM('b','m',1,"bian"),
	PY_ITEM('b','c',1,"biao"),
	PY_ITEM('b','x',1,"bie"),
	PY_ITEM('b','n',1,"bin"),
	PY_ITEM('b','y',1,"bing"),
	PY_ITEM('b','o',1,"bo"),
	PY_ITEM('b','u',1,"bu"),
	
	PY_ITEM('c','a',1,"ca"),
	PY_ITEM('c','l',1,"cai"),
	PY_ITEM('c','j',1,"can"),
	PY_ITEM('c','h',1,"cang"),
	PY_ITEM('c','k',1,"cao"),
	PY_ITEM('c','e',1,"ce"),
	PY_ITEM('c','f',1,"cen"),
	PY_ITEM('c','g',1,"ceng"),
	PY_ITEM('c','i',1,"ci"),
	PY_ITEM('c','s',1,"cong"),
	PY_ITEM('c','b',1,"cou"),
	PY_ITEM('c','u',1,"cu"),
	PY_ITEM('c','r',1,"cuan"),
	PY_ITEM('c','v',1,"cui"),
	PY_ITEM('c','p',1,"cun"),
	PY_ITEM('c','o',1,"cuo"),
	
	PY_ITEM('i','a',2,"cha"),
	PY_ITEM('i','l',2,"chai"),
	PY_ITEM('i','j',2,"chan"),
	PY_ITEM('i','h',2,"chang"),
	PY_ITEM('i','k',2,"chao"),
	PY_ITEM('i','e',2,"che"),
	PY_ITEM('i','f',2,"chen"),
	PY_ITEM('i','g',2,"cheng"),
	PY_ITEM('i','i',2,"chi"),
	PY_ITEM('i','s',2,"chong"),
	PY_ITEM('i','b',2,"chou"),
	PY_ITEM('i','u',2,"chu"),
	PY_ITEM('i','y',2,"chuai"),
	PY_ITEM('i','r',2,"chuan"),
	PY_ITEM('i','d',2,"chuang"),
	PY_ITEM('i','v',2,"chui"),
	PY_ITEM('i','p',2,"chun"),
	PY_ITEM('i','o',2,"chuo"),
	
	PY_ITEM('d','a',1,"da"),
	PY_ITEM('d','l',1,"dai"),
	PY_ITEM('d','j',1,"dan"),
	PY_ITEM('d','h',1,"dang"),
	PY_ITEM('d','k',1,"dao"),
	PY_ITEM('d','e',1,"de"),
	PY_ITEM('d','z',1,"dei"),
	PY_ITEM('d','f',1,"den"),
	PY_ITEM('d','g',1,"deng"),
	PY_ITEM('d','i',1,"di"),
	PY_ITEM('d','w',1,"dia"),
	PY_ITEM('d','m',1,"dian"),
	PY_ITEM('d','c',1,"diao"),
	PY_ITEM('d','x',1,"die"),
	PY_ITEM('d','y',1,"ding"),
	PY_ITEM('d','q',1,"diu"),
	PY_ITEM('d','s',1,"dong"),
	PY_ITEM('d','b',1,"dou"),
	PY_ITEM('d','u',1,"du"),
	PY_ITEM('d','r',1,"duan"),
	PY_ITEM('d','v',1,"dui"),
	PY_ITEM('d','p',1,"dun"),
	PY_ITEM('d','o',1,"duo"),
	
	PY_ITEM('e','e',0,"e"),
	PY_ITEM('e','i',0,"ei"),
	PY_ITEM('e','n',0,"en"),
	PY_ITEM('e','r',0,"er"),
	
	PY_ITEM('f','a',1,"fa"),
	PY_ITEM('f','j',1,"fan"),
	PY_ITEM('f','h',1,"fang"),
	PY_ITEM('f','z',1,"fei"),
	PY_ITEM('f','f',1,"fen"),
	PY_ITEM('f','g',1,"feng"),
	PY_ITEM('f','c',1,"fiao"),
	PY_ITEM('f','o',1,"fo"),
	PY_ITEM('f','b',1,"fou"),
	PY_ITEM('f','u',1,"fu"),
	
	PY_ITEM('g','a',1,"ga"),
	PY_ITEM('g','l',1,"gai"),
	PY_ITEM('g','j',1,"gan"),
	PY_ITEM('g','h',1,"gang"),
	PY_ITEM('g','k',1,"gao"),
	PY_ITEM('g','e',1,"ge"),
	PY_ITEM('g','z',1,"gei"),
	PY_ITEM('g','f',1,"gen"),
	PY_ITEM('g','g',1,"geng"),
	PY_ITEM('g','s',1,"gong"),
	PY_ITEM('g','b',1,"gou"),
	PY_ITEM('g','u',1,"gu"),
	PY_ITEM('g','w',1,"gua"),
	PY_ITEM('g','y',1,"guai"),
	PY_ITEM('g','r',1,"guan"),
	PY_ITEM('g','d',1,"guang"),
	PY_ITEM('g','v',1,"gui"),
	PY_ITEM('g','p',1,"gun"),
	PY_ITEM('g','o',1,"guo"),
	
	PY_ITEM('h','a',1,"ha"),
	PY_ITEM('h','l',1,"hai"),
	PY_ITEM('h','j',1,"han"),
	PY_ITEM('h','h',1,"hang"),
	PY_ITEM('h','k',1,"hao"),
	PY_ITEM('h','e',1,"he"),
	PY_ITEM('h','z',1,"hei"),
	PY_ITEM('h','f',1,"hen"),
	PY_ITEM('h','g',1,"heng"),
	PY_ITEM('h','s',1,"hong"),
	PY_ITEM('h','b',1,"hou"),
	PY_ITEM('h','u',1,"hu"),
	PY_ITEM('h','w',1,"hua"),
	PY_ITEM('h','y',1,"huai"),
	PY_ITEM('h','r',1,"huan"),
	PY_ITEM('h','d',1,"huang"),
	PY_ITEM('h','v',1,"hui"),
	PY_ITEM('h','p',1,"hun"),
	PY_ITEM('h','o',1,"huo"),

	PY_ITEM('j','i',1,"ji"),
	PY_ITEM('j','w',1,"jia"),
	PY_ITEM('j','m',1,"jian"),
	PY_ITEM('j','d',1,"jiang"),
	PY_ITEM('j','c',1,"jiao"),
	PY_ITEM('j','x',1,"jie"),
	PY_ITEM('j','n',1,"jin"),
	PY_ITEM('j','y',1,"jing"),
	PY_ITEM('j','s',1,"jiong"),
	PY_ITEM('j','q',1,"jiu"),
	PY_ITEM('j','u',1,"ju"),
	PY_ITEM('j','r',1,"juan"),
	PY_ITEM('j','t',1,"jue"),
	PY_ITEM('j','p',1,"jun"),
	
	PY_ITEM('k','a',1,"ka"),
	PY_ITEM('k','l',1,"kai"),
	PY_ITEM('k','j',1,"kan"),
	PY_ITEM('k','h',1,"kang"),
	PY_ITEM('k','k',1,"kao"),
	PY_ITEM('k','e',1,"ke"),
	PY_ITEM('k','z',1,"kei"),
	PY_ITEM('k','f',1,"ken"),
	PY_ITEM('k','g',1,"keng"),
	PY_ITEM('k','s',1,"kong"),
	PY_ITEM('k','b',1,"kou"),
	PY_ITEM('k','u',1,"ku"),
	PY_ITEM('k','w',1,"kua"),
	PY_ITEM('k','y',1,"kuai"),
	PY_ITEM('k','r',1,"kuan"),
	PY_ITEM('k','d',1,"kuang"),
	PY_ITEM('k','v',1,"kui"),
	PY_ITEM('k','p',1,"kun"),
	PY_ITEM('k','o',1,"kuo"),
	
	PY_ITEM('l','a',1,"la"),
	PY_ITEM('l','l',1,"lai"),
	PY_ITEM('l','j',1,"lan"),
	PY_ITEM('l','h',1,"lang"),
	PY_ITEM('l','k',1,"lao"),
	PY_ITEM('l','e',1,"le"),
	PY_ITEM('l','z',1,"lei"),
	PY_ITEM('l','g',1,"leng"),
	PY_ITEM('l','i',1,"li"),
	PY_ITEM('l','w',1,"lia"),
	PY_ITEM('l','m',1,"lian"),
	PY_ITEM('l','d',1,"liang"),
	PY_ITEM('l','c',1,"liao"),
	PY_ITEM('l','x',1,"lie"),
	PY_ITEM('l','n',1,"lin"),
	PY_ITEM('l','y',1,"ling"),
	PY_ITEM('l','q',1,"liu"),
	PY_ITEM('l','s',1,"long"),
	PY_ITEM('l','b',1,"lou"),
	PY_ITEM('l','u',1,"lu"),
	PY_ITEM('l','r',1,"luan"),
	PY_ITEM('l','t',1,"lue"),
	PY_ITEM('l','p',1,"lun"),
	PY_ITEM('l','o',1,"luo"),
	PY_ITEM('l','v',1,"lv"),
	PY_ITEM('l','T',1,"lve"),
	
	PY_ITEM('m','a',1,"ma"),
	PY_ITEM('m','l',1,"mai"),
	PY_ITEM('m','j',1,"man"),
	PY_ITEM('m','h',1,"mang"),
	PY_ITEM('m','k',1,"mao"),
	PY_ITEM('m','e',1,"me"),
	PY_ITEM('m','z',1,"mei"),
	PY_ITEM('m','f',1,"men"),
	PY_ITEM('m','g',1,"meng"),
	PY_ITEM('m','i',1,"mi"),
	PY_ITEM('m','m',1,"mian"),
	PY_ITEM('m','c',1,"miao"),
	PY_ITEM('m','x',1,"mie"),
	PY_ITEM('m','n',1,"min"),
	PY_ITEM('m','y',1,"ming"),
	PY_ITEM('m','q',1,"miu"),
	PY_ITEM('m','o',1,"mo"),
	PY_ITEM('m','b',1,"mou"),
	PY_ITEM('m','u',1,"mu"),
	
	PY_ITEM('n','a',1,"na"),
	PY_ITEM('n','l',1,"nai"),
	PY_ITEM('n','j',1,"nan"),
	PY_ITEM('n','h',1,"nang"),
	PY_ITEM('n','k',1,"nao"),
	PY_ITEM('n','e',1,"ne"),
	PY_ITEM('n','z',1,"nei"),
	PY_ITEM('n','f',1,"nen"),
	PY_ITEM('n','g',1,"neng"),
	PY_ITEM('n','i',1,"ni"),
	PY_ITEM('n','m',1,"nian"),
	PY_ITEM('n','d',1,"niang"),
	PY_ITEM('n','c',1,"niao"),
	PY_ITEM('n','x',1,"nie"),
	PY_ITEM('n','n',1,"nin"),
	PY_ITEM('n','y',1,"ning"),
	PY_ITEM('n','q',1,"niu"),
	PY_ITEM('n','s',1,"nong"),
	PY_ITEM('n','b',1,"nou"),
	PY_ITEM('n','u',1,"nu"),
	PY_ITEM('n','r',1,"nuan"),
	PY_ITEM('n','t',1,"nue"),
	PY_ITEM('n','p',1,"nun"),
	PY_ITEM('n','o',1,"nuo"),
	PY_ITEM('n','v',1,"nv"),
	PY_ITEM('n','T',1,"nve"),
	
	PY_ITEM('o','o',0,"o"),
	PY_ITEM('o','u',0,"ou"),
	
	PY_ITEM('p','a',1,"pa"),
	PY_ITEM('p','l',1,"pai"),
	PY_ITEM('p','j',1,"pan"),
	PY_ITEM('p','h',1,"pang"),
	PY_ITEM('p','k',1,"pao"),
	PY_ITEM('p','z',1,"pei"),
	PY_ITEM('p','f',1,"pen"),
	PY_ITEM('p','g',1,"peng"),
	PY_ITEM('p','i',1,"pi"),
	PY_ITEM('p','m',1,"pian"),
	PY_ITEM('p','c',1,"piao"),
	PY_ITEM('p','x',1,"pie"),
	PY_ITEM('p','n',1,"pin"),
	PY_ITEM('p','y',1,"ping"),
	PY_ITEM('p','o',1,"po"),
	PY_ITEM('p','b',1,"pou"),
	PY_ITEM('p','u',1,"pu"),
	
	PY_ITEM('q','i',1,"qi"),
	PY_ITEM('q','w',1,"qia"),
	PY_ITEM('q','m',1,"qian"),
	PY_ITEM('q','d',1,"qiang"),
	PY_ITEM('q','c',1,"qiao"),
	PY_ITEM('q','x',1,"qie"),
	PY_ITEM('q','n',1,"qin"),
	PY_ITEM('q','y',1,"qing"),
	PY_ITEM('q','s',1,"qiong"),
	PY_ITEM('q','q',1,"qiu"),
	PY_ITEM('q','o',1,"qo"),
	PY_ITEM('q','u',1,"qu"),
	PY_ITEM('q','r',1,"quan"),
	PY_ITEM('q','t',1,"que"),
	PY_ITEM('q','p',1,"qun"),
	
	PY_ITEM('r','j',1,"ran"),
	PY_ITEM('r','h',1,"rang"),
	PY_ITEM('r','k',1,"rao"),
	PY_ITEM('r','e',1,"re"),
	PY_ITEM('r','f',1,"ren"),
	PY_ITEM('r','g',1,"reng"),
	PY_ITEM('r','i',1,"ri"),
	PY_ITEM('r','s',1,"rong"),
	PY_ITEM('r','b',1,"rou"),
	PY_ITEM('r','u',1,"ru"),
	PY_ITEM('r','r',1,"ruan"),
	PY_ITEM('r','v',1,"rui"),
	PY_ITEM('r','p',1,"run"),
	PY_ITEM('r','o',1,"ruo"),
	
	PY_ITEM('s','a',1,"sa"),
	PY_ITEM('s','l',1,"sai"),
	PY_ITEM('s','j',1,"san"),
	PY_ITEM('s','h',1,"sang"),
	PY_ITEM('s','k',1,"sao"),
	PY_ITEM('s','e',1,"se"),
	PY_ITEM('s','f',1,"sen"),
	PY_ITEM('s','g',1,"seng"),
	PY_ITEM('s','i',1,"si"),
	PY_ITEM('s','s',1,"song"),
	PY_ITEM('s','b',1,"sou"),
	PY_ITEM('s','u',1,"su"),
	PY_ITEM('s','r',1,"suan"),
	PY_ITEM('s','v',1,"sui"),
	PY_ITEM('s','p',1,"sun"),
	PY_ITEM('s','o',1,"suo"),
	
	PY_ITEM('u','a',2,"sha"),
	PY_ITEM('u','l',2,"shai"),
	PY_ITEM('u','j',2,"shan"),
	PY_ITEM('u','h',2,"shang"),
	PY_ITEM('u','k',2,"shao"),
	PY_ITEM('u','e',2,"she"),
	PY_ITEM('u','z',2,"shei"),
	PY_ITEM('u','f',2,"shen"),
	PY_ITEM('u','g',2,"sheng"),
	PY_ITEM('u','i',2,"shi"),
	PY_ITEM('u','b',2,"shou"),
	PY_ITEM('u','u',2,"shu"),
	PY_ITEM('u','w',2,"shua"),
	PY_ITEM('u','y',2,"shuai"),
	PY_ITEM('u','r',2,"shuan"),
	PY_ITEM('u','d',2,"shuang"),
	PY_ITEM('u','v',2,"shui"),
	PY_ITEM('u','p',2,"shun"),
	PY_ITEM('u','o',2,"shuo"),
	
	PY_ITEM('t','a',1,"ta"),
	PY_ITEM('t','l',1,"tai"),
	PY_ITEM('t','j',1,"tan"),
	PY_ITEM('t','h',1,"tang"),
	PY_ITEM('t','k',1,"tao"),
	PY_ITEM('t','e',1,"te"),
	PY_ITEM('t','g',1,"teng"),
	PY_ITEM('t','i',1,"ti"),
	PY_ITEM('t','m',1,"tian"),
	PY_ITEM('t','c',1,"tiao"),
	PY_ITEM('t','x',1,"tie"),
	PY_ITEM('t','y',1,"ting"),
	PY_ITEM('t','s',1,"tong"),
	PY_ITEM('t','b',1,"tou"),
	PY_ITEM('t','u',1,"tu"),
	PY_ITEM('t','r',1,"tuan"),
	PY_ITEM('t','v',1,"tui"),
	PY_ITEM('t','p',1,"tun"),
	PY_ITEM('t','o',1,"tuo"),
	
	PY_ITEM('w','a',1,"wa"),
	PY_ITEM('w','l',1,"wai"),
	PY_ITEM('w','j',1,"wan"),
	PY_ITEM('w','h',1,"wang"),
	PY_ITEM('w','k',1,"wao"),
	PY_ITEM('w','z',1,"wei"),
	PY_ITEM('w','f',1,"wen"),
	PY_ITEM('w','g',1,"weng"),
	PY_ITEM('w','o',1,"wo"),
	PY_ITEM('w','u',1,"wu"),
	
	PY_ITEM('x','i',1,"xi"),
	PY_ITEM('x','w',1,"xia"),
	PY_ITEM('x','m',1,"xian"),
	PY_ITEM('x','d',1,"xiang"),
	PY_ITEM('x','c',1,"xiao"),
	PY_ITEM('x','x',1,"xie"),
	PY_ITEM('x','n',1,"xin"),
	PY_ITEM('x','y',1,"xing"),
	PY_ITEM('x','s',1,"xiong"),
	PY_ITEM('x','q',1,"xiu"),
	PY_ITEM('x','u',1,"xu"),
	PY_ITEM('x','r',1,"xuan"),
	PY_ITEM('x','t',1,"xue"),
	PY_ITEM('x','p',1,"xun"),

	PY_ITEM('y','a',1,"ya"),
	PY_ITEM('y','j',1,"yan"),
	PY_ITEM('y','h',1,"yang"),
	PY_ITEM('y','k',1,"yao"),
	PY_ITEM('y','e',1,"ye"),
	PY_ITEM('y','i',1,"yi"),
	PY_ITEM('y','n',1,"yin"),
	PY_ITEM('y','y',1,"ying"),
	PY_ITEM('y','o',1,"yo"),
	PY_ITEM('y','s',1,"yong"),
	PY_ITEM('y','b',1,"you"),
	PY_ITEM('y','u',1,"yu"),
	PY_ITEM('y','r',1,"yuan"),
	PY_ITEM('y','t',1,"yue"),
	PY_ITEM('y','p',1,"yun"),
	
	PY_ITEM('z','a',1,"za"),
	PY_ITEM('z','l',1,"zai"),
	PY_ITEM('z','j',1,"zan"),
	PY_ITEM('z','h',1,"zang"),
	PY_ITEM('z','k',1,"zao"),
	PY_ITEM('z','e',1,"ze"),
	PY_ITEM('z','z',1,"zei"),
	PY_ITEM('z','f',1,"zen"),
	PY_ITEM('z','g',1,"zeng"),
	PY_ITEM('z','i',1,"zi"),
	PY_ITEM('z','s',1,"zong"),
	PY_ITEM('z','b',1,"zou"),
	PY_ITEM('z','u',1,"zu"),
	PY_ITEM('z','r',1,"zuan"),
	PY_ITEM('z','v',1,"zui"),
	PY_ITEM('z','p',1,"zun"),
	PY_ITEM('z','o',1,"zuo"),
	
	PY_ITEM('v','a',2,"zha"),
	PY_ITEM('v','l',2,"zhai"),
	PY_ITEM('v','j',2,"zhan"),
	PY_ITEM('v','h',2,"zhang"),
	PY_ITEM('v','k',2,"zhao"),
	PY_ITEM('v','e',2,"zhe"),
	PY_ITEM('v','z',2,"zhei"),
	PY_ITEM('v','f',2,"zhen"),
	PY_ITEM('v','g',2,"zheng"),
	PY_ITEM('v','i',2,"zhi"),
	PY_ITEM('v','s',2,"zhong"),
	PY_ITEM('v','b',2,"zhou"),
	PY_ITEM('v','u',2,"zhu"),
	PY_ITEM('v','w',2,"zhua"),
	PY_ITEM('v','y',2,"zhuai"),
	PY_ITEM('v','r',2,"zhuan"),
	PY_ITEM('v','d',2,"zhuang"),
	PY_ITEM('v','v',2,"zhui"),
	PY_ITEM('v','p',2,"zhun"),
	PY_ITEM('v','o',2,"zhuo"),
	
	PY_ITEM('a',0,1,"a"),
	PY_ITEM('b',0,1,"b"),
	PY_ITEM('c',0,1,"c"),
	PY_ITEM('d',0,1,"d"),
	PY_ITEM('e',0,1,"e"),
	PY_ITEM('f',0,1,"f"),
	PY_ITEM('g',0,1,"g"),
	PY_ITEM('h',0,1,"h"),
	PY_ITEM('j',0,1,"j"),
	PY_ITEM('k',0,1,"k"),
	PY_ITEM('l',0,1,"l"),
	PY_ITEM('m',0,1,"m"),
	PY_ITEM('n',0,1,"n"),
	PY_ITEM('o',0,1,"o"),
	PY_ITEM('p',0,1,"p"),
	PY_ITEM('q',0,1,"q"),
	PY_ITEM('r',0,1,"r"),
	PY_ITEM('s',0,1,"s"),
	PY_ITEM('t',0,1,"t"),
	PY_ITEM('w',0,1,"w"),
	PY_ITEM('x',0,1,"x"),
	PY_ITEM('y',0,1,"y"),
	PY_ITEM('z',0,1,"z"),
	PY_ITEM('i',0,2,"ch"),
	PY_ITEM('u',0,2,"sh"),
	PY_ITEM('v',0,2,"zh"),
	
	PY_ITEM(0,2,0,"u"),
	PY_ITEM(0,2,0,"v"),
	PY_ITEM(0,2,0,"i"),
	
	PY_ITEM(0,0,1,py_split_s),
};

#define PY_COUNT (sizeof(py_all)/sizeof(struct py_item))

static struct py_item *sp_index[PY_COUNT];
static py_tree_t py_index;

static int item_cmpr(const void *p1,const void *p2)
{
	struct py_item *i1=(struct py_item*)p1;
	struct py_item *i2=(struct py_item*)p2;
	int ret=i1->len-i2->len;
	if(ret!=0) return ret;
	return strncmp(i1->quan,i2->quan,i1->len);
}

static int sp_cmpr(const void *p1,const void *p2)
{
	struct py_item *i1=*(struct py_item**)p1;
	struct py_item *i2=*(struct py_item**)p2;
	return i1->val-i2->val;
}

void py_init(int split,char *sp)
{
	int i;

	if(split)
	{
		py_split=split;
		py_split_s[0]=split;
	}
	if(py_split=='\'')
		py_type=0;
	else if(sp)
		py_type=1;
	else
		py_type=2;

	for(i=0;i<PY_COUNT;i++)
		py_all[i].len=strlen(py_all[i].quan);
	qsort(py_all,PY_COUNT,sizeof(struct py_item),item_cmpr);

	if(sp && sp[0])
	{
		FILE *fp=fopen(sp,"r");
		if(fp)
		{
			//int ret;
			char line[256];
			char *quan,*shuang;
			//while((ret=fscanf(fp,"%s %s\n",quan,shuang))==2)
			while(fgets(line,256,fp))
			{
				struct py_item it,*res;
				int len;
				if(line[0]=='#') continue;
				len=strcspn(line,"\r\n");line[len]=0;
				quan=line;
				shuang=strchr(line,' ');
				if(!shuang) break;
				*shuang++=0;
				len=strcspn(shuang," ");shuang[len]=0;
				if(len!=2)
				{
					//printf("%s != 2\n",shuang);
					continue;
				}
				it.len=strlen(quan);
				it.quan=quan;
				res=bsearch(&it,py_all,PY_COUNT,sizeof(struct py_item),item_cmpr);
				if(!res)
				{
					//printf("not found %s\n",quan);
					continue;
				}
				res->val=shuang[0]<<8|shuang[1];
				if(shuang[1]==';') sp_semicolon=1;
				//printf("%s %s\n",shuang,quan);
			}
			fclose(fp);
		}
	}
	
	for(i=0;i<PY_COUNT;i++)
		sp_index[i]=&py_all[i];
	qsort(sp_index,PY_COUNT,sizeof(struct py_item*),sp_cmpr);
	
	py_split_item.len=1;
	py_tree_init(&py_index);
	for(i=0;i<PY_COUNT;i++)
	{
		struct py_item *pi=py_all+i;
		if(pi->val<0x100) continue;
		if(pi->val==PY_VAL('a',0)) continue;
		if(pi->val==PY_VAL('e',0)) continue;
		if(pi->val==PY_VAL('o',0)) continue;
		py_tree_add(&py_index,pi->quan,pi->len,i);
	}
}

typedef struct py_parser{
	py_item_t *token;
	int count;
	int caret;
}PY_PARSER;

#if 1

int py_is_valid_code(const char *in)
{
	if(py_split=='\'')
	{
		int count;
		int out[6];
		struct py_item *p;
		count=py_tree_get(&py_index,in,out);
		if(count<=0)
			return 0;
		p=py_all+out[count-1];
		if(in[p->len]!=0)
			return 0;
		if(p->len==p->yun)
			return 0;
		return 1;
	}
	else
	{
		return strlen(in)==py_split;
	}
}

static int py_parse_r(PY_PARSER *parser,const char *input,int len)
{
	struct py_item *res=NULL;
	
	while(input[0]==' ')
	{
		input++;
		len--;
		if(parser) parser->caret--;
	}
	while(input[0]==py_split && input[1]==py_split)
	{
		input++;
		len--;
		if(parser) parser->caret--;
	}
	if(input[0]==py_split)
	{
		res=&py_split_item;
	}
	else
	{
		int count;
		int out[6];
		int i;
		count=py_tree_get(&py_index,input,out);
		if(count<=0) return 0;
		for(i=count-1;i>=1;i--)
		{
			struct py_item *p=py_all+out[i];
			int last=input[p->len-1],next=input[p->len];
			if(!strcmp(p->quan+1,"ve"))			// 分词的时候我们不把nve之类的当作合法拼音
				continue;
			if(!next || next==py_split)
				break;
			/* 下一个字母是iuv就可以认为前面的切分已经出错了 */
			if(strchr("iuv",next))
				continue;
			if(!strcmp(p->quan+1,"iao") && !strncmp(input+p->len,"linpike",7))
			{
				i--;
				continue;
			}
			
			if(next=='g' && (!input[p->len+1] || !strchr("aeou",input[p->len+1])))
			{
				/* 避免这个切分错误 qingquangran（情趣盎然) */
				if(strstr(p->quan,"uan"))
					continue;
				if(strstr(p->quan,"ian"))	// biangde(比昂的)
				{
					if(p->quan[0]=='d')
						i--;
					continue;
				}
			}
			/* 如果两种切分不止差一个编码，则不用考虑例外情况，按最长匹配即可 */
			if(p->len-py_all[out[i-1]].len!=1)
				break;
			/* 在r后无韵母的情况下er优先结合 */
			if(last=='e' && next=='r')
			{
				if(!input[p->len+1] || !strchr("aeiou",input[p->len+1]))
					continue;
				if(!strncmp(input+p->len+1,"ai",2))
					continue;				
				if(!strncmp(input+p->len+1,"er",2) && (!input[p->len+3] || !strchr("aeiou",input[p->len+3])))
					continue;
			}
			if(last=='r' && !strncmp(input+p->len,"ong",3))
				continue;
			/* 在n后无韵母的情况下en优先结合 */
			if(last=='e' && next=='n' && (!input[p->len+1] || !strchr("aeiouv",input[p->len+1])))
				continue;
			if(last=='e' && p->len>2 && input[p->len-2]=='u')
			{
				// 俄罗斯词频明显比螺丝高
				if(!strncmp(input+p->len,"luosi",5))
					continue;
			}
			
			/* gn优先和后面的韵母结合 */
			if((last=='g' && strchr("aeou",next)) || (last=='n' && strchr("aeiouv",next)))
			{
				/* 添加一些常用的例外 */
				if(next=='e' && input[p->len+1]=='r')
				{
					int nnext=input[p->len+2];
					if(!nnext || !strchr("aeiou",nnext))
						break;
					if(input[p->len+2]=='a')
					{
						if(input[p->len+2]!='n' && input[p->len+2]!='o')
							break;
					}
					if(!strncmp(input+p->len+2,"er",2) && (!input[p->len+4] || !strchr("aeiou",input[p->len+4])))
						break;
				}
				if(next=='e' && last=='g' && p->len>=4 && !strncmp(input+p->len-4,"ying",4))
				{
					if(!strncmp(input+p->len,"eluosi",6))
					{
						break;
					}
				}
				if(next=='e' && last=='g' && p->len>=4 && !strncmp(input+p->len-3,"ang",3))
				{
					if(!strncmp(input+p->len,"eluosi",6))
					{
						break;
					}
				}
				if(last=='g' && next=='a' && input[p->len+1]=='\0')	// 末字a比ga常见的多
					break;
				if((last=='n' || last=='g') && next=='o')
				{
					if(strncmp(input+p->len,"ou",2) && strncmp(input+p->len,"ong",2))
						break;
				}
				if(!strchr("ivu",next) || strncmp(input+p->len,"on",2))
				{
					if(!strncmp(input,"dian",4))		//这里dia音很罕见
						break;
					if(!strncmp(input,"deng",4))			//den音很罕见
						break;
				}
				if(!strncmp(input+p->len,"anquan",6))
				{
					// nanquan不成词，anquan安全很常见
					break;
				}
				if(!strncmp(input+p->len,"aolinpike",9))
				{
					// 奥林匹克
					break;
				}
				if(!strncmp(input+p->len,"alabo",5))
				{
					// labo 不成词，alabo是词
					break;
				}
				if(!strcmp(input+p->len,"aoyun") || !strncmp(input+p->len,"aoyunhui",8))
				{
					break;
				}
				continue;
			}
			break;
		}
		res=py_all+out[i];
	}
	if(res)
	{
		if(parser)
		{
			parser->caret-=res->len;
			if(parser->count<PY_MAX_TOKEN)
				parser->token[parser->count++]=res;
			if(parser->caret<0 && len>0 && parser->count<PY_MAX_TOKEN)
			{
				parser->token[parser->count++]=&py_caret;
				parser->caret=0x7fff;
			}
		}
		len-=res->len;
		input+=res->len;
	}
	if(res && len>0)
		return py_parse_r(parser,input,len);
	else
		return res?1:0;
}

#else
static int py_parse_r(PY_PARSER *parser,const char *input,int len)
{
	struct py_item it,*res;
	
	while(input[0]==' ')
	{
		input++;
		len--;
	}
	while(input[0]==py_split && input[1]==py_split)
	{
		input++;
		len--;
	}
	it.len=MIN(len,MAX_PYLEN);
	it.quan=input;
	do{
		//res=bsearch(&it,py_all,PY_COUNT,
		//		sizeof(struct py_item),item_cmpr);
		int left=l_bsearch_left(&it,py_all,PY_COUNT,sizeof(struct py_item),item_cmpr);
		if(left<PY_COUNT && !item_cmpr(&it,py_all+left))
		{
			res=py_all+left;
			if(left+1<PY_COUNT && (res->val&0xff)==0 && !item_cmpr(&it,res+1))
				res++;
		}
		else
		{
			res=NULL;
		}
		if(res && it.len>1 && input[it.len]!=0)
		{
			int last=input[it.len-1],next=input[it.len];
			/* 在r后无韵母的情况下er优先结合 */
			if(last=='e' && next=='r' && !strchr("aeiou",input[it.len+1]))
				res=0;
			if(last=='e' && next=='n' && !strchr("aeiouv",input[it.len+1]))
				res=0;
			if(res) while((last=='g' && strchr("aeou",next)) ||
			   (last=='n' && strchr("aeiouv",next)))
			{
				/* 添加一些常用的例外 */
				if(next=='e' && input[it.len+1]=='r' && !strchr("aeiou",input[it.len+1]))
					break; 
				if(next=='a' && input[it.len+1]=='\0')	// 末字a比ga常见的多
					break;
				if(!strncmp(input,"dian",4))		//这里dia音很罕见
					break;
				it.len--;
				if(bsearch(&it,py_all,PY_COUNT,sizeof(struct py_item),item_cmpr) &&
						py_parse_r(0,input+it.len,len-it.len)>0)
				{
					res=0;
				}
				it.len++;
				break;
			}
			if(res && next && strchr("iuv",next))
				res=0;
		}
		if(!res) it.len--;
	}while(!res && it.len>0);
	if(res)
	{
		len-=it.len;
		input+=it.len;
		if(parser)
		{
			parser->caret-=it.len;
			if(parser->count<PY_MAX_TOKEN)
				parser->token[parser->count++]=res;
			if(parser->caret<0 && len>0 && parser->count<PY_MAX_TOKEN)
			{
				parser->token[parser->count++]=&py_caret;
				parser->caret=0x7fff;
			}
		}
		if(len>0)
			return py_parse_r(parser,input,len);
	}
	else if(!res && parser && len>1)
	{
		len--;
		input++;
		return py_parse_r(parser,input,len);
	}
	return res?1:0;
}
#endif

int py_parse_string(const char *input,py_item_t *token,int caret)
{
	if(py_type==0)
	{
		PY_PARSER parser;
		
		parser.token=token;
		parser.count=0;
		parser.caret=(caret>=0)?caret:strlen(input);

		py_parse_r(&parser,input,strlen(input));

		return parser.count;
	}
	else if(py_type==1)
	{
		int i,count;
		struct py_item it,*p,**pp;
		char *s;
		for(i=0,count=0;input[i]!=0;)
		{
			if(input[i]==' ')
			{
				i++;
				continue;
			}
			if(input[i+1])
			{
				p=&it;it.val=input[i]<<8|input[i+1];
				pp=bsearch(&p,sp_index,PY_COUNT,sizeof(struct py_item*),sp_cmpr);
				if(pp)
				{
					p=*pp;
					s=(char*)&token[count];
					strncpy(s,input+i,2);
					s[2]=0;
					i+=2;
					count++;
					continue;
				}
			}
			p=&it;
			it.val=input[i]<<8;
			pp=bsearch(&p,sp_index,PY_COUNT,sizeof(struct py_item*),sp_cmpr);
			if(pp)
			{
				p=*pp;
				s=(char*)&token[count];
				s[0]=input[0];s[1]=0;
				i++;
				count++;
				continue;
			}
			else
			{
				break;
			}
		}
		return count;
	}
	else if(py_type==2 && py_split>1 && py_split<4)
	{
		int len=strlen(input);
		int i,count;
		for(i=0,count=0;i<len;)
		{
			char *p=(char*)&token[count];
			if(input[i]==' ')
			{
				i++;
				continue;
			}
			if(input[i+1]==' ' || input[i+1]==0)
			{
				p[0]=input[i];
				p[1]=0;
				i++;
			}
			else if(py_split==3 && (input[i+2]==' ' || input[i+2]==0))
			{
				p[0]=input[i];p[1]=input[i+1];p[2]=0;
				i+=2;
			}
			else
			{
				strncpy(p,input+i,py_split);
				p[py_split]=0;
				i+=py_split;
			}
			count++;
		}
		return count;
	}
	else
	{
		return -1;
	}
}

int py_parse_sp_simple(const char *input,py_item_t *token)
{
	int i,pos,c;
	for(i=pos=0;(c=input[i])!=0;i++)
	{
		int val;
		int j;
		if(c==' ') continue;
		val=PY_VAL(c,0);
		for(j=0;j<L_ARRAY_SIZE(py_all);j++)
		{
			if(py_all[j].val==val)
			{
				token[pos++]=py_all+j;
				break;
			}
		}
		if(j==L_ARRAY_SIZE(py_all))
		{
			return -1;
		}
	}
	return pos;
}

int py_string_step(char *input,int caret,uint8_t step[],int max)
{
	if(py_split<10)
	{
		memset(step,py_split,max);
	}
	else
	{
		py_item_t token[PY_MAX_TOKEN];
		int i,count,pos;
		char temp=input[caret];
		input[caret]=0;
		count=py_parse_string(input,token,caret);
		memset(step,0,max);
		for(i=0,pos=0;i<count;i++)
		{
			if(token[i]==&py_split_item)
			{
				step[pos]+=token[i]->len;
				continue;
			}
			if(!token[i]->len) continue;
			step[pos]+=token[i]->len;
			pos++;
		}
		input[caret]=temp;
	}
	return 0;
}

int py_build_string(char *out,py_item_t *token,int count)
{
	if(py_type==0)
	{
		int i,pos;
		
		for(pos=0,i=0;i<count;i++)
		{
			if(token[i]==&py_caret)
				continue;
			memcpy(out+pos,token[i]->quan,token[i]->len);
			pos+=token[i]->len;
			if(i+1<count &&	token[i]->val && token[i+1]->val)
			{
				out[pos++]=' ';
			}
		}
		out[pos]=0;
		return pos;
	}
	else
	{
		int i,pos;
		for(pos=0,i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			int len=strlen(p);
			memcpy(out+pos,p,len);
			pos+=len;
		}
		out[pos]=0;
		return pos;
	}
}

int py_get_space_pos(py_item_t *token,int count,int space)
{
	int i,pos;
	if(space<=0) return 0;
	if(py_type==0)
	{
		for(pos=0,i=0;i<count;i++)
		{
			if(token[i]==&py_caret)
				continue;
			pos+=token[i]->len;
			if(pos==space)
				return i+1;
			if(pos>space)
				return 0;
		}
	}
	else
	{
		for(pos=0,i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			int len=strlen(p);
			pos+=len;
			if(pos==space)
				return i+1;
			if(pos>space)
				return 0;
		}
	}
	return 0;
}

int py_build_sp_string(char *out,py_item_t *token,int count)
{
	int i,pos;
	int simple=0;
	
	for(i=0;i<count-1;i++)
	{
		if(token[i]==&py_caret || token[i]==&py_split_item)
			continue;
		if(!(token[i]->val&0xff))
		{
			simple=1;
			break;
		}
	}
	for(pos=0,i=0;i<count;i++)
	{
		if(token[i]==&py_caret || token[i]==&py_split_item)
			continue;
		if(simple && token[i]->len==1)
		{
			out[pos++]=token[i]->quan[0];
			out[pos++]='\'';
		}
		else
		{
			out[pos++]=(char)((token[i]->val>>8)&0xff);
			out[pos++]=(char)((token[i]->val>>0)&0xff);
			if(out[pos-1]==0) out[pos-1]=py_split;
		}
	}
	out[pos]=0;
	return pos;
}

int py_remove_split(py_item_t *token,int count)
{
	if(py_type==0)
	{
		py_item_t temp[count];
		int i,pos;
		
		for(pos=0,i=0;i<count;i++)
		{
			if(token[i]->val==0)
				continue;
			temp[pos++]=token[i];
		}
		memcpy(token,temp,pos*sizeof(py_item_t));
		return pos;
	}
	else
	{
		return count;
	}
}

int py_caret_to_pos(py_item_t *token,int count,int caret)
{
	int i,pos,len;
	
	if(caret==-1)
		caret=0x7ffff;
	
	for(pos=0,i=0;i<count && caret>0;i++)
	{
		if(token[i]==&py_caret)
			continue;
		len=token[i]->len;
		len=MIN(caret,len);
		pos+=len;
		if(i+1<count &&	token[i]->val && token[i+1]->val)
			pos++;
	}
	return pos;
}

int py_prepare_string(char *to,const char *from,int *caret)
{
	int i,count;
	while(from[0]==py_split || from[0]==' ')
	{
		from++;
		if(caret && *caret>0)
			*caret=*caret-1;
	}
	for(i=0,count=0;*from!=0;i++)
	{
		if(*from==' ' || (from[0]==py_split && from[1]==py_split))
		{
			from++;
			if(caret && *caret>i)
				*caret=*caret-1;
			continue;
		}
		*to++=*from++;
		count++;
	}
	*to=0;
	return count;
}

int py_conv_from_sp(const char *in,char *out,int size,int split)
{
	int i=0,pos=0;
	while(in[i]!=0 && pos+8<size)
	{
		struct py_item it,*p,**pp;
		if(in[i]==' ')
		{
			if(pos>0 && out[pos-1]==split)
				out[pos-1]=' ';
			else
				out[pos++]=' ';
			i++;
			continue;
		}
		if(in[i] && in[i+1])
		{
			p=&it;
			it.val=in[i]<<8|in[i+1];
			pp=bsearch(&p,sp_index,PY_COUNT,sizeof(struct py_item*),sp_cmpr);
			if(pp)
			{
				p=*pp;
				if(pos+p->len+1>size)
					break;
				memcpy(out+pos,p->quan,p->len);
				pos+=p->len;
				i+=2;
				if(in[i]!=0 && split)
				{
					out[pos++]=split;
				}
				continue;
			}
		}
		p=&it;
		it.val=in[i]<<8;
		pp=bsearch(&p,sp_index,PY_COUNT,sizeof(struct py_item*),sp_cmpr);
		if(pp)
		{
			p=*pp;
			if(pos+p->len+1>size)
					break;
			memcpy(out+pos,p->quan,p->len);
			pos+=p->len;
			i++;
			if(in[i-1]=='a' || in[i-1]=='e' || in[i-1]=='o')
			{
				// 以这个形式表示双拼中单个出现的aeo，现在能判断句尾出现的
				out[pos++]=split;
				//out[pos++]=' ';
			}
			else if(in[i]!=0 && split)
			{
				out[pos++]=split;
			}
		}
		else
		{
			break;
		}
	}
	out[pos]=0;
	//printf("%s\n",out);
	return pos;
}

/* 输入双拼和全拼中的位置，得到在双拼中的位置 */
int py_pos_of_sp(const char *in,int pos)
{
	int i=0;
	while(pos>0 && in[i]!=0)
	{
		struct py_item it,*p,**pp;
		if(in[i]==' ')
		{
			pos--;
			i++;
			continue;
		}
		if(/*in[i] && */in[i+1])
		{
			p=&it;
			it.val=in[i]<<8|in[i+1];
			pp=bsearch(&p,sp_index,PY_COUNT,sizeof(struct py_item*),sp_cmpr);
			if(pp)
			{
				p=*pp;
				pos-=p->len;
				i+=2;
				continue;
			}
		}
		p=&it;
		it.val=in[i]<<8;
		pp=bsearch(&p,sp_index,PY_COUNT,sizeof(struct py_item*),sp_cmpr);
		if(pp)
		{
			p=*pp;
			pos-=p->len;
			i++;
		}
		else
		{
			break;
		}
	}
	return i;
}

int py_pos_of_qp(py_item_t *in,int pos)
{
	int i;
	int res;
	for(res=i=0;pos>0;i++)
	{
		if(in[i]==&py_caret)
			continue;
		if(in[i]==NULL)
			return -1;
		if(in[i]==&py_split_item)
		{
			res++;
			continue;
		}
		if(pos>=2)
		{
			res+=in[i]->len;
			pos-=2;
		}
		else
		{
			res+=in[i]->yun;
			pos--;
		}
	}
	return res;
}

int py_is_valid_input(int sp,int c,int pos)
{
	if(sp)
	{
		return (c>='a' && c<='z') || ((pos&0x01) && c==';' && sp_semicolon);
	}
	else
	{
		return c>='a' && c<='z';
	}
}

int py_is_valid_quanpin(const char *input)
{
	PY_PARSER parser;
	py_item_t token[PY_MAX_TOKEN];
	
	parser.token=token;
	parser.count=0;
	parser.caret=strlen(input);

	int ret=py_parse_r(&parser,input,strlen(input));
	if(parser.count==0 || ret==0)
		return 0;
	int i;
	for(i=0;i<parser.count;i++)
	{
		py_item_t it=token[i];
		if((it->val&0xff)==0)
			return 0;
	}
	return 1;
}

int py_is_valid_sp(const char *input)
{
	char temp[128];
	int len=py_conv_from_sp(input,temp,sizeof(temp),'\'');
	if(len<=0)
		return 0;
	int ret=py_is_valid_quanpin(temp);
	return ret;
}

int py_sp_has_semi(void)
{
	return sp_semicolon;
}

static inline const char *hz_goto_next(const char *s,int *hz)
{
	if(gb_is_gbk((const uint8_t*)s))
	{
		*hz=GBK_MAKE_CODE(s[0],s[1]);
		return s+2;
	}
	else if(gb_is_gb18030_ext((const uint8_t*)s))
	{
		*hz=0;
		return s+4;
	}
	return 0;
}

static inline int hz_get_first_code(int hz)
{
	static const uint16_t range[]={
			/*0xb0a1,*/
			0xb0c5,0xb2c1,0xb4ee,0xb6ea,0xb7a2,
			0xb8c1,0xb9fe,0xbbf7,0xbbf7,0xbfa6,
			0xc0ac,0xc2e8,0xc4c3,0xc5b6,0xc5be,
			0xc6da,0xc8bb,0xc8f6,0xcbfa,0xcdda,
			0xcdda,0xcdda,0xcef4,0xd1b9,0xd4d1
			/*,0xd7f9*/
	};
	int i;
	if(hz<0xb0a1 || hz>0xd7f9)
	{
		static const struct{
			uint16_t hz;
			char code;
		}pair[]={
			{0xdece,'g'},
		};
		for(i=0;i<1;i++)
		{
			if(pair[i].hz==hz)
				return pair[i].code;
		}
		return 0;
	}
	for(i=0;i<25 && hz>=range[i];i++);
	return 'a'+i;
}

int py_conv_to_sp(const char *s,const char *zi,char *out)
{
	int hz;
	int py[6];
	int count;
	struct py_item *it;
	
	/* 跳过第一个字 */
	zi=hz_goto_next(zi,&hz);
	if(!zi)
	{
		return -1;
	}
	while(s[0]!=0 && zi!=NULL)
	{
		const char *prev=zi;
		zi=hz_goto_next(zi,&hz);
		count=py_tree_get(&py_index,s,py);
		if(count<=0)
		{
			return -1;	// 分析错误
		}
		if(count==1)			// 不需要选择
		{
			it=py_all+py[0];
		}
		else if(!zi || !hz)		// 没有更多信息，简单最长匹配
		{
			it=py_all+py[count-1];
		}
		else if(count==2 && (py_all[py[0]].val&0xff)==0)	// 只有一个选项
		{
			it=py_all+py[1];
		}
		else
		{
			int code=hz_get_first_code(hz);
			if(code!=0)			// 利用下一个字的编码信息
			{
				int index=count;
				while(--index>=0)
				{
					it=py_all+py[index];
					if((it->val & 0xff)==0)
						break;
					if(s[it->len]!=code)
						continue;
					int ret;
					out[0]=(char)(it->val>>8);
					out[1]=(char)(it->val);
					ret=py_conv_to_sp(s+it->len,prev,out+2);
					if(ret==0) return 0;
				}
				it=py_all+py[count-1];
			}
			else 				// 最长匹配
			{
				int index=count;
				while(--index>=0)
				{
					int ret;
					it=py_all+py[index];
					if((it->val & 0xff)==0)
						break;
					out[0]=(char)(it->val>>8);
					out[1]=(char)(it->val);
					ret=py_conv_to_sp(s+it->len,prev,out+2);
					if(ret==0) return 0;
				}
				return -1;
			}
		}
		//printf("%s\n",it->quan);
		s+=it->len;
		/* 这里假设不出现简拼的情况 */
		*out++=(char)(it->val>>8);
		*out++=(char)(it->val);
		if(!zi && (s[0]==0 || s[0]==' ' || s[0]=='\t'))
		{
			*out=0;
			return 0;
		}
	}
	return -1;
}

#ifdef TOOLS_PARSE
int main(int arc,char *arg[])
{
	py_init(0,0);
	if(arc==2)
	{
		py_item_t token[PY_MAX_TOKEN];
		int count;
		char display[MAX_DISPLAY];
		count=py_parse_string(arg[1],token,0);
		py_build_string(display,token,count);
		printf("%s\n",display);
		py_prepare_string(display,display,0);
		count=py_parse_string(display,token,0);
		py_build_string(display,token,count);
		printf("%s\n",display);
		py_conv_from_sp(arg[1],display,MAX_DISPLAY,' ');
		printf("%s\n",display);
	}
	else if(arc==3)
	{
		char temp[64];
		char out[32];
		int ret;
		l_utf8_to_gb(arg[2],temp,sizeof(temp));
		ret=py_conv_to_sp(arg[1],temp,out);
		if(ret==0)
			printf("%s\n",out);
	}
	return 0;
}
#endif

#ifdef TOOLS_SP
int main(int arc,char *arg[])
{
	char temp[64];
	py_init('\'',"zrm");
	py_conv_from_sp("lt",temp,sizeof(temp),' ');
	printf("%s\n",temp);
	return 0;
}
#endif
