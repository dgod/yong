#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>

#include "gbk.h"
#include "mb.h"
#include "pinyin.h"
#include "llib.h"
#include "pyzip.h"

#define PYUNZIP_BEFORE_CMP	1

#if !PYUNZIP_BEFORE_CMP
#define LEARN_MAGIC 0x44332216
#else
#define LEARN_MAGIC 0x44332217
#endif

#define PSEARCH_PHRASE		0x00
#define PSEARCH_SENTENCE	0x01
#define PSEARCH_ALL			0x02
#define PSEARCH_PART		0x03

#define PSEARCH_BEGIN		0x01
#define PSEARCH_MID			0x02
#define PSEARCH_END			0x04
#define PSEARCH_ANY_POS		0x07

typedef struct{
	uint32_t magic;				/* 文件头标识 */
	uint32_t zi_offset;			/* 单字表偏移量 */
	uint32_t zi_count;			/* 字的数量 */
	uint32_t ci_freq;			/* 所有词对数频率 */
	uint32_t ci_offset;			/* 词的偏移量 */
	uint32_t ci_count;			/* 词的数量 */
	uint32_t it_offset;			/* 语料的偏移量 */
	uint32_t it_count;			/* 语料的数量 */
	uint32_t it_phrase;			/* 真正的语料的数量 */
	uint32_t raw_offset;		/* 数据偏移 */
	uint32_t raw_size;			/* 数据大小 */
}LEARN_HEADER;

typedef struct{
	uint32_t code;				/* 编码 */
	uint32_t cand;				/* 词 */
	union{
		uint8_t lf[4];
		struct{
			uint8_t zero;		/* 保留为0 */
			uint8_t nseg;		/* 编码是几元组 */
			uint8_t phrase;		/* 语料的第一段和最后一段的长度，以字为单位，各占3位 */
			uint8_t freq;		/* 设置的频率 */
		};
	};
}LEARN_ITEM;

#ifdef TOOLS_LEARN
typedef struct{
	void *next;
	uint32_t freq;
	uint32_t part[3];
	char ci[16];
}CI_FREQ_ITEM;
#endif

typedef struct{
	char ci[12];
	uint32_t freq;
}CI_FREQ_ITEM_S;





static int ci_freq_cmp_s(const CI_FREQ_ITEM_S *v1,const CI_FREQ_ITEM_S *v2)
{
	/*int64_t ret=v1->val-v2->val;
	if(ret==0) return 0;
	else if(ret<0) return -1;
	return 1;*/
	return strcmp(v1->ci,v2->ci);
}

typedef struct{
	struct y_mb *mb;				// 对应的码表
	uint32_t all_freq;				// 词频之和，unigram计算时用
	uint32_t hz_freq[GB_HZ_SIZE];	// 字频表
	int ci_count;					// 词频数量
	CI_FREQ_ITEM_S *ci_flat;		// 使用线性数组的词频表
	LHashTable *ci_index;			// 所有词的哈希索引，只在创建语料库时用
	int it_count;					// 总的于应料数量
	int it_phrase;					// 三元组语料数量
	LEARN_ITEM *it_data;			// 语料指针
	int raw_size;					// 字词等数据大大小
	uint8_t *raw_data;				// 字词数据
	LEARN_ITEM key;
	void *user;
#ifdef TOOLS_LEARN
	LHashTable *ci_freq;			// 词频表
	CODE_CACHE *code_cache;
#endif
}LEARN_DATA;

static LEARN_DATA *l_predict_data;
static int l_force_mmseg;
int l_predict_simple;
int l_predict_sp;
extern int PySwitch;

void y_mb_learn_free(LEARN_DATA *data)
{
	if(!data) data=l_predict_data;
	if(!data) return;
#ifdef TOOLS_LEARN
	l_hash_table_free(data->ci_freq,l_free);
#endif
	l_free(data->ci_flat);
	l_hash_table_free(data->ci_index,l_free);
#ifdef TOOLS_LEARN
	code_cache_free(data->code_cache);
#endif
	free(data->it_data);
	free(data->raw_data);
	free(data);
	l_predict_data=NULL;
}

LEARN_DATA *y_mb_learn_load(struct y_mb *mb,char *in)
{
	LEARN_HEADER hdr;
	FILE *fp;
	LEARN_DATA *data;
	int ret;
	int i;
	
	fp=y_mb_open_file(in,"rb");
	if(!fp) return NULL;
	ret=fread(&hdr,1,sizeof(hdr),fp);
	if(ret!=sizeof(hdr) || hdr.magic!=LEARN_MAGIC)
	{
		fclose(fp);
		return NULL;
	}
	data=calloc(1,sizeof(LEARN_DATA));
	data->mb=mb;
	data->all_freq=hdr.ci_freq;
	
	fseek(fp,hdr.zi_offset,SEEK_SET);
	fread(data->hz_freq,sizeof(uint32_t),hdr.zi_count,fp);

	data->ci_count=hdr.ci_count;
	data->ci_flat=l_cnew0(data->ci_count,CI_FREQ_ITEM_S);
	fseek(fp,hdr.ci_offset,SEEK_SET);
	for(i=0;i<hdr.ci_count;i++)
	{
		CI_FREQ_ITEM_S *item=data->ci_flat+i;
		uint8_t len;
		fread(&len,1,1,fp);
		fread(&item->freq,4,1,fp);
		fread(&item->ci,1,len,fp);
		item->ci[len]=0;
	}

	if(hdr.it_count>0)
	{
		data->it_count=hdr.it_count;
		data->it_phrase=hdr.it_phrase;
		data->it_data=malloc(data->it_count*sizeof(LEARN_ITEM));
		if(!data->it_data)
		{
			y_mb_learn_free(data);
			return NULL;
		}
		fseek(fp,hdr.it_offset,SEEK_SET);
		fread(data->it_data,sizeof(LEARN_ITEM),data->it_count,fp);
	}
	
	if(hdr.raw_size>0)
	{
		data->raw_size=hdr.raw_size;
		data->raw_data=malloc(hdr.raw_size+256);
		if(!data->raw_data)
		{
			y_mb_learn_free(data);
			return NULL;
		}
		fseek(fp,hdr.raw_offset,SEEK_SET);
		fread(data->raw_data,1,data->raw_size,fp);
		data->key.code=data->raw_size;
	}
	
	fclose(fp);
	l_predict_data=data;
	
	if(data->ci_count<1000)
		l_force_mmseg=1;
	return data;
}

typedef struct{
	struct y_mb *mb;
	short space;
	short space2;
	py_item_t input[PY_MAX_TOKEN];
	int count;
	char *cand;
	int setence_begin;		// input是否一个句子的开始
	int setence_end;		// input是否一个句子的结束
	char assist_begin;		// 句首辅助码，现在没有用到
	char assist_end;		// 句尾辅助码
	int mark_skip;			// 标记是否跳过第一个匹配的
	char last_zi[8];		// 在没有句尾辅助码的情况下的最后一个字
	struct y_mb_ci *codec[1024];
}MMSEG;

#if 0
void uprintf(const char *fmt,...)
{
	va_list ap;
	char gb[256];
	char utf8[256];
	va_start(ap,fmt);
	vsprintf(gb,fmt,ap);
	va_end(ap);
	l_gb_to_utf8(gb,utf8,256);
	printf("%s",utf8);
}
#endif

static int mmseg_exist(MMSEG *mm,py_item_t *input,int count)
{
	char code[Y_MB_KEY_SIZE+1];
	struct y_mb_ci *ret;
	int len;
#ifndef TOOLS_LEARN
	const int end=(mm->assist_end && input+count==mm->input+mm->count);		
#endif

	int pos;
	pos=((input-mm->input)<<3)|count;
	ret=mm->codec[pos];
	if(ret!=(struct y_mb_ci*)-1)
		return ret?1:0;

	py_build_string(code,input,count);
	len=py_prepare_string(code,code,0);
	if(mm->space && count>1)
	{
		(void)len;
		int start=input-mm->input;
		if(start<mm->space2 && start+count>mm->space2)
		{
			// 空格分开的编码不能合起来用
			mm->codec[pos]=NULL;
			return 0;
		}
	}

#ifdef TOOLS_LEARN
	ret=code_cache_test(l_predict_data->code_cache,code);
	mm->codec[pos]=ret;
	return ret?1:0;

#else
	if(l_predict_sp)
	{
		trie_tree_t *t=mm->mb->trie;
		trie_node_t *n;
		py_build_sp_string(code,input,count);
		len=py_prepare_string(code,code,0);
		n=trie_tree_get_leaf(t,code,len);
		
		// 找不到拼音时，简单的选择单子简拼
		if((!n || !n->data) && len==2 && code[1]=='\'')
		{
			code[1]=0;
			n=trie_tree_get_path(t,code,1);
			if(n!=NULL)
			{
				if(n->leaf)
				{
					n=trie_node_get_leaf(t,n);
				}
				else
				{
					n=trie_node_get_child(t,n);
					for(;n!=NULL;n=trie_node_get_brother(t,n))
					{
						if(n->leaf)
						{
							n=trie_node_get_leaf(t,n);
							break;
						}
					}
				}
			}					
		}
		if(!n || !n->data)
		{			
			ret=NULL;
		}
		else
		{
			ret=((struct y_mb_ci*)n->data);
			for(;ret!=NULL;ret=ret->next)
			{
				char *s;
				if(ret->del) continue;
				if(ret->len!=count*2) continue;
				s=y_mb_ci_string(ret);
				if(!(s[0]&0x80))
					continue;
				if(end && !y_mb_assist_test(mm->mb,ret,mm->assist_end,0,1))
				{
					mm->mark_skip=1;
					continue;
				}
				break;
			}
		}
	}
	else
	{
		int dlen=-1;
		if(l_predict_sp || (mm->mb->split>=2 && mm->mb->split<=4))
			dlen=count;
		ret=y_mb_code_exist(mm->mb,code,len,dlen);
		if(ret==NULL && count==1)
		{
			struct y_mb_context ctx;
			y_mb_push_context(mm->mb,&ctx);
			y_mb_set_zi(mm->mb,1);
			// FIXME: why match=0? it should be 1.
			mm->mb->ctx.result_match=0;
			if(y_mb_set(mm->mb,code,len,0)>0)
			{
				ret=mm->mb->ctx.result_first->phrase;
			}
			y_mb_pop_context(mm->mb,&ctx);
		}
		for(;ret!=NULL;ret=ret->next)
		{
			if(ret->del) continue;
			if(ret->zi && ret->len!=2)
				continue;
			if(!ret->zi && ret->ext)
				continue;
			if(ret->len!=count*2)
				continue;
			if(end && !y_mb_assist_test(mm->mb,ret,mm->assist_end,0,1))
			{
				mm->mark_skip=1;
				continue;
			}
			break;
		}
	}
	mm->codec[pos]=ret;
	return ret?1:0;
#endif
}

/*
 句尾辅助码测试
 womfuiuif
 nimfjdd
 dajwxdp
*/
static int mmseg_logcf(MMSEG *mm,int pos,int len,struct y_mb_ci **ci)
{
	char code[Y_MB_KEY_SIZE+1];
	int max=0,max2=0;
	int ext=0;
	int i;
	struct y_mb_ci /**first,*/*list,*tmp=0,*tmp2=0;
	int cpos;
	int end=((pos+len)==mm->count);
	
	cpos=(pos<<3)|len;
	list=mm->codec[cpos];
	if(!list) goto out;
	/*first=*/list=(struct y_mb_ci*)(((uintptr_t)list)&~0x03);
	
	// code只在单字的时候使用
	code[0]=0;

	if(!l_predict_data)
	{
		tmp=list;
		/*if(tmp && tmp->zi && mm->mb->simple)
		{
			// 单字有简码时，可以认为简码的字频比较高
			for(list=list->next;list!=NULL;list=list->next)
			{
				if(!tmp->simp && list->simp)
				{
					tmp=list;
					break;
				}
			}
		}*/
		for(i=0,list=list->next;i<6 && list!=NULL;list=list->next)
		{
			if(l_predict_sp && list->len!=2*len)
				continue;
			if(mm->mb->split>=2 && mm->mb->split<=4 && list->len!=2*len)
				continue;
			if(end && mm->assist_end && !y_mb_assist_test(mm->mb,list,mm->assist_end,0,1))
				continue;
			if(!tmp2)
				tmp2=list;
			if(tmp->zi && mm->mb->simple && list->simp)
			{
				tmp=list;
				tmp2=tmp;
				break;
			}
		}
		goto out;
	}
	for(i=0;i<6 && list!=NULL;list=list->next)
	{
		uint32_t freq=0;

		if(list->del)
			continue;
		if(l_predict_sp && list->len!=2*len)
			continue;
		if(!list->zi && list->ext)
			continue;
		if(mm->mb->split>=2 && mm->mb->split<=4 && list->len!=2*len)
		{
			continue;
		}
		if(end && mm->assist_end && !y_mb_assist_test(mm->mb,list,mm->assist_end,0,1))
		{
			continue;
		}
		i++;
		if(len>4)
		{
		}
		else if(list->zi)
		{
			const char *s=(char*)&list->data;
			if(code[0]==0)
			{
				py_build_string(code,mm->input+pos,len);
				py_prepare_string(code,code,0);
			}
			if(y_mb_is_good_code(mm->mb,code,s))
			{			
				if(gb_is_hz((uint8_t*)s))
					freq=l_predict_data->hz_freq[GB_HZ_OFFSET(s)];
				if(pos==0 && mm->setence_begin)
					freq=freq&1023;
				else if(end && mm->setence_end)
					freq=(freq>>21)&1023;
				else
					freq=(freq>>10)&2047;
			}
		}
		else
		{
			const char *s=y_mb_ci_string(list);
			freq=0;
			if(len<=4)
			{
				CI_FREQ_ITEM_S *found;
				found=bsearch(s,l_predict_data->ci_flat,
						l_predict_data->ci_count,
						sizeof(CI_FREQ_ITEM_S),
						(LCmpFunc)ci_freq_cmp_s);
				if(found)
				{
					freq=found->freq;
					if(pos==0 && mm->setence_begin)
						freq=freq&1023;
					else if(end && mm->setence_end)
						freq=(freq>>21)&1023;
					else
						freq=(freq>>10)&2047;
				}
			}
		}

		/* 没有辅助码时按词频，有的时候按顺序 */
		if(mm->assist_end && end)
		{
			if(!tmp)
			{
				max=freq;
				ext=list->ext;
				tmp=list;
			}
			else if(!tmp2)
			{
				max2=freq;
				tmp2=list;
			}
		}
		else
		{
			if(!tmp || freq>max || (ext && !list->ext))
			{
				max=freq;
				ext=list->ext;
				tmp=list;
			}
		}
	}
	// 发生句尾辅助时，优先用第二个匹配的词
out:
	if(mm->assist_end && end && tmp2 && len>=1)
	{
		const char *s=y_mb_ci_string(tmp);
		// 只有在首选的tmp的末字相同才换成第二候选
		//printf("%s %s\n",s,mm->last_zi);
		int last_len=(int)strlen(mm->last_zi);
		int cmp_len=(tmp->len>=last_len)?last_len:tmp->len;
		if(!strcmp(s+tmp->len-cmp_len,mm->last_zi+last_len-cmp_len))
		{
			tmp=tmp2;
			max=max2;
		}
	}
	
	//printf("%s %d\n",y_mb_ci_string(tmp),max);
	if(ci) *ci=tmp;
	if(l_predict_data)
		max-=l_predict_data->all_freq;
	return max;
}

static int ci_freq_get(const char *s)
{
	CI_FREQ_ITEM_S *item;
	if(!l_predict_data) return 0;
	item=bsearch(s,l_predict_data->ci_flat,
			l_predict_data->ci_count,
			sizeof(CI_FREQ_ITEM_S),(LCmpFunc)ci_freq_cmp_s);
	if(!item) return 0;
	return item->freq;
}

#if !PYUNZIP_BEFORE_CMP
static int predict_compar_phrase(const LEARN_ITEM *p1, const LEARN_ITEM *p2)
{
	char *s1,*s2;
	s1=(char*)l_predict_data->raw_data+p1->code;
	s2=(char*)l_predict_data->raw_data+p2->code;
	return strcmp(s1,s2);
}

static int predict_compar_part(const LEARN_ITEM *p1, const LEARN_ITEM *p2)
{
	char *s1,*s2;
	int n;
	s1=(char*)l_predict_data->raw_data+p1->code;
	s2=(char*)l_predict_data->raw_data+p2->code;
	if(!p1->cand)
	{
		n=strlen(s1);
	}
	else
	{
		n=strlen(s2);
	}
	return strncmp(s1,s2,n);
}
#else
static int predict_compar_phrase(const LEARN_ITEM *p1, const LEARN_ITEM *p2)
{
	char temp[256];
	char *s1,*s2;
	if(!p1->cand)
	{
		s1=(char*)l_predict_data->raw_data+p1->code;
		cp_unzip((char*)l_predict_data->raw_data+p2->code,temp);
		s2=temp;
	}
	else
	{
		s2=(char*)l_predict_data->raw_data+p2->code;
		cp_unzip((char*)l_predict_data->raw_data+p1->code,temp);
		s1=temp;
	}
	return strcmp(s1,s2);
}

static int predict_compar_part(const LEARN_ITEM *p1, const LEARN_ITEM *p2)
{
	char temp[256];
	char *s1,*s2;
	int n;
	if(!p1->cand)
	{
		s1=(char*)l_predict_data->raw_data+p1->code;
		cp_unzip((char*)l_predict_data->raw_data+p2->code,temp);
		s2=temp;
		n=strlen(s1);
	}
	else
	{
		s2=(char*)l_predict_data->raw_data+p2->code;
		cp_unzip((char*)l_predict_data->raw_data+p1->code,temp);
		s1=temp;
		n=strlen(s2);
	}
	return strncmp(s1,s2,n);
}
#endif

static int predict_compar_part_cand(const LEARN_ITEM *p1, const LEARN_ITEM *p2)
{
	char *s1,*s2;
	int n;
	s1=(char*)l_predict_data->raw_data+p1->cand;
	s2=(char*)l_predict_data->raw_data+p2->cand;
	if(!p1->cand)
	{
		n=strlen(s1);
	}
	else
	{
		n=strlen(s2);
	}
	return strncmp(s1,s2,n);
}

static int predict_test_assist(MMSEG *mm,LEARN_ITEM *item,int end)
{
	char temp[MAX_CAND_LEN+1];
	int len;
	
	if(!mm || !mm->assist_end || !end)
		return 1;
	len=cz_unzip((const char*)(l_predict_data->raw_data+item->cand),temp,sizeof(temp));
	if(len<2) return 1;
	return y_mb_assist_test_hz(mm->mb,temp+len-2,mm->assist_end);
}

static LEARN_ITEM *predict_search(LEARN_DATA *data,MMSEG *mm,const char *code,int flag,int *count,int end)
{
	LEARN_ITEM *key=&data->key;
	LEARN_ITEM *item=NULL,*pnext=NULL;
	char *temp;
	if(!data)
		return NULL;
	temp=(char*)data->raw_data+key->code;
	key->cand=0;

#if !PYUNZIP_BEFORE_CMP
	cp_zip(code,temp);
#else
	if(temp!=code)
		strcpy(temp,code);
#endif

	{
		int i,num;
		i=l_bsearch_left(key,data->it_data,data->it_count,sizeof(LEARN_ITEM),
				(LCmpFunc)(flag==PSEARCH_PHRASE?predict_compar_phrase:predict_compar_part));
		for(num=0;i<data->it_count;i++)
		{
			LEARN_ITEM *p;
			p=data->it_data+i;
			if((flag==PSEARCH_PHRASE || flag==PSEARCH_SENTENCE) && predict_compar_phrase(p,key)!=0)
			{
				break;
			}
			else if(predict_compar_part(p,key)!=0)
				break;
			if(p->lf[1]<1 && flag==PSEARCH_PHRASE)
				continue;
			if(mm->space2 && (p->lf[2]>>3) && (p->lf[2]>>3)>mm->space2)
				continue;
			if(predict_test_assist(mm,p,end)==1)
			{
				num++;
				if(!item)
					item=data->it_data+i;
				else if(num==2 && mm!=NULL)
					pnext=data->it_data+i;
			}
		}
		if(count) *count=num;
	}
	if(mm && flag==PSEARCH_PHRASE && end && item && mm->assist_end)
	{
		mm->mark_skip=1;
		return pnext;
	}

	return item;
}

static int predict_copy(LEARN_DATA *data,char *dst,LEARN_ITEM *src,int count)
{
	const char *part=(const char*)data->raw_data+src->cand;
	return cz_unzip(part,dst,count);
}

#if 1
// 保存局部最优解
static uint32_t unigram_split[64][64];
static struct y_mb_ci *unigram_codec[1024];

// 获得字词频
static int unigram_logcf(MMSEG *mm,int pos,int len,struct y_mb_ci **ci)
{
	int ret;
	if(len>7 || !mmseg_exist(mm,mm->input+pos,len))
	{
		return 0;
	}
	else
	{
		ret=mmseg_logcf(mm,pos,len,ci);
	}
	if(l_predict_data)
	{
		return -ret;
	}
	else
	{
		// TODO: 没有语料库的时候怎么办
	}
	return ret;
}

// 维特比算法
static uint32_t unigram_best(MMSEG *mm,int b,int l)
{
	uint32_t res;
	int i;
	struct y_mb_ci *c;
	res=unigram_split[b][l];
	if(res!=0)
		return res;
	res=unigram_logcf(mm,b,l,&c);
	if(res>0 && c)
	{
		res=res<<6|l;
		unigram_codec[b<<3|l]=c;
		goto out;
		//if(l==1)
		//	goto out;
		//printf("R %d %d %d %d\n",b,l,res&0x3f,res>>6);
	}
	if(l==1)
	{
		//printf("unigram best can't found %d %d\n",b,l);
		return 0;
	}

	for(i=1;i<l;i++)
	{
		uint32_t res1,res2,temp;
		res1=unigram_best(mm,b,i);
		if(res1==0) return 0;
		res2=unigram_best(mm,b+i,l-i);
		if(res2==0) return 0;
		temp=(res1>>6)+(res2>>6);
		if(res==0 || temp<res>>6)
		{
			res=i|(temp<<6);
		}
	}
out:
	//printf("%d %d %d %d\n",b,l,res&0x3f,res>>6);
	unigram_split[b][l]=res;
	return res;
}

// 生成一个切分序列
static int unigram_build(int b,int l,uint8_t *out,int p)
{
	uint32_t res;
	int len;
	res=unigram_split[b][l];
	assert(res>0);
	len=(int)(res&0x3f);
	assert(len>0 && len<=l);
	if(len==l)
	{
		out[p++]=(int)l;
	}
	else
	{
		p=unigram_build(b,len,out,p);
		p=unigram_build(b+len,l-len,out,p);
	}
	return p;
}

// 输出汉字
static void unigram_output(MMSEG *mm,uint8_t *seq,int len,char *out)
{
	int i;
	int pos;
	//for(i=0;i<len;i++)printf("%d\n",seq[i]);
	for(i=0,pos=0;i<len;)
	{
		struct y_mb_ci *c;
#if 1
		int j;
		for(j=4;j>=2;j--)
		{
			int tlen=0,k;
			LEARN_ITEM *item,*key;
			char *predict,*temp;
			if(i+j>len)
			{
				continue;
			}
			key=&l_predict_data->key;
			temp=(char*)l_predict_data->raw_data+key->code;			
			for(k=0;k<j;k++) tlen+=seq[i+k];			
			py_build_string(temp,mm->input+pos,tlen);
			py_prepare_string(temp,temp,0);
			item=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,pos+tlen==mm->count);
			if(!item)
			{
				continue;
			}
			predict=mm->cand+strlen(mm->cand);
			predict_copy(l_predict_data,predict,item,-1);
			if((item->lf[2]&0x07)>0)
			{
				if((item->lf[2]&0x07)<seq[i+j-1])
				{
					int delta=seq[i+j-1]-(item->lf[2]&0x07);
					seq[i+j-1]-=delta;
					seq[i+j-2]+=delta;
				}
				else if((item->lf[2]&0x07)==seq[i+j-1]+1 && seq[i+j-2]>1)
				{
					// 只处理比默认分割多一个字，且上一个分隔不止一个字的情况
					seq[i+j-2]--;
					seq[i+j-1]++;
				}
			}
			if(i+j<len)
			{
				int tlen2=0;
				LEARN_ITEM *item2;
				for(k=0;k<j;k++) tlen2+=seq[i+1+k];
				py_build_string(temp,mm->input+pos+seq[i],tlen2);
				py_prepare_string(temp,temp,0);
				item2=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,i+j+1==len);
				// 后续语料优先级高且重合内容不一样，放弃在当前位置进行查找
				if(item2!=NULL && item2->lf[3]>item->lf[3])
				{
					predict_copy(l_predict_data,temp,item,-1);
					if(memcmp(predict+2*seq[i],temp,(tlen-seq[i])*2))
					{
						predict[0]=0;
						break;
					}
				}
			}
			
			i+=j;
			pos+=tlen;
			
			// 循环语料联想
			while(i<len)		// 没有到句尾
			{
				// 这里k表示j元组的后几个词
				for(k=j-1;k>=1;k--)
				{
					// base是k个词的编码数量，ext表示联想用到的额外词语的数量
					int base,t,ext=1;
					for(t=0,base=0;t<k;t++) base+=seq[i-k+t];
					// prefix是k个词的文本
					char *prefix=predict+strlen(predict)-2*base;
					py_build_string(temp,mm->input+pos-base,base+seq[i]);
					py_prepare_string(temp,temp,0);
					item=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,pos+seq[i]==mm->count);
					if(!item && i+1<len)
					{
						py_build_string(temp,mm->input+pos-base,base+seq[i]+seq[i+1]);
						py_prepare_string(temp,temp,0);
						item=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,pos+seq[i]+seq[i+1]==mm->count);
						if(item) ext++;
					}
					if(!item) continue;
					predict_copy(l_predict_data,temp,item,-1);
					if(memcmp(prefix,temp,2*base))
					{
						continue;
					}
					strcpy(mm->cand+strlen(mm->cand),temp+2*base);
					pos+=seq[i];
					if(ext>1) pos+=seq[i+1];
					i+=ext;
					j=k+ext;
					break;
				}
				if(k==0) break;
			}
			
			goto next;
		}
#endif
		c=unigram_codec[pos<<3|seq[i]];
		//fprintf(stderr,"%d %d %d %p %d\n",i,pos,seq[i],c,pos<<3|seq[i]);
		strcat(out,y_mb_ci_string(c));
		pos+=seq[i];
		i++;
next:;
	}
	//printf("%s\n",out);
}

static int unigram(MMSEG *mm)
{
	uint8_t seq[64];
	int len;
	
	//clock_t start=clock();
	
	memset(mm->codec,-1,sizeof(mm->codec));
	memset(unigram_split,0,sizeof(unigram_split));
	mm->mark_skip=0;
	
	if(0==unigram_best(mm,0,mm->count))
	{
		//printf("find best fail\n");
		return -1;
	}
	len=unigram_build(0,mm->count,seq,0);
	
#if 1
	int i,pos;
	for(i=0,pos=0;i<len-1;i++)
	{
		struct y_mb_ci *c;
		int freq1,freq2;
		if(seq[i]!=1)
		{
			pos+=seq[i];
			continue;
		}
		if(seq[i+1]!=1)
		{
			pos+=seq[i];
			continue;
		}
		c=mm->codec[pos<<3|2];
		if(c==(void*)-1)
		{
			mmseg_exist(mm,mm->input+pos,2);
			c=mm->codec[pos<<3|2];
		}
		if(c==NULL)
		{
			pos+=seq[i];
			continue;
		}
		freq1=(unigram_best(mm,pos,1)>>6)+(unigram_best(mm,pos+1,1)>>6);
		freq2=unigram_best(mm,pos,2)>>6;
		//printf("%d %d\n",freq1,freq2);
		
		if(i<len-2 && seq[i+2]==1)
		{
			if(freq2-freq2/10>freq1)
			{
				pos+=seq[i];
				continue;
			}
		}
		else if(freq2-freq2/25>freq1)
		{
			pos+=seq[i];
			continue;
		}
		//fprintf(stdout,"%d %d %x %s\n",i,pos,(pos<<3|2),y_mb_ci_string(c));
		seq[i]=2;
		memmove(seq+i+1,seq+i+2,len-i-2);
		len--;
		pos+=seq[i];
	}
#endif
	unigram_output(mm,seq,len,mm->cand);
	
	//printf("%.3f\n",(clock()-start)*1.0/CLOCKS_PER_SEC);

	return 0;
}

#endif

static int mmseg_split(MMSEG *mm)
{
	int mmcount;					/* 所有可能的三元组的数量 */
	uint8_t mmlen[80];				/* 一个三元组的组合的长度 */
	uint8_t mmword[80];			/* 三元组第一元的长度 */
	uint8_t mmword2[80];			/* 三元组前二元的长度 */
	uint8_t mmnword[80];			/* 候选组合有多少元 */
	int mmlogcf[80];			/* 对应编码和词的词频 */
	int mmlogcf2[80];			/* 前二元的词频 */
	uint16_t mmavglen[80];			/* 三元组的平均长度 */
	float mmvarlen[80];			/* 三元组的长度变化 */
	uint8_t mmlong[80];			/* 候选三元组中最长的一批 */
	struct y_mb_ci *mmfirst[80];	/* 对应词频的首选词 */
	int longest;					/* 当前最长的候选三元组 */
	int i,k,l;
	int result=0;

	mm->mark_skip=0;
    memset(mm->codec,-1,sizeof(mm->codec));

    for(i=0;i<mm->count;)
    {
        /* 得到以i位置开始的三元组，和相关统计信息 */
		int w[3];
		
		longest=0;
		mmcount=0;
		/* w[0]放在最前面，可以使得第一个最长词稍微有点优先度 */
		for(w[0]=7;w[0]>=1;w[0]--){
		if(i+w[0] > mm->count) continue;
		if(!mmseg_exist(mm,mm->input+i,w[0])) continue;
		for(w[1]=7;w[1]>=0;w[1]--){
		if(i+w[0]+w[1] > mm->count) continue;
		if(w[1] && !mmseg_exist(mm,mm->input+i+w[0],w[1]))
		{
			/* 即使单字不存在，仍然继续查询其他组合
			 * 对正常输入法来说，单字必然存在，减少一个判断反而会让输入法的速度提升
			if(w[1]==1)
			{
				return -1;
			}
			*/
			continue;
		}
		for(w[2]=7;w[2]>=0;w[2]--){
		if(i+w[0]+w[1]+w[2]>mm->count) continue;
		if(!w[1] && w[2]) continue;
		if(w[2] && !mmseg_exist(mm,mm->input+i+w[0]+w[1],w[2]))
		{
			/*
			if(w[2]==1)
			{
				return -2;
			}
			*/
			continue;
		}
		{
			float sumsq=0;
			uint16_t avglen;
			
			mmword[mmcount]=w[0];
			mmword2[mmcount]=w[0]+w[1];
			mmlen[mmcount]=0;
			mmnword[mmcount]=0;
			mmavglen[mmcount]=0;
			mmlogcf[mmcount]=0;
			mmlogcf2[mmcount]=0;
			mmfirst[mmcount]=0;
			
			for(k=0;k<3;k++)
				mmlen[mmcount]+=w[k];
			if(mmlen[mmcount]<longest)
				continue;
			if(mmlen[mmcount]>longest)
			{
				mmlen[0]=mmlen[mmcount];
				mmcount=0;
				mmword[mmcount]=w[0];
				mmword2[mmcount]=w[0]+w[1];
				mmnword[mmcount]=0;
				mmavglen[mmcount]=0;
				mmlogcf[mmcount]=0;
			}

			for(k=0;k<3;k++)
			{
				if(w[k]>0)
				{
					struct y_mb_ci *ci;
					int pos=i;
					int logcf;
					if(k>=1) pos+=w[0];
					if(k>=2) pos+=w[1];
					logcf=mmseg_logcf(mm,pos,w[k],&ci);			
					mmlogcf[mmcount]+=logcf;
					if(k<2) mmlogcf2[mmcount]+=logcf;
					if(k==0)
					{
						//assert(ci!=NULL);
						mmfirst[mmcount]=ci;
					}
					mmnword[mmcount]++;
				}
			}
			mmavglen[mmcount]=avglen=(uint16_t)(mmlen[mmcount]*100/mmnword[mmcount]);
			for(k=0;k<mmnword[mmcount];k++)
				sumsq+=(float) (w[k] - avglen/100.0) * (w[k] - avglen/100.0);
			mmvarlen[mmcount]=sumsq;
			if(mmlen[mmcount]>longest)
				longest=mmlen[mmcount];
			
			/* 可能在最后只剩下一个词，直接输出就好，使用语料库反而出错 */	
			if(!w[1])
			{
				struct y_mb_ci *c=mmfirst[mmcount];
				y_mb_ci_string2(c,mm->cand+strlen(mm->cand));
				return result;
			}
			mmcount++;
			//printf("%d %d: %d %d %d %d %.2f\n",i,mmcount,w[0],w[1],w[2],mmlogcf[mmcount-1],mmavglen[mmcount-1]/100.0);
			
			/* 这里跳出循环，因为本循环里面最长的词了，更短点没有意义 */
			break;
		}}}}
        if(longest==0)
        {
            /* 一个都没找到，一个不合法的输入串 */
            return result;
        }
        /* 得到有最大长度的的列表 */
        for(l=k=0;k<mmcount;k++)
        {
            if(mmlen[k]==longest)
            {
                mmlong[l]=k;
                l++;
            }
		}
		if(l>0 && l_predict_data && l_predict_data->it_count>0)
        {
			LEARN_ITEM *key=&l_predict_data->key;
			LEARN_ITEM *item,*prev=NULL;
			int pos,which,full=1;
			int prev_count=0;
			char *temp=(char*)l_predict_data->raw_data+key->code;
			for(k=0;k<l;k++)
			{
				int count=mmlen[mmlong[k]];
				if(count==prev_count)
				{
					/* 有可能出现几个组合是同一个编码的情况，这种情况下不重复查找 */
					continue;
				}
				prev_count=count;
				py_build_string(temp,mm->input+i,count);
				py_prepare_string(temp,temp,0);
				item=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,i+count==mm->count);
				if(!item) continue;
				if(!prev || item->lf[3]>prev->lf[3])
				{
					pos=0;
					prev=item;
					which=mmlong[k];
				}
			}
			if(!prev && longest>=5 && i+longest<mm->count)
			{
				/* 这里简单处理某些四元组，但我们不处理嵌套的语料 */
				int count=longest+1;
				py_build_string(temp,mm->input+i,count);
				py_prepare_string(temp,temp,0);
				item=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,i+count==mm->count);
				if(item && item->lf[1]==3)
				{
					char *predict=mm->cand+strlen(mm->cand);
					predict_copy(l_predict_data,predict,item,-1);
					i+=longest+1;
					continue;
				}
			}
			if(!prev && longest>=4)
			{
				full=0;
				if(longest==4)
				{
					/* 有常用词2+2，不再尝试部分语音料匹配 */
					for(k=0;k<l;k++)
					{
						if(mmword[k]==2 && mmword2[k]==4)
							goto skip_2_2;
					}
				}
				for(k=0;k<l;k++)
				{
					int count=mmnword[mmlong[k]];
					if(count!=3) continue;
					py_build_string(temp,mm->input+i,mmword2[mmlong[k]]);
					py_prepare_string(temp,temp,0);
					item=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,0);
					if(item && (!prev || item->lf[3]>prev->lf[3]))
					{
						pos=0;
						prev=item;
						which=mmlong[k];
					}
					py_build_string(temp,mm->input+i+mmword[mmlong[k]],mmlen[mmlong[k]]-mmword[mmlong[k]]);
					py_prepare_string(temp,temp,0);
					item=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,i+mmlen[mmlong[k]]==mm->count);
					if(item && (!prev || item->lf[3]>prev->lf[3]))
					{
						pos=1;
						prev=item;
						which=mmlong[k];
					}
				}
				skip_2_2:;
			}
			if(prev) /* 我们找到了最有可能性的语料了 */
			{
				/* last: 最后一个词的起始位置 base: 最后一个词的长度 */
				int last,base;
				char *prefix,*predict;
				
				if(pos==0)
				{
					predict=mm->cand+strlen(mm->cand);
					predict_copy(l_predict_data,predict,prev,-1);
					
					if(mmlen[which]==mmword2[which] || !full)
					{
						last=i+mmword[which];
						base=mmword2[which]-mmword[which];
					}
					else
					{
						last=i+mmword2[which];
						base=mmlen[which]-mmword2[which];
					}
					if(full)
						i+=mmlen[which];
					else
						i+=mmword2[which];
				}
				else
				{
					/* 23位置是语料，我们只应该先选择第一个，否则34也是语料的话，否则就无法利用频率信息了 */
					y_mb_ci_string2(mmfirst[which],mm->cand+strlen(mm->cand));
					i+=mmword[which];
					continue;
				}
				//printf("find in dict %s\n",predict);
				
				/* 递归查找是否有和当前语料想匹配的二元组 */
				prefix=predict+strlen(predict)-2*base;
				while(prefix>predict && mm->count>last+base)
				{
					//printf("prefix %s\n",prefix);	
					k=MIN(7,mm->count-last-base);
					for(;k>0;k--)
					{
						py_build_string(temp,mm->input+last,base+k);
						py_prepare_string(temp,temp,0);
						//printf("%d %s\n",k,temp);
						item=predict_search(l_predict_data,mm,temp,PSEARCH_PHRASE,0,last+base+k==mm->count);
						if(!item) continue;
						predict_copy(l_predict_data,temp,item,-1);
						if(memcmp(prefix,temp,2*base))
						{
							continue;
						}
						predict=temp;
						break;
					}
					if(k==0) break;
					//printf("predict %s\n",predict);
					strcpy(mm->cand+strlen(mm->cand),predict+2*base);
					last+=base;
					base=k;
					i+=k;
					//prefix=predict+strlen(predict)-2*base;
					prefix=mm->cand+strlen(mm->cand)-2*base;
				}
				continue;
			}
		}

        if(l==1)
        {
            /* 首先应用最大匹配原则 */
            int mmword_len=mmword[mmlong[0]];
            y_mb_ci_string2(mmfirst[mmlong[0]],mm->cand+strlen(mm->cand));
			i+=mmword_len;
			//printf("find in long %d %s\n",mmword_len,y_mb_ci_string(mmfirst[mmlong[0]]));
        }
        else
        {
            /* 应用平均词长原则 */
            float lavg=0;
            int n=0;
            uint8_t largeavg[100];
            for(k=0;k<l;k++)
            {
                if(mmavglen[mmlong[k]]>lavg)
                    lavg=mmavglen[mmlong[k]];
            }
			for(k=0;k<l;k++)
			{
				if(mmavglen[mmlong[k]]==lavg)
				{
					largeavg[n]=mmlong[k];
					n++;
				}
			}
            if(n==1)
            {
				int mmword_len=mmword[largeavg[0]];
				y_mb_ci_string2(mmfirst[largeavg[0]],mm->cand+strlen(mm->cand));
				i+=mmword_len;
				//printf("find in avg %d %.2f %s\n",mmword_len,lavg,y_mb_ci_string(mmfirst[largeavg[0]]));
            }
            else
            {
                /* 应用词长的较小均方差原则 */
                float svar=100;
                uint8_t smallvar[100];
                int o;
                for(k=0;k<n;k++)
                {
                    if(mmvarlen[largeavg[k]]<svar)
                        svar=mmvarlen[largeavg[k]];
                }
                for(o=k=0;k<n;k++)
                {
					//最小均方差原则经常得到错误且难以理解的结果，屏蔽这行以暂时禁用
                    //if(mmvarlen[largeavg[k]]==svar)
                    {
                        smallvar[o]=largeavg[k];
                        o++;
                    }
                }
                if(o==1)
                {
					int mmword_len=mmword[smallvar[0]];
					y_mb_ci_string2(mmfirst[smallvar[0]],mm->cand+strlen(mm->cand));
					i+=mmword_len;
					//printf("find in var %d %s\n",mmword_len,y_mb_ci_string(mmfirst[smallvar[0]]));
                }
                else
                {
                    /* 应用最大的字词频logcf的和原则 */
                    int llog=-1000000;
                    uint8_t largelog=0;
                    char *out=mm->cand+strlen(mm->cand);
                    int mmword_len;
                    
                    for(k=0;k<o;k++)
                    {
                        if(mmlogcf[smallvar[k]]>llog)
                            llog=mmlogcf[smallvar[k]];
                    }
					for(k=0;k<o;k++)
					{
						if(mmlogcf[smallvar[k]]==llog)
						{
							largelog=smallvar[k];
							break;
						}
					}
					/*if(mmword2[largelog]==4 && mmword[largelog]==2)
					{
						if(!mmseg_fix(mm,i,4,mmlogcf2[largelog],out))
						{
							i+=4;
							continue;
						}
					}*/
					mmword_len=mmword[largelog];
					y_mb_ci_string2(mmfirst[largelog],out);
					i+=mmword_len;
					//printf("find in freq %d %d %s\n",mmword_len,llog,y_mb_ci_string(mmfirst[largelog]));
                }
            }
        }
    }
    return result;
}

const char *y_mb_predict_nth(const char *s,int n)
{
	int i;
	for(i=0;i<n;i++)
	{
		int len=strlen(s);
		assert(len>0);
		s+=len+1;
	}
	return s;
}

static int get_space_pos(MMSEG *mm,const char *s)
{
	int pos=0;
	const char *p;
	p=strchr(s,' ');
	if(p)
	{
		struct y_mb *mb=mm->mb;
		int i,split;
		pos=(int)(p-s);
		for(i=split=0;i<pos;i++)
			if(s[i]==mb->split) split++;
		pos-=split;
	}
	return pos;
}

static inline int zrm_csh_mohu(int in,int out)
{
	if(in=='c' && out=='i')
		return 1;
	if(in=='s' && out=='u')
		return 1;
	if(in=='z' && out=='v')
		return 1;
	return 0;
}

struct _p_item{struct y_mb_ci *c;int f;int m;};
static int _p_item_cmpar(struct _p_item *it1,struct _p_item *it2)
{int m=it2->m-it1->m;if(m) return m;return it2->f-it1->f;}
static int predict_quanpin_simple(struct y_mb *mb,py_item_t *item,int count,char *out,int *out_len)
{
	int ret,len;
	char temp[128];
	trie_iter_t iter;
	trie_tree_t *trie;
	trie_node_t *n;
	int depth;
	char *c;
	LArray *array;

	if(!(trie=mb->trie) || count<2)
		return 0;
	depth=py_build_sp_string(temp,item,count);
	if(!(c=strchr(temp,'\'')) || !c[1])
		return 0;
	
	array=l_array_new(26,sizeof(struct _p_item));
retry:
	n=trie_iter_path_first(&iter,trie,NULL,64);
	while(n!=NULL)
	{
		int cur=iter.depth;
		if(cur<depth)
		{
			//printf("%d %c\n",cur,temp[cur]);
			int c=temp[cur];
			if(n->self!=c && c!='\'' && ((cur&0x01)!=0 || !zrm_csh_mohu(c,n->self)))
			{
				trie_iter_path_skip(&iter);
				n=trie_iter_path_next(&iter);
				continue;
			}
		}
		if(cur>=depth-1 && n->leaf)
		{
			struct y_mb_ci *ci=trie_node_get_leaf(trie,n)->data;
			for(;ci!=NULL;ci=ci->next)
			{
				struct _p_item item;
				if(ci->del || ci->zi || ci->len!=cur+1)
					continue;
				c=y_mb_ci_string(ci);
				item.c=ci;
				item.f=(l_predict_data && ci->len<15)?ci_freq_get(c):0;
				item.m=(ci->len==depth);
				l_array_insert_sorted(array,&item,(LCmpFunc)_p_item_cmpar);
				if(!l_predict_data && array->len>25)
					array->len=25;
				if(l_predict_data && array->len>10)
					array->len=10;
			}
		}
		n=trie_iter_path_next(&iter);
	}
	if(array->len==0)
	{
		// FIXME: 这里还是只能解决部分的歧义
		c=strpbrk(temp,"uvi");
		if(c!=NULL && c[1]=='\'')
		{
			memmove(c+4,c+2,strlen(c+2)+1);
			if(c[0]=='u') c[0]='s';
			else if(c[0]=='v') c[0]='z';
			else c[0]='c';
			c[1]='\'';
			c[2]='h';
			c[3]='\'';
			goto retry;
		}
	}
	for(ret=len=0;ret<array->len;ret++)
	{
		struct _p_item *it=l_array_nth(array,ret);
		struct y_mb_ci *ci=it->c;
		if(len+ci->len+1+1>MAX_CAND_LEN)
			break;
		c=y_mb_ci_string(ci);
		y_mb_ci_string2(ci,out+len);
		len+=ci->len+1;
	}
	out[len]=0;
	l_array_free(array,NULL);
	if(out_len)
		*out_len=len+1;
	return ret;
}

static int y_mb_find_setence(MMSEG *mm,const char *code)
{
	typedef struct{
		LEARN_ITEM *p;
		int len;
	}RES_ITEM;
	#define MATCH_COUNT		16

	RES_ITEM lst[MATCH_COUNT];
	int lcnt=0;
	
	LEARN_ITEM *item;
	int i,count,len,tmp;
	int first_match=-1;
	int assist_end;
	int cp_len;
	char temp[128];
	
	if(!l_predict_data)
		return 0;

	assist_end=mm->assist_end;
	mm->assist_end=0;

#if !PYUNZIP_BEFORE_CMP
	cp_zip(code,temp);
	cp_len=strlen(temp);
#else
	cp_len=strlen(code);
#endif
	
	item=predict_search(l_predict_data,mm,code,PSEARCH_ALL,&count,1);
	if(item!=NULL) for(i=0;i<count && lcnt<MATCH_COUNT;i++,item++)
	{
		int res_len=-1;
#if !PYUNZIP_BEFORE_CMP
		char *item_code=(char*)l_predict_data->raw_data+item->code;
		if(item_code[cp_len]!=0)
		{
			res_len=mm->count;
		}
#else
		cp_unzip((char*)l_predict_data->raw_data+item->code,temp);
		if(temp[cp_len]!=0)
		{
			res_len=mm->count;
		}
#endif
		if(assist_end)
		{
			int templen;
			templen=predict_copy(l_predict_data,temp,item,res_len);
			if(y_mb_assist_test_hz(mm->mb,temp+templen-2,assist_end)==0)
				continue;
		}
		lst[lcnt++]=(RES_ITEM){item,res_len};
		if(first_match==-1)
			first_match=i;
	}
	
	mm->assist_end=assist_end;
	
	for(i=0,tmp=0,len=0;i<lcnt;i++)
	{
		int templen,j;
		if(mm->assist_end && i==0 && first_match==0)
			continue;
		if(mm->count<5 && lst[i].len>=0)
			continue;
		for(j=0;j<i;j++)
		{
			if(0==predict_compar_part_cand(lst[i].p,lst[j].p))
			{
				break;
			}
		}
		if(i!=0 && j<i)
			continue;
		templen=predict_copy(l_predict_data,temp,lst[i].p,lst[i].len);
		if(tmp+templen+1>MAX_CAND_LEN)
			break;
		strcpy(mm->cand+tmp,temp);
		tmp+=templen+1;
		len++;
	}
	if(!mm->assist_end && mm->count>=5) for(i=0;i<lcnt;i++)
	{
		int templen;
		if(lst[i].len<=0) continue;
		templen=predict_copy(l_predict_data,temp,lst[i].p,-1);
		if(templen==2*lst[i].len) continue;
		if(tmp+templen+1>MAX_CAND_LEN)
			break;
		strcpy(mm->cand+tmp,temp);
		tmp+=templen+1;
		len++;
	}

	if(len && (!mm->assist_end || first_match>=0))
		return len;
	mm->cand[0]=0;
	return 0;
}

int y_mb_predict_by_learn(struct y_mb *mb,char *s,int caret,char *out,int size,int begin)
{
	MMSEG mm;
	int len;
	int tmp;
	char temp[256];
	
	char simple_data[256];
	int simple_count=0;
	int simple_size;

	mm.mb=mb;
	mm.setence_begin=begin;
	mm.setence_end=(s[caret]==0);
	mm.assist_begin=0;
	mm.assist_end=0;
	tmp=s[caret];s[caret]=0;

	if(l_predict_sp)
	{
		py_prepare_string(temp,s,0);
		if(l_predict_simple && !PySwitch)
		{
			mm.count=py_parse_sp_simple(s,mm.input);
			if(mm.count>1)
				simple_count=predict_quanpin_simple(mb,mm.input,mm.count,simple_data,&simple_size);
			else
				simple_count=y_mb_predict_simple(mb,temp,simple_data,&simple_size,l_predict_data?ci_freq_get:0);
			if(simple_count>0)
			{
				memcpy(out,simple_data,simple_size);
				s[caret]=tmp;
				return simple_count;
			}
		}
		len=py_conv_from_sp(s,temp,sizeof(temp),'\'');
		if(tmp==0)
		{
			int assist=s[caret-1];
			if(!islower(assist) && !(assist==';' && py_sp_has_semi()))
			{
				mm.assist_end=s[caret-1];
			}
			else if(len>=3 && temp[len-1]=='\'')
			{
				mm.assist_end=temp[len-2];
				temp[len-2]=0;
			}
		}
		mm.space=get_space_pos(&mm,temp);
		mm.count=py_parse_string(temp,mm.input,-1);
		if(mm.count<=0)
			return 0;
		mm.count=py_remove_split(mm.input,mm.count);
		mm.space2=py_get_space_pos(mm.input,mm.count,mm.space);
		if(mm.assist_end)
		{
			char last[16];
			py_build_string(last,mm.input+mm.count-1,1);
			if(!y_mb_check_assist(mb,last,strlen(last),mm.assist_end,1))
			{
				mm.assist_end=0;
				len=py_conv_from_sp(s,temp,sizeof(temp),'\'');
				mm.space=get_space_pos(&mm,temp);
				mm.count=py_parse_string(temp,mm.input,-1);
				mm.count=py_remove_split(mm.input,mm.count);
				mm.space2=py_get_space_pos(mm.input,mm.count,mm.space);
			}
		}
	}
	else
	{
		mm.space=get_space_pos(&mm,s);
		mm.count=py_parse_string(s,mm.input,-1);
		mm.count=py_remove_split(mm.input,mm.count);
		mm.space2=py_get_space_pos(mm.input,mm.count,mm.space);
	}
	s[caret]=tmp;

	if(mm.count<=1)
	{
		return 0;
	}
	
	if(mm.space>0)
	{
		assert(mm.space2);
	}
	
	py_build_string(temp,mm.input,mm.count);
	py_prepare_string(temp,temp,0);

	if(!l_predict_sp && l_predict_simple  && !PySwitch)
	{
		if(mb->trie)
			simple_count=predict_quanpin_simple(mb,mm.input,mm.count,simple_data,&simple_size);
		else
			simple_count=y_mb_predict_simple(mb,temp,simple_data,&simple_size,l_predict_data?ci_freq_get:0);
		if(simple_count>0)
		{
			memcpy(out,simple_data,simple_size);
			return simple_count;
		}
	}

	mm.cand=out;
	mm.cand[0]=0;

	tmp=mb->match;
	mb->match=1;

	/* 对双拼加形在末尾有三个字母的情况下，强制认为是句尾辅助码 */
	if((mb->split==2 || l_predict_sp ) && mm.count>2 && !mm.assist_end && s[caret]=='\0')
	{
		char last[16];
		if(l_predict_sp)
		{
			py_build_sp_string(last,mm.input+mm.count-1,1);
			if(last[1]=='\'')
				last[1]=0;
		}
		else
		{
			py_build_string(last,mm.input+mm.count-1,1);
		}
		py_prepare_string(last,last,0);
		if(strlen(last)==1)
		{
			int assist=last[0];
			py_build_string(last,mm.input+mm.count-2,1);
			if(y_mb_check_assist(mb,last,strlen(last),assist,1))
			{
				mm.assist_end=assist;
				mm.count--;
				temp[strlen(temp)-1]=0;
			}
		}
	}

	len=y_mb_find_setence(&mm,temp);
	if(len>0) return len;

	if(mm.assist_end)
	{
		int temp=mm.assist_end;
		int count;
		mm.assist_end=0;
		mm.last_zi[0]=0;
		if(l_predict_data!=NULL && !l_force_mmseg)
			count=unigram(&mm);
		else
			count=mmseg_split(&mm);
		len=strlen(mm.cand);
		if(len>=4)
		{
			mm.assist_end=temp;
			strcpy(mm.last_zi,mm.cand+len-4);
			mm.cand[0]=0;
			if(l_predict_data!=NULL && !l_force_mmseg)
				mm.count=unigram(&mm);
			else
				mm.count=mmseg_split(&mm);
		}
		else
		{
			mm.count=count;
		}
	}
	else
	{
		mm.last_zi[0]=0;
		if(l_predict_data!=NULL && !l_force_mmseg)
		{
			mm.count=unigram(&mm);
		}
		else
		{
			mm.count=mmseg_split(&mm);
		}
	}

	len=strlen(mm.cand);
	mb->match=tmp;
	
	if(gb_strlen((uint8_t*)mm.cand)==1)
	{
		out[0]=0;
		len=0;
	}
	if(mm.count>0)
		return mm.count;
	
	return len>0?1:0;
}


