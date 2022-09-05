#include <llib.h>
#include <string.h>
#include "cset.h"

typedef struct _assoc_item{
	struct _assoc_item *next;
	char *phrase;
	int index;
}ASSOC_ITEM;

static void assoc_item_free(ASSOC_ITEM *p)
{
	if(!p)
		return;
	free(p->phrase);
	free(p);
}

void cset_init(CSET *cs)
{
	memset(cs,0,sizeof(*cs));
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
	if(g->count)
	{
		cset_append(cs,(CSET_GROUP*)g);
		if(cs->assoc)
		{
			cset_apply_assoc(cs);
		}
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

void cset_array_group_sort(CSET_GROUP_ARRAY *g,LCmpDataFunc cmp,void *arg)
{
	l_array_sort_r(g->array,cmp,arg);
}

void cset_set_assoc(CSET *cs,char CalcPhrase[][MAX_CAND_LEN+1],int count)
{
	if(cs->assoc)
	{
		l_hash_table_free(cs->assoc,(LFreeFunc)assoc_item_free);
		cs->assoc=0;
	}
	if(count<=0)
		return;
	LHashTable *assoc=L_HASH_TABLE_STRING(ASSOC_ITEM,phrase);
	int i,j;
	for(i=0;i<count;i++)
	{
		char *p=CalcPhrase[i];
		int len=strlen(p);
		if((len&0x01))
			continue;
		for(j=2;j<=len;j+=2)
		{
			ASSOC_ITEM *it=l_new(ASSOC_ITEM);
			it->phrase=l_strndup(p,j);
			it->index=i;
			if(!l_hash_table_insert(assoc,it))
				assoc_item_free(it);
		}
	}
	cs->assoc=assoc;
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
			if(s1>s2)
				return 1;
			else if(s1<s2)
				return -1;
			return 0;
		}
		else
			return 1;
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
		return;
	}
	cset_array_group_sort(ga,(LCmpDataFunc)cmp_with_assoc,cs->assoc);
	cs->list=l_slist_insert_before(cs->list,gmb,ga);
	cset_group_offset((CSET_GROUP*)gmb,ga->count);
}

