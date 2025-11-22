#include "correct.h"
#include "yong.h"
#include "mb.h"

extern EXTRA_IM EIM;
static LHashTable *corr;

typedef struct{
	void *next;
	char code[8];
}CORRECT_TARGET;

typedef struct{
	void *next;
	char code[8];
	CORRECT_TARGET *target;
}CORRECT_ITEM;

bool correct_enabled(void)
{
	return corr?true:false;
}

static void correct_item_free(CORRECT_ITEM *p)
{
	if(!p)
		return;
	l_slist_free(p->target,l_free);
	l_free(p);
}

int correct_init(void)
{
	const char *file=EIM.GetConfig(NULL,"correct");
	if(!file || !file[0])
		return -1;
	FILE *fp=EIM.OpenFile(file,"r");
	if(!fp)
		return -1;
	corr=L_HASH_TABLE_STRING(CORRECT_ITEM,code,17);
	while(!feof(fp))
	{
		char line[256];
		int len=l_get_line(line,sizeof(line),fp);
		if(len<0)
			break;
		if(len==0 || line[0]=='#')
			continue;
		char *arr[5];
		int count=l_strtok0(line,' ',arr,5);
		if(count<=1)
			break;
		len=strlen(arr[0]);
		if(len<1 || len>7)
			break;
		CORRECT_ITEM *item=l_hash_table_lookup(corr,arr[0]);
		if(!item)
		{
			item=l_new(CORRECT_ITEM);
			item->target=NULL;
			strcpy(item->code,arr[0]);
			l_hash_table_insert(corr,item);
		}
		for(int i=1;i<count;i++)
		{
			int tlen=strlen(arr[i]);
			if(tlen<1 || tlen>len)
				continue;
			CORRECT_TARGET *target=l_new(CORRECT_TARGET);
			strcpy(target->code,arr[i]);
			item->target=l_slist_prepend(item->target,target);
		}
		if(!item->target)
		{
			l_hash_table_remove(corr,item);
			correct_item_free(item);
		}
	}
	fclose(fp);
	return 0;
}

void correct_destroy(void)
{
	l_hash_table_free(corr,(LFreeFunc)correct_item_free);
}

bool correct_run(char *s,struct y_mb *mb,int filter,int *count)
{
	if(!corr)
		return false;
	int len=strlen(s);
	if(len<2)
		return false;
	int pos=len>7?len-7:0;
	while(pos<=len-1)
	{
		CORRECT_ITEM *item=l_hash_table_lookup(corr,s+pos);
		if(!item)
		{
			pos++;
			continue;
		}
		char temp[len+1];
		strcpy(temp,s);
		for(CORRECT_TARGET *t=item->target;t!=NULL;t=t->next)
		{
			if(!mb)
			{
				strcpy(s+pos,t->code);
				if(count)
					*count=0;
				return true;
			}
			strcpy(temp+pos,t->code);
			int ret=y_mb_set(mb,temp,strlen(temp),filter);
			if(ret>0)
			{
				strcpy(s,temp);
				if(count)
					*count=ret;
				return true;
			}
		}
		pos++;
	}
	return false;
}

