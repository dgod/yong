#include <llib.h>
#include <string.h>
#include "cset.h"
#include "gbk.h"

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
		}
	}
	cs->calc.phrase=l_calloc(cs->calc.size,MAX_CAND_LEN+1);
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

	l_hash_table_free(cs->assoc,(LFreeFunc)assoc_item_free);
	cs->assoc=NULL;
}

void cset_destroy(CSET *cs)
{
	cset_reset(cs);
	l_array_free(cs->array.array,NULL);
	cs->array.array=NULL;
	l_free(cs->calc.phrase);
	cs->calc.phrase=NULL;
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
	CSET_GROUP *g=cs->list;
	int total=0;
	for(;g!=NULL;g=g->next)
	{
		total+=g->count-g->offset;
	}
	return total;
}

void cset_clear(CSET *cs,int type)
{
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
		fprintf(stdout,"%s\n",temp);
#endif
	}
	fprintf(fp,"\n");
}

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
	int i;
	for(i=0;i<num;i++)
	{
		const char *s=y_mb_predict_nth(g->phrase,at+i);
		assert(s!=NULL);
		strcpy(cand[i],s);
		tip[i][0]=0;
	}
	return 0;
}

CSET_GROUP_PREDICT *cset_predict_group_new(CSET *cs)
{
	CSET_GROUP_PREDICT *g=&cs->predict;
	cset_remove(cs,(CSET_GROUP*)g);

	g->offset=0;

	g->type=CSET_TYPE_PREDICT;
	g->get=(void*)cset_predict_group_get;
	g->free=NULL;

	g->phrase[0]=0;
	g->count=0;
	return g;
}

int cset_predict_group_count(CSET *cs)
{
	return cs->predict.count;
}

static int cset_calc_group_get(CSET_GROUP_CALC *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	int i;
	assert(at+num<=g->count);
	for(i=0;i<num;i++)
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

int cset_calc_group_append(CSET_GROUP_CALC *g,const char *s)
{
	CSET_GROUP_CALC *c=(CSET_GROUP_CALC*)g;
	if(c->count>=Y_MB_DATA_CALC)
		return -1;
	strcpy(c->phrase[c->count],s);
	c->count++;
	return 0;
}

static int cset_mb_group_get(CSET_GROUP_MB *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	struct y_mb *mb=g->mb;
	int ret=y_mb_get(mb,at,num,cand,tip);
	return ret;
}

CSET_GROUP_MB *cset_mb_group_new(CSET *cs,struct y_mb *mb,int count)
{
	CSET_GROUP_MB *g=&cs->mb;
	cset_remove(cs,(CSET_GROUP*)g);

	g->offset=0;

	g->type=CSET_TYPE_MB;
	g->get=(void*)cset_mb_group_get;
	g->free=NULL;

	g->count=count;
	g->mb=mb;

	return g;
}

void cset_mb_group_set(CSET *cs,struct y_mb *mb,int count)
{
	CSET_GROUP_MB *g=cset_mb_group_new(cs,mb,count);
	cset_append(cs,(CSET_GROUP*)g);
	if(cs->assoc)
	{
		cset_apply_assoc(cs);
	}
}

static void cset_group_array_item_free(CSET_GROUP_ARRAY_ITEM *item)
{
	if(!item)
		return;
	l_free(item->cand);
	l_free(item->codetip);
}

static void cset_array_group_free(CSET_GROUP_ARRAY *g)
{
	if(!g)
		return;
	g->offset=0;
	g->count=0;
	l_array_clear(g->array,(LFreeFunc)cset_group_array_item_free);
}

static int cset_array_group_get(CSET_GROUP_ARRAY *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	int i;
	assert(at+num<=g->count);
	for(i=0;i<num;i++)
	{
		CSET_GROUP_ARRAY_ITEM *item=l_array_nth(g->array,at+i);
		strcpy(cand[i],item->cand);
		if(item->codetip)
			strcpy(tip[i],item->codetip);
		else
			tip[i][0]=0;
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
		g->array=l_array_new(Y_MB_DATA_CALC,sizeof(CSET_GROUP_ARRAY_ITEM));
	else
		l_array_clear(g->array,(LFreeFunc)cset_group_array_item_free);

	return g;
}

int cset_array_group_append(CSET_GROUP_ARRAY *g,const char *cand,const char *codetip)
{
	CSET_GROUP_ARRAY_ITEM item;
	item.cand=l_strdup(cand);
	item.codetip=codetip?l_strdup(codetip):NULL;
	l_array_append(g->array,&item);
	g->count++;
	return 0;
}

static int cset_array_group_insert(CSET_GROUP_ARRAY *g,int n,const char *cand,const char *codetip)
{
	CSET_GROUP_ARRAY_ITEM item;
	item.cand=l_strdup(cand);
	item.codetip=codetip?l_strdup(codetip):NULL;
	l_array_insert(g->array,n,&item);
	g->count++;
	return 0;
}

void cset_array_group_sort(CSET_GROUP_ARRAY *g,LCmpDataFunc cmp,void *arg)
{
	int i;
	for(i=0;i<g->array->len;i++)
	{
		CSET_GROUP_ARRAY_ITEM *s=l_array_nth(g->array,i);
		s->index=i;
	}
	l_array_sort_r(g->array,cmp,arg);
}

void cset_set_assoc(CSET *cs,char CalcPhrase[][MAX_CAND_LEN+1],int count)
{
	if(cs->assoc)
	{
		l_hash_table_free(cs->assoc,(LFreeFunc)assoc_item_free);
		cs->assoc=NULL;
	}
	if(count<=0)
		return;
	LHashTable *assoc=L_HASH_TABLE_STRING(ASSOC_ITEM,phrase);
	int i,j;
	for(i=0;i<count;i++)
	{
		char *p=CalcPhrase[i];
		int len;
		for(len=0;p[len]!=0;)
		{
			if(gb_is_gb18030_ext((const uint8_t*)(p+len)))
			{
				len+=4;
			}
			else if(((uint8_t*)p)[len]<0x80)
			{
				break;
			}
			else
			{
				len+=2;
			}
		}
		for(j=(gb_is_gb18030_ext((const uint8_t*)p)?4:2);j<=len;j+=(gb_is_gb18030_ext((const uint8_t*)p+j)?4:2))
		{
			if(cs->assoc_adjust==2 && j!=len)
				continue;
			ASSOC_ITEM *it=l_new(ASSOC_ITEM);
			it->phrase=l_strndup(p,j);
			it->code=NULL;
			it->index=i;
			if(j==len && p[len])
			{
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
	cs->assoc=assoc;
}

int cset_has_assoc(CSET *cs,const char *code)
{
	if(!cs || !cs->assoc || !code)
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

static int cmp_with_assoc(const CSET_GROUP_ARRAY_ITEM *s1,const CSET_GROUP_ARRAY_ITEM *s2,LHashTable *assoc)
{
	ASSOC_ITEM *it1=l_hash_table_lookup(assoc,(void*)s1->cand);
	ASSOC_ITEM *it2=l_hash_table_lookup(assoc,(void*)s2->cand);
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
	int i,j;

	for(i=0;i<gmb->count;)
	{
		int num=gmb->count-i;
		if(num>10)
			num=10;
		gmb->get((CSET_GROUP*)gmb,i,num,cand,tip);
		for(j=0;j<num;j++)
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
		cset_array_group_sort(ga,(LCmpDataFunc)cmp_with_assoc,cs->assoc);
		cset_group_offset((CSET_GROUP*)gmb,ga->count);
	}
	if(gmb && gmb->mb && cs->assoc_adjust_add!=0)
	{
		const char *code=EIM.CodeInput;
		int code_len=strlen(code);
		int add=cs->assoc_adjust_add;
		LHashIter iter;
		int n=0;
		l_hash_iter_init(&iter,cs->assoc);
		while(!l_hash_iter_next(&iter))
		{
			ASSOC_ITEM *it=l_hash_iter_data(&iter);
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

