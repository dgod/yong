#include <llib.h>
#include <string.h>
#include "cset.h"
#include "gbk.h"
#include "mb.h"
#include "learn.h"

typedef struct _assoc_item{
	struct _assoc_item *next;
	char *phrase;
	char *code;
	int index;
}ASSOC_ITEM;

static void assoc_item_free(ASSOC_ITEM *p)
{
	if(!p)
		return;
	free(p->code);
	free(p->phrase);
	free(p);
}

typedef struct{
	void *next;
	const char *cand;
}DEDUP_ITEM;

static void dedup_item_free(void *p)
{
	y_mb_slice_free(p,sizeof(DEDUP_ITEM));
}
#define DEDUP_MAX			2047


static LHashTable *dedup;
void cset_init(CSET *cs)
{
	memset(cs,0,sizeof(*cs));
	cs->calc.size=Y_MB_DATA_CALC;

	const char *s;
	s=EIM.GetConfig(NULL,"assoc_len");
	if(s!=NULL && atoi(s)>0)
	{
		s=EIM.GetConfig(NULL,"assoc_adjust");
		if(s!=NULL)
			cs->assoc_adjust=atoi(s);	
		if(s!=NULL && atoi(s)>0)
		{
			s=EIM.GetConfig(NULL,"assoc_adjust_add");
			if(s!=NULL)
			{
				cs->assoc_adjust_add=(short)atoi(s);
			}
			cs->calc.size=128;
			s=EIM.GetConfig(NULL,"assoc_adjust_code");
			if(s!=NULL)
			{
				cs->assoc_adjust_code=(short)atoi(s);
			}
		}
	}
	cs->calc.phrase=l_calloc(cs->calc.size,MAX_CAND_LEN+1);
	s=EIM.GetConfig(NULL,"dedup");
	if(s && s[0])
		cs->dedup_mode=atoi(s);
	dedup=L_HASH_TABLE_STRING(DEDUP_ITEM,cand,DEDUP_MAX);
}

void cset_group_free(CSET_GROUP *g)
{
	if(!g)
		return;
	
	if(!g->free)
	{
		g->count=0;
		return;
	}
	g->offset=0;
	g->free(g);
}

void cset_reset(CSET *cs)
{
	l_slist_free(cs->list,(LFreeFunc)cset_group_free);
	cs->list=NULL;

	if(cs->assoc_reset)
	{
		if(cs->assoc) // should never happen
			l_hash_table_free(cs->assoc,(LFreeFunc)assoc_item_free);
		cs->assoc=cs->assoc_reset;
		cs->assoc_reset=NULL;
	}
	else
	{
		l_hash_table_clear(cs->assoc,(LFreeFunc)assoc_item_free);
	}
	l_hash_table_clear(dedup,dedup_item_free);
}

void cset_destroy(CSET *cs)
{
	cset_reset(cs);
	l_hash_table_free(cs->assoc,(LFreeFunc)assoc_item_free);
	cs->assoc=NULL;
	l_hash_table_free(cs->assoc_reset,(LFreeFunc)assoc_item_free);
	cs->assoc_reset=NULL;
	l_array_free(cs->array.array,(LFreeFunc)cset_array_item_free);
	cs->array.array=NULL;
	l_free(cs->calc.phrase);
	cs->calc.phrase=NULL;
	l_array_free(cs->mb.array,(LFreeFunc)cset_array_item_free);
	cs->mb.array=NULL;
	l_array_free(cs->predict.array,(LFreeFunc)cset_array_item_free);
	cs->predict.array=NULL;
	l_hash_table_free(dedup,(LFreeFunc)dedup_item_free);
	dedup=NULL;
}

void cset_append(CSET *cs,CSET_GROUP *g)
{
	assert(g->type>0 && g->type<CSET_TYPE_UNKNOWN);
	cs->list=l_slist_append(cs->list,g);
}

void cset_prepend(CSET *cs,CSET_GROUP *g)
{
	assert(g->type>0 && g->type<CSET_TYPE_UNKNOWN);
	cs->list=l_slist_prepend(cs->list,g);
}

void cset_remove(CSET *cs,CSET_GROUP *g)
{
	cs->list=l_slist_remove(cs->list,g);
}

int cset_count(CSET *cs)
{
	int total=0;
	for(CSET_GROUP *g=cs->list;g!=NULL;g=g->next)
	{
		total+=g->count-g->offset;
	}
	return total;
}

void cset_clear(CSET *cs,int type)
{
	if(type==CSET_TYPE_ALL)
	{
		while(cs->list)
		{
			CSET_GROUP *g=cs->list;
			cset_remove(cs,g);
			cset_group_free(g);
		}
		return;
	}
	CSET_GROUP *g=cset_get_group_by_type(cs,type);
	if(!g)
		return;
	cset_remove(cs,g);
	cset_group_free(g);
}

int cset_group_offset(CSET_GROUP *g,int offset)
{
	g->offset=offset;
	return 0;
}

#if 0
void cset_group_dump(CSET_GROUP *g,FILE *fp)
{
	int i;
	for(i=0;i<g->count;i++)
	{
		char cand[1][MAX_CAND_LEN+1];
		char tip[1][MAX_TIPS_LEN+1];
		g->get(g,i,1,cand,tip);
#ifdef _WIN32
		fprintf(fp,"%s\n",cand[0]);
#else
		char temp[256];
		l_gb_to_utf8(cand[0],temp,sizeof(temp));
		fprintf(fp,"%s\n",temp);
#endif
	}
	fprintf(fp,"\n");
}
#endif

int cset_output(CSET *cs,int pos,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	CSET_GROUP *g=cs->list;
	int CandWordCount=0;
	for(;g!=NULL;g=g->next)
	{
		int count=g->count-g->offset;
		if(pos>=count)
		{
			pos-=count;
			continue;
		}
		if(count-pos>=num)
		{
			g->get(g,pos+g->offset,num,cand+CandWordCount,tip+CandWordCount);
			CandWordCount+=num;
			break;
		}
		else
		{
			count-=pos;
			g->get(g,pos+g->offset,count,cand+CandWordCount,tip+CandWordCount);
			num-=count;
			pos=0;
			CandWordCount+=count;
		}
	}
	return CandWordCount;
}

static int cset_predict_group_get(CSET_GROUP_PREDICT *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	for(int i=0;i<num;i++)
	{
		const char *s=cset_predict_group_nth(g,at+i);
		strcpy(cand[i],s);
		tip[i][0]=0;
	}
	return 0;
}

static void cset_predict_group_free(CSET_GROUP_PREDICT *g)
{
	if(!g)
		return;
	l_array_clear(g->array,(LFreeFunc)cset_array_item_free);
}

void cset_predict_group_init(CSET_GROUP_PREDICT *g)
{
	g->offset=0;

	g->type=CSET_TYPE_PREDICT;
	g->get=(void*)cset_predict_group_get;
	g->free=(void*)cset_predict_group_free;

	if(!g->array)
		g->array=l_array_new(Y_MB_DATA_CALC,sizeof(CSET_ARRAY_ITEM));
	else
		l_array_clear(g->array,(LFreeFunc)cset_array_item_free);
	g->count=0;
}

CSET_GROUP_PREDICT *cset_predict_group_new(CSET *cs)
{
	CSET_GROUP_PREDICT *g=&cs->predict;
	cset_remove(cs,(CSET_GROUP*)g);
	cset_predict_group_init(g);
	return g;
}

const char *cset_predict_group_nth(CSET_GROUP_PREDICT *g,int i)
{
	if(i>=g->count)
		return NULL;
	CSET_ARRAY_ITEM *item=l_array_nth(g->array,i);
	return L_CPTR(item->cand);
}

int cset_predict_group_append(CSET_GROUP_PREDICT *g,const char *s)
{
	CSET_ARRAY_ITEM *item=l_array_append(g->array,NULL);
	cset_array_item_fill(item,s,strlen(s),NULL,0);
	g->count++;
	return 0;
}

int cset_predict_group_count(CSET *cs)
{
	return cs->predict.count;
}

int cset_predict_group_dedup(CSET_GROUP_PREDICT *g)
{
	cset_array_item_dedup(g->array);
	g->count=l_array_length(g->array);
	return 0;
}

static int cset_calc_group_get(CSET_GROUP_CALC *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	assert(at+num<=g->count);
	for(int i=0;i<num;i++)
	{
		const char *s=g->phrase[at+i];
		strcpy(cand[i],s);
		tip[i][0]=0;
	}
	return 0;
}

CSET_GROUP_CALC *cset_calc_group_new(CSET *cs)
{
	CSET_GROUP_CALC *g=&cs->calc;
	cset_remove(cs,(CSET_GROUP*)g);

	g->offset=0;
	g->count=0;

	g->type=CSET_TYPE_CALC;
	g->get=(void*)cset_calc_group_get;
	g->free=NULL;

	g->count=0;
	return g;
}

int cset_calc_group_count(CSET *cs)
{
	return cs->calc.count;
}

int cset_calc_group_append(CSET_GROUP_CALC *g,const void *s)
{
	if(g->count>=Y_MB_DATA_CALC)
		return -1;
	strcpy(g->phrase[g->count++],s);
	return 0;
}

int cset_calc_group_append2(CSET_GROUP_CALC *g,const void *s,int len)
{
	if(g->count>=Y_MB_DATA_CALC)
		return -1;
	l_memcpy0(g->phrase[g->count++],s,len);
	return 0;
}

static int cset_mb_group_get(CSET_GROUP_MB *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	if(g->array && l_array_length(g->array)>0)
	{
		for(int i=0;i<num;i++)
		{
			CSET_ARRAY_ITEM *item=l_array_nth(g->array,at+i);
			memcpy(cand[i],L_CPTR(item->cand),item->cand_size);
			memcpy(tip[i],L_CPTR(item->codetip),item->codetip_size);
		}
		return 0;
	}
	else
	{
		struct y_mb *mb=g->mb;
		int ret=y_mb_get(mb,at,num,cand,tip);
		return ret;
	}
}

static void cset_mb_group_free(CSET_GROUP_MB *g)
{
	g->count=0;
	g->offset=0;
	if(!g->array || !g->array->len)
		return;
	l_array_clear(g->array,(LFreeFunc)cset_array_item_free);
}

static void cset_mb_group_dedup(CSET *cs,CSET_GROUP_MB *g)
{
	if(g->count<=1)
		return;
	if(!g->array)
	{
		g->array=l_array_new(DEDUP_MAX,sizeof(CSET_ARRAY_ITEM));
	}
	else if(l_array_length(g->array)>0)
	{
		return;
	}
	const int STEP=30;
	char cand[STEP][MAX_CAND_LEN+1];
	char tip[STEP][MAX_TIPS_LEN+1];
	for(int at=0;at<g->count;at+=STEP)
	{
		int num=MIN(STEP,g->count-at);
		y_mb_get(g->mb,at,num,cand,tip);
		for(int i=0;i<num;i++)
		{
			CSET_ARRAY_ITEM item;
			cset_array_item_fill(&item,cand[i],strlen(cand[i]),tip[i],strlen(tip[i]));
			DEDUP_ITEM *dedup_item=y_mb_slice_new(DEDUP_ITEM);
			dedup_item->cand=L_CPTR(item.cand);
			if(!l_hash_table_insert(dedup,dedup_item))
			{
				dedup_item_free(dedup_item);
				cset_array_item_free(&item);
			}
			else
			{
				l_array_append(g->array,&item);
				if(l_array_length(g->array)>=DEDUP_MAX)
					goto out;
			}
		}
	}
out:
	l_hash_table_clear(dedup,dedup_item_free);
	g->count=l_array_length(g->array);
}

CSET_GROUP_MB *cset_mb_group_new(CSET *cs,struct y_mb *mb,int count)
{
	CSET_GROUP_MB *g=&cs->mb;
	cset_remove(cs,(CSET_GROUP*)g);

	cset_mb_group_free(g);

	g->type=CSET_TYPE_MB;
	g->get=(void*)cset_mb_group_get;
	g->free=(void*)cset_mb_group_free;

	g->count=count;
	g->mb=mb;

	if(cs->dedup_mode==1)
		cset_mb_group_dedup(cs,g);

	return g;
}

void cset_mb_group_set(CSET *cs,struct y_mb *mb,int count)
{
	CSET_GROUP_MB *g=cset_mb_group_new(cs,mb,count);
	if(cs->list && cs->list->type==CSET_TYPE_SENTENCE)
	{
		// 自动组句造的词以追加形式显示
		cset_prepend(cs,(CSET_GROUP*)g);
	}
	else
	{
		cset_append(cs,(CSET_GROUP*)g);
	}
	if(cs->assoc)
	{
		cset_apply_assoc(cs);
	}
}

void cset_array_item_free(CSET_ARRAY_ITEM *item)
{
	if(!item)
		return;
	y_mb_slice_free(L_CPTR(item->cand),item->cand_size);
	item->cand=l_cptr_null;
	item->cand_size=0;
	y_mb_slice_free(L_CPTR(item->codetip),item->codetip_size);
	item->codetip=l_cptr_null;
	item->codetip_size=0;
}

void cset_array_item_dedup(LArray *array)
{
	if(l_array_length(array)<=1)
		return;
	for(int i=0;i<l_array_length(array);)
	{
		CSET_ARRAY_ITEM *item=l_array_nth(array,i);
		DEDUP_ITEM *dedup_item=y_mb_slice_new(DEDUP_ITEM);
		dedup_item->cand=L_CPTR(item->cand);
		if(!l_hash_table_insert(dedup,dedup_item))
		{
			dedup_item_free(dedup_item);
			cset_array_item_free(item);
			l_array_remove(array,i);
		}
		else
		{
			i++;
		}
	}
	l_hash_table_clear(dedup,dedup_item_free);
}

static void cset_array_group_free(CSET_GROUP_ARRAY *g)
{
	if(!g)
		return;
	g->offset=0;
	g->count=0;
	l_array_clear(g->array,(LFreeFunc)cset_array_item_free);
}

static int cset_array_group_get(CSET_GROUP_ARRAY *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	assert(at+num<=g->count);
	for(int i=0;i<num;i++)
	{
		CSET_ARRAY_ITEM *item=l_array_nth(g->array,at+i);
		memcpy(cand[i],L_CPTR(item->cand),item->cand_size);
		memcpy(tip[i],L_CPTR(item->codetip),item->codetip_size);
	}
	return 0;
}

CSET_GROUP_ARRAY *cset_array_group_new(CSET *cs)
{
	CSET_GROUP_ARRAY *g=&cs->array;
	cset_remove(cs,(CSET_GROUP*)g);

	g->offset=0;
	g->count=0;
	g->type=CSET_TYPE_ARRAY;	
	g->get=(void *)cset_array_group_get;
	g->free=(void*)cset_array_group_free;
	if(!g->array)
		g->array=l_array_new(Y_MB_DATA_CALC,sizeof(CSET_ARRAY_ITEM));
	else
		l_array_clear(g->array,(LFreeFunc)cset_array_item_free);

	return g;
}

void cset_array_item_fill(CSET_ARRAY_ITEM *item,const void *cand,int cand_len,const void *codetip,int codetip_len)
{
	void *p=y_mb_slice_alloc(cand_len+1);
	l_memcpy0(p,cand,cand_len);
	item->cand=L_CPTR_T(p);
	item->cand_size=cand_len+1;
	if(codetip && codetip_len>0)
	{
		p=y_mb_slice_alloc(codetip_len+1);
		l_memcpy0(p,codetip,codetip_len);
		item->codetip=L_CPTR_T(p);
		item->codetip_size=codetip_len+1;
	}
	else
	{
		item->codetip=l_cptr_null;
		item->codetip_size=0;
	}
}

int cset_array_group_append2(CSET_GROUP_ARRAY *g,const char *cand,int cand_len,const char *codetip,int codetip_len)
{
	CSET_ARRAY_ITEM *item=l_array_append(g->array,NULL);
	cset_array_item_fill(item,cand,cand_len,codetip,codetip_len);
	g->count++;
	return 0;
}

int cset_array_group_append(CSET_GROUP_ARRAY *g,const char *cand,const char *codetip)
{
	CSET_ARRAY_ITEM *item=l_array_append(g->array,NULL);
	int cand_len=strlen(cand);
	int codetip_len=codetip?strlen(codetip):0;
	cset_array_item_fill(item,cand,cand_len,codetip,codetip_len);
	g->count++;
	return 0;
}

int cset_array_group_insert(CSET_GROUP_ARRAY *g,int n,const char *cand,const char *codetip)
{
	if(n<0 || n>=g->array->len)
		return cset_array_group_append(g,cand,codetip);
	CSET_ARRAY_ITEM item;
	int cand_len=strlen(cand);
	int codetip_len=codetip?strlen(codetip):0;
	cset_array_item_fill(&item,cand,cand_len,codetip,codetip_len);
	l_array_insert(g->array,n,&item);
	g->count++;
	return 0;
}

void cset_array_group_sort(CSET_GROUP_ARRAY *g,LCmpDataFunc cmp,void *arg)
{
	int i;
	for(i=0;i<g->array->len;i++)
	{
		CSET_ARRAY_ITEM *s=l_array_nth(g->array,i);
		s->index=i;
	}
	l_array_sort_r(g->array,cmp,arg);
}

int cset_set_assoc(CSET *cs,char CalcPhrase[][MAX_CAND_LEN+1],int count)
{
	LHashTable *assoc;
	if(cs->assoc_reset)
	{
		if(cs->assoc)
			l_hash_table_free(cs->assoc,(LFreeFunc)assoc_item_free);
		cs->assoc=cs->assoc_reset;
		cs->assoc_reset=NULL;
	}
	if(cs->assoc)
	{
		l_hash_table_clear(cs->assoc,(LFreeFunc)assoc_item_free);
		assoc=cs->assoc;
	}
	else
	{
		cs->assoc=assoc=L_HASH_TABLE_STRING(ASSOC_ITEM,phrase,0);
	}
	if(count<=0)
		return 0;
	int first_code=-1;
	for(int i=0;i<count;i++)
	{
		char *p=CalcPhrase[i];
		int len=gb_strbrk((const uint8_t*)p);
		for(int j=(gb_is_gb18030_ext((const uint8_t*)p)?4:2);j<=len;j+=(gb_is_gb18030_ext((const uint8_t*)p+j)?4:2))
		{
			if((cs->assoc_adjust==2 || p[len]) && j!=len)
				continue;
			ASSOC_ITEM *it=l_new(ASSOC_ITEM);
			it->phrase=l_strndup(p,j);
			it->code=NULL;
			it->index=i;
			if(j==len && p[len])
			{
				if(first_code==-1)
					first_code=i;
				it->code=l_strdup(p+len);
				p[len]=0;
				if(l_hash_table_find(assoc,it)!=NULL)
				{
					assoc_item_free(it);
					continue;
				}

				for(int k=1;;k++)
				{
					ASSOC_ITEM *it_code=l_new0(ASSOC_ITEM);
					it_code->phrase=l_strndup(it->code,k);
					if(!l_hash_table_insert(assoc,it_code))
						assoc_item_free(it_code);
					if(!it->code[k])
						break;
				}
			}
			if(!l_hash_table_insert(assoc,it))
				assoc_item_free(it);
		}
	}
	return first_code==-1?count:first_code;
}

int cset_has_assoc(CSET *cs,const char *code)
{
	if(!cs || !cs->assoc || !code || !l_hash_table_size(cs->assoc))
		return 0;
	ASSOC_ITEM *it=l_hash_table_lookup(cs->assoc,code);
	if(!it)
		return 0;
	return 1;
}

void *cset_get_group_by_type(CSET *cs,int type)
{
	CSET_GROUP *g;
	for(g=cs->list;g!=NULL;g=g->next)
	{
		if(g->type==type)
			return g;
	}
	return NULL;
}

static int cmp_with_assoc(const CSET_ARRAY_ITEM *s1,const CSET_ARRAY_ITEM *s2,CSET *cs)
{
	ASSOC_ITEM *it1=l_hash_table_lookup(cs->assoc,L_CPTR(s1->cand));
	if(it1 && !it1->code && cs->assoc_adjust_code)
		it1=NULL;
	if(it1 && it1->code && strcmp(it1->code,EIM.CodeInput))
		it1=NULL;
	ASSOC_ITEM *it2=l_hash_table_lookup(cs->assoc,L_CPTR(s2->cand));
	if(it2 && !it2->code && cs->assoc_adjust_code)
		it2=NULL;
	if(it2 && it2->code && strcmp(it2->code,EIM.CodeInput))
		it2=NULL;
	if(!it1)
	{
		if(!it2)
		{
			return s1->index-s2->index;
		}
		else
		{
			return 1;
		}
	}
	if(!it2)
	{
		return -1;
	}
	return it1->index-it2->index;
}

int cset_ci_first_cmp(const CSET_ARRAY_ITEM *s1,const CSET_ARRAY_ITEM *s2,void *unused)
{
	int l1=l_gb_strlen(y_mb_skip_display(L_CPTR(s1->cand),-1),-1);
	int l2=l_gb_strlen(y_mb_skip_display(L_CPTR(s2->cand),-1),-1);
	if((l1==1 && l2==1) || (l1>1 && l2>1))
		return s1->index-s2->index;
	return l2-l1;
}

void cset_reorder_mb_cand(CSET *cs,LCmpDataFunc cmp,void *data)
{
	CSET_GROUP_MB *gmb=(void*)cset_get_group_by_type(cs,CSET_TYPE_MB);
	if(!gmb)
	{
		return;
	}
	CSET_GROUP_ARRAY *ga=cset_array_group_new(cs);
	gmb->offset=0;
	char cand[10][MAX_CAND_LEN+1];
	char tip[10][MAX_TIPS_LEN+1];
	for(int i=0;i<gmb->count;)
	{
		int num=gmb->count-i;
		if(num>10)
			num=10;
		gmb->get((CSET_GROUP*)gmb,i,num,cand,tip);
		for(int j=0;j<num;j++)
		{
			if(tip[j][0])
				goto out;
			else
				cset_array_group_append(ga,cand[j],NULL);
		}
		i+=num;
	}
out:
	if(ga->count<=1)
	{
		ga->free((CSET_GROUP*)ga);
		cset_array_group_new(cs);
	}
	else
	{
		cset_array_group_sort(ga,cmp,data);
		cset_group_offset((CSET_GROUP*)gmb,ga->count);
		cs->list=l_slist_insert_before(cs->list,gmb,ga);
	}
}

void cset_apply_assoc(CSET *cs)
{
	if(!cs->assoc || l_hash_table_size(cs->assoc)==0)
	{
		return;
	}
	CSET_GROUP_ARRAY *ga=cset_array_group_new(cs);
	CSET_GROUP_MB *gmb=(void*)cset_get_group_by_type(cs,CSET_TYPE_MB);
	if(!gmb)
	{
		return;
	}
	gmb->offset=0;

	char cand[10][MAX_CAND_LEN+1];
	char tip[10][MAX_TIPS_LEN+1];

	// 找到当前完全匹配的候选放入ga中
	for(int i=0;i<gmb->count;)
	{
		int num=gmb->count-i;
		if(num>10)
			num=10;
		gmb->get((CSET_GROUP*)gmb,i,num,cand,tip);
		for(int j=0;j<num;j++)
		{
			if(tip[j][0])
			{
				goto out;
			}
			else
			{
				cset_array_group_append(ga,cand[j],NULL);
			}
		}
		i+=num;
	}
out:
	if(ga->count<=1)
	{
		ga->free((CSET_GROUP*)ga);
		ga=cset_array_group_new(cs);
	}
	else
	{
		cset_array_group_sort(ga,(LCmpDataFunc)cmp_with_assoc,cs);
		cset_group_offset((CSET_GROUP*)gmb,ga->count);
	}
	if(gmb && gmb->mb && cs->assoc_adjust_add!=0)
	{
		// 输入编码如果和联想词库编码相同，则把对应联想词加入候选
		const char *code=EIM.CodeInput;
		int code_len=strlen(code);
		int add=cs->assoc_adjust_add;
		LHashIter iter;
		int n=0;
		l_hash_iter_init(&iter,cs->assoc);
		while(1)
		{
			ASSOC_ITEM *it=l_hash_iter_next(&iter);
			if(!it)
				break;
			if(!it->code)
				continue;
			int len=strlen(it->code);
			if(add==-1 && len!=code_len && gmb->count>0)
				continue;
			if(add>1 && gmb->count>0 && code_len<add && len!=code_len)
				continue;
			if(!strncmp(it->code,code,code_len))
			{
				cset_array_group_insert(ga,n++,it->phrase,it->code+code_len);
			}

		}
		if(ga->count==0)
		{
			ga->free((CSET_GROUP*)ga);
			return;
		}
	}
	cs->list=l_slist_insert_before(cs->list,gmb,ga);
}

