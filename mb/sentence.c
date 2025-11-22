#include "llib.h"
#include "yong.h"
#include "mb.h"
#include "cset.h"
#include "gbk.h"

extern EXTRA_IM EIM;
typedef struct _item{
	struct _item *next;
	LList node;
	LSList index;
	char *code;
	char *cand;
}ITEM;

typedef struct _code_index{
	struct _code_index *next;
	LSList *head;
	LSList *tail;
	char code[4];
}CODE_INDEX;

typedef struct{
	CSET_GROUP;
	int code_len;
	LPtrArray *array;
}CSET_GROUP_SENTENCE;

typedef struct{
	char *file;
	uint8_t begin;
	uint8_t end;
	int count;
	uint32_t dirty;
	uint32_t flush;
	LQueue *que;
	LHashTable *cand_index;
	CODE_INDEX *code_index;
	CSET_GROUP_SENTENCE cset;	
}SENTENCE;

static SENTENCE *sentence;

static CODE_INDEX *code_index_get(const char *code,bool add)
{
	CODE_INDEX *p=sentence->code_index;
	for(;p!=NULL;p=p->next)
	{
		if(p->code[0]==code[0] && p->code[1]==code[1])
			return p;
	}
	if(!add)
		return NULL;
	p=l_new(CODE_INDEX);
	p->code[0]=code[0];
	p->code[1]=code[1];
	p->head=NULL;
	p->tail=NULL;
	sentence->code_index=l_slist_prepend(sentence->code_index,p);
	return p;
}

static int code_index_add_item(ITEM *p)
{
	CODE_INDEX *ci=code_index_get(p->code,true);
	l_slist_append(ci->tail,&p->index);
	ci->tail=&p->index;
	if(!ci->head)
	{
		ci->head=ci->tail;
	}
	return 0;
}

static int code_index_del_item(ITEM *p)
{
	CODE_INDEX *ci=code_index_get(p->code,false);
	if(!ci)
		return 0;
	ci->head=l_slist_remove(ci->head,&p->index);
	if(&p->index==ci->tail)
	{
		ci->tail=l_slist_last(ci->head);
	}
	return 0;
}

static LPtrArray *code_index_get_item(const char *code,LPtrArray *res,int count,struct y_mb *mb,char super)
{
	CODE_INDEX *ci=code_index_get(code,false);
	if(!ci)
		return NULL;
	int code_len=strlen(code);
	if(code_len<2)
		return NULL;
	for(LSList *p=ci->head;p!=NULL;p=p->next)
	{
		ITEM *it=container_of(p,ITEM,index);
		if(strncmp(code,it->code,code_len))
			continue;
		if(super && it->code[code_len]!='\0')
			continue;
		if(mb->compact)
		{
			if(strlen(it->code)>code_len+mb->compact-1)
				continue;
		}
		if(mb && super)
		{
			int cand_len=strlen(it->cand);
			if(cand_len>=4)
			{
				const uint8_t *s=(const uint8_t*)it->cand+cand_len-2;
				if(s[0]<=0xFE && s[0]>=0x81 && s[1]<=0x39 && s[1]>=0x30)
				{
					s-=2;
				}
				if(!y_mb_assist_test_hz(mb,(const char*)s,super))
					continue;
			}
		}
		l_ptr_array_append(res,it);
		if(count>0 && l_ptr_array_length(res)>=count)
			break;
	}
	if(l_ptr_array_length(res)==0)
	{
		return NULL;
	}
	return res;
}

static void item_free(ITEM *p)
{
	if(!p)
		return;
	l_hash_table_remove(sentence->cand_index,p);
	code_index_del_item(p);
	l_free(p->code);
	l_free(p->cand);
}

static void item_queue_free(void *n)
{
	ITEM *p=container_of(n,ITEM,node);
	item_free(p);
}

void sentence_save(void)
{
	if(!sentence)
		return;
	if(!sentence->dirty)
		return;
	LString *str=l_string_new(0x10000);
	LList *p=sentence->que->head;
	for(;p!=NULL;p=p->next)
	{
		ITEM *it=container_of(p,ITEM,node);
		l_string_append(str,it->code,-1);
		l_string_append_c(str,' ');
		l_string_append(str,it->cand,-1);
		l_string_append_c(str,'\n');
	}
	int ret=EIM.Callback(EIM_CALLBACK_ASYNC_WRITE_FILE,sentence->file,str,false);
	if(ret!=0)
	{
		FILE *fp=EIM.OpenFile(sentence->file,"wb");
		if(fp!=NULL)
		{
			fwrite(str->str,str->len,1,fp);
			fclose(fp);
		}
		l_string_free(str);
	}
	sentence->dirty=0;
}

int sentence_del(const char *cand)
{
	if(!sentence)
		return -1;
	ITEM *p=l_hash_table_lookup(sentence->cand_index,cand);
	if(!p)
		return -1;
	l_queue_remove(sentence->que,&p->node);
	item_free(p);
	sentence->dirty++;
	if(sentence->dirty>=sentence->flush)
	{
		sentence_save();
	}
	return 0;
}

int sentence_add(const char *code,const char *cand)
{
	if(!sentence)
		return -1;
	int ret=strlen(code);
	if(ret<=2)
		return -1;
	ret=l_gb_strlen((const uint8_t*)cand,-1);
	if(ret<sentence->begin || ret>sentence->end)
		return -1;
	ITEM *prev=l_hash_table_lookup(sentence->cand_index,cand);
	if(prev)
	{
		l_queue_remove(sentence->que,&prev->node);
		item_free(prev);
	}
	ITEM *p=l_new0(ITEM);
	p->code=l_strdup(code);
	p->cand=l_strdup(cand);
	l_queue_push_tail(sentence->que,&p->node);
	code_index_add_item(p);
	if(!l_hash_table_insert(sentence->cand_index,p))
	{
		l_queue_remove(sentence->que,&p->node);
		item_free(p);
	}
	if(l_queue_length(sentence->que)>sentence->count)
	{
		item_queue_free(l_queue_pop_head(sentence->que));
	}
	sentence->dirty++;
	if(sentence->dirty>=sentence->flush)
		sentence_save();
	return 0;
}

CSET_GROUP *sentence_get(const char *code,struct y_mb *mb,char super)
{
	if(!sentence)
		return NULL;
	LPtrArray *arr=sentence->cset.array;
	l_ptr_array_clear(arr,NULL);
	code_index_get_item(code,arr,10,mb,super);
	if(l_ptr_array_length(arr)==0)
		return NULL;
	sentence->cset.count=l_ptr_array_length(arr);
	sentence->cset.code_len=strlen(code);
	return (CSET_GROUP*)&sentence->cset;
}

static int sentence_load(void)
{
	FILE *fp;
	if(!sentence || !sentence->file)
		return -1;
	fp=EIM.OpenFile(sentence->file,"rb");
	if(!fp)
		return 0;
	while(!feof(fp))
	{
		char line[1024];
		int ret=l_get_line(line,sizeof(line),fp);
		if(ret<=0)
			continue;
		char code[Y_MB_KEY_SIZE+1];
		char cand[Y_MB_DATA_SIZE+1];
		ret=l_sscanf(line,"%63s %255s",code,cand);
		if(ret!=2)
			continue;
		sentence_add(code,cand);
	}
	fclose(fp);
	return 0;
}

static void cset_sentence_group_free(CSET_GROUP_SENTENCE *g)
{
	l_ptr_array_clear(g->array,NULL);
}

static void cset_sentence_group_get(CSET_GROUP_SENTENCE *g,int at,int num,char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	assert(at+num<=g->count);
	for(int i=0;i<num;i++)
	{
		ITEM *item=l_ptr_array_nth(g->array,at+i);
		strcpy(cand[i],item->cand);
		strcpy(tip[i],item->code+g->code_len);
	}
}

int sentence_init(void)
{
	if(sentence)
		return 0;
	const char *config=EIM.GetConfig(NULL,"auto_sentence");
	if(!config || !config[0])
		return -1;
	char file[128];
	int begin,end,count;
	int ret=l_sscanf(config,"%d %d %s %d",&begin,&end,file,&count);
	if(ret!=4)
		return -1;
	if(begin<2 || begin>20 || end<begin || end>20 || count<=0)
		return -1;
	sentence=l_new0(SENTENCE);
	sentence->begin=begin;
	sentence->end=end;
	sentence->count=count;
	sentence->flush=10;
	sentence->file=l_strdup(file);
	sentence->cand_index=L_HASH_TABLE_STRING(ITEM,cand,0);
	sentence->que=l_queue_new((LFreeFunc)item_queue_free);
	sentence->cset=(CSET_GROUP_SENTENCE){
		.type=CSET_TYPE_SENTENCE,
		.get=(void*)cset_sentence_group_get,
		.free=(void*)cset_sentence_group_free,
		.array=l_ptr_array_new(10)
	};
	ret=sentence_load();
	sentence->dirty=0;
	return ret;
}

void sentence_destroy(void)
{
	if(!sentence)
		return;
	sentence_save();
	l_ptr_array_free(sentence->cset.array,NULL);
	l_queue_free(sentence->que);
	l_free(sentence);
	sentence=NULL;
}
