#include <llib.h>

#include "fuzzy.h"
#include "pinyin.h"

#ifndef FUZZY_TEST
#include "mb.h"
#endif

L_HASH_STRING(fuzzy,FUZZY_ITEM,from);

// 递归添加模糊码，把to的目标编码添加到item的目标中去
static void fuzzy_recursive(FUZZY_TABLE *ft,FUZZY_ITEM *item,const char *to)
{
	FUZZY_ITEM key,*next;
	int i,j;
	strcpy(key.from,to);
	next=l_hash_table_find(ft,&key);
	if(!next)
	{
		// 现在的to没有模糊，所以直接返回
		return;
	}
	for(i=0;i<4;i++)
	{
		FUZZY_TO *pto=next->to+i;
		const char *s=pto->code;
		if(!s[0])
		{
			// to对应的模糊码查找完毕，可以退出循环了
			break;
		}
		// 编码相同的已经存在，继续下一项
		if(!strcmp(s,item->from))
		{
			continue;
		}
		// 找一个空的位置进行添加
		for(j=0;j<4;j++)
		{
			char *t=item->to[j].code;
			if(!t[0])
			{
				strcpy(t,s);
				break;
			}
			if(!strcmp(s,t)) break;
		}
	}
}

static void fuzzy_table_insert(FUZZY_TABLE *ft,const char *from,const char *to,int mode)
{
	FUZZY_ITEM *res;
	FUZZY_ITEM *item;
	int i;
	if(!strcmp(from,to))
		return;
	item=l_new0(FUZZY_ITEM);
	strcpy(item->from,from);
	res=l_hash_table_find(ft,item);
	if(res)
	{
		for(i=0;i<4;i++)
		{
			FUZZY_TO *pto=res->to+i;
			if(pto->code[0]==0)
			{
				strcpy(pto->code,to);
				pto->mode=mode;
				break;
			}
			if(!strcmp(pto->code,to))
			{
				if(mode)
					pto->mode=mode;
				break;
			}
		}
		if(i!=4) fuzzy_recursive(ft,res,to);
		l_free(item);
	}
	else
	{
		item->next=NULL;
		strcpy(item->to[0].code,to);
		item->to[0].mode=mode;
		l_hash_table_insert(ft,item);
		fuzzy_recursive(ft,item,to);
	}
}

FUZZY_TABLE *fuzzy_table_load(const char *file)
{
	FILE *fp;
	LHashTable *ft;
	char line[1024];
	char **prefix=NULL;
	char **suffix=NULL;
	
	if(!file || !file[0])
		return NULL;
	
#ifdef FUZZY_TEST
	fp=fopen(file,"rb");
#else
	fp=y_mb_open_file(file,"rb");
#endif
	if(!fp)
		return NULL;
	ft=l_hash_table_new(401,(LHashFunc)fuzzy_hash,(LCmpFunc)fuzzy_cmp);
	while(l_get_line(line,sizeof(line),fp)>=0)
	{
		if(line[0]=='#') continue;
		if(line[0]=='^' && line[1]=='=')
		{
			if(prefix) l_strfreev(prefix);
			prefix=l_strsplit(line+2,' ');
		}
		else if(line[0]=='$' && line[1]=='=')
		{
			if(suffix) l_strfreev(suffix);
			suffix=l_strsplit(line+2,' ');
		}
		else
		{
			char from[8],to[8],op;
			int ret;
			int from_len,to_len;
			int mode=FUZZY_DEFAULT;
			ret=l_sscanf(line,"%8[^>=<]%c%8s",from,&op,to);
			if(ret!=3) continue;
			if(op=='>') mode=FUZZY_FORCE;
			else if(op=='<') mode=FUZZY_CORRECT;
			from_len=strlen(from);to_len=strlen(to);
			if(from[0]=='*')
			{
				int i;
				if(!prefix) continue;
				if(from[from_len-1]=='*') continue;
				if(to[0]!='*') continue;
				if(to[to_len-1]=='*') continue;
				for(i=0;prefix[i]!=NULL;i++)
				{
					char rfrom[16],rto[16];
					ret=snprintf(rfrom,sizeof(rfrom),"%s%s",prefix[i],from+1);
					if(ret>=8) continue;
					ret=snprintf(rto,sizeof(rto),"%s%s",prefix[i],to+1);
					if(ret>=8) continue;
					fuzzy_table_insert(ft,rfrom,rto,mode);
					if(op!='<')
						fuzzy_table_insert(ft,rto,rfrom,0);
				}
			}
			else if(from[from_len-1]=='*')
			{
				int i;
				if(!suffix) continue;
				if(to[to_len-1]!='*') continue;
				if(to[0]=='*') continue;
				from[from_len-1]=0;
				to[to_len-1]=0;
				for(i=0;prefix[i]!=NULL;i++)
				{
					char rfrom[16],rto[16];
					ret=snprintf(rfrom,sizeof(rfrom),"%s%s",from,suffix[i]);
					if(ret>=8) continue;
					ret=snprintf(rto,sizeof(rto),"%s%s",to,suffix[i]);
					if(ret>=8) continue;
					fuzzy_table_insert(ft,rfrom,rto,mode);
					if(op!='<')
						fuzzy_table_insert(ft,rto,rfrom,0);
				}
			}
			else
			{
				fuzzy_table_insert(ft,from,to,mode);
				if(op!='<')
					fuzzy_table_insert(ft,to,from,0);
			}
		}
	}
	fclose(fp);
	l_strfreev(prefix);
	l_strfreev(suffix);
	if(l_hash_table_size(ft)<=0)
	{
		l_hash_table_free(ft,l_free);
		return NULL;
	}
	return ft;
}

void fuzzy_table_free(FUZZY_TABLE *ft)
{
	l_hash_table_free(ft,l_free);
}

#ifdef FUZZY_TEST
void fuzzy_table_dump(FUZZY_TABLE *ft)
{
	LHashIter iter;
	
	l_hash_iter_init(&iter,ft);
	while(!l_hash_iter_next(&iter))
	{
		FUZZY_ITEM *item=l_hash_iter_data(&iter);
		int i;
		printf("%s",item->from);
		for(i=0;i<4;i++)
		{
			FUZZY_TO *pto=item->to+i;
			if(!pto->code[0]) break;
			if(pto->mode==FUZZY_DEFAULT)
				printf(" >%s",pto->code);
			else if(pto->mode==FUZZY_FORCE)
				printf(" >>%s",pto->code);
			else
				printf(" <%s",pto->code);
		}
		printf("\n");
	}
}
#endif

FUZZY_ITEM *fuzzy_table_lookup(FUZZY_TABLE *ft,const char *code)
{
	FUZZY_ITEM key,*res;
	int len=strlen(code);
	if(len>=8) return NULL;
	strcpy(key.from,code);
	res=l_hash_table_find(ft,&key);
	return res;
}

typedef struct{
	FUZZY_TABLE *ft;
	LArray *list;
	int split;
}FUZZY_LIST;

static void fuzzy_key_add(LArray *list,const char *code)
{
	int i;
	for(i=0;i<list->len;i++)
	{
		if(!strcmp(l_ptr_array_nth(list,i),code))
			return;
	}
	l_ptr_array_append(list,l_strdup(code));
}

static void fuzzy_enum_key(FUZZY_LIST *fl,const char *prev,py_item_t *input,int count,const char *tail)
{
	char code[8];
	FUZZY_ITEM *it;
	int i;
	char *me;
	py_build_string(code,input,1);
	if(count>1)
	{
		me=l_sprintf("%s%s",prev,code);
		fuzzy_enum_key(fl,me,input+1,count-1,tail);
	}
	else
	{
		me=l_sprintf("%s%s%s",prev,code,tail);
		fuzzy_key_add(fl->list,me);
	}
	l_free(me);
	it=fuzzy_table_lookup(fl->ft,code);
	if(it!=NULL) for(i=0;i<4;i++)
	{
		FUZZY_TO *to=it->to+i;
		if(to->code[0]==0)
			continue;
		if(!py_is_valid_code(to->code))
			continue;
		if(count>1)
		{
			me=l_sprintf("%s%s",prev,to->code);
			fuzzy_enum_key(fl,me,input+1,count-1,tail);
		}
		else
		{
			me=l_sprintf("%s%s%s",prev,to->code,tail);
			fuzzy_key_add(fl->list,me);
		}
		l_free(me);
	}
}

LArray *fuzzy_key_list(FUZZY_TABLE *ft,const char *code,int len,int split)
{
	LArray *list;
	FUZZY_ITEM *it;
	py_item_t input[PY_MAX_TOKEN];
	int count;
	FUZZY_LIST fl;
	char tail[128];
	list=l_ptr_array_new(4);
	if(len<0)
		l_ptr_array_append(list,l_strdup(code));
	else
		l_ptr_array_append(list,l_strndup(code,len));
	it=fuzzy_table_lookup(ft,code);
	if(it!=NULL)
	{
		int i;
		for(i=0;i<4;i++)
		{
			FUZZY_TO *to=it->to+i;
			if(to->code[0]==0)
				continue;
			if(to->mode!=FUZZY_DEFAULT)
				continue;
			fuzzy_key_add(list,to->code);
		}
		return list;
	}
	if(split==0 || split==1)
		return list;
	count=py_parse_string(l_ptr_array_nth(list,0),input,-1,NULL,NULL);
	count=py_remove_split(input,count);
	if(count==0)
		return list;
	fl.list=list;
	fl.ft=ft;
	fl.split=split;
	if(count<=7)
	{
		tail[0]=0;
	}
	else
	{
		py_build_string(tail,input+7,count-7);
		py_prepare_string(tail,tail,0);
		count=7;
	}
	fuzzy_enum_key(&fl,"",input,count,tail);
	return list;
}

#ifndef Y_MB_KEY_SIZE
#define Y_MB_KEY_SIZE	63
#endif

int fuzzy_correct(FUZZY_TABLE *ft,char *s,int len)
{
	if(!ft)
		return len;
	if(!s || len<0)
		return -1;

	LHashIter iter;
	
	l_hash_iter_init(&iter,ft);
	while(!l_hash_iter_next(&iter))
	{
		FUZZY_ITEM *item=l_hash_iter_data(&iter);
		int i;
		for(i=0;i<4;i++)
		{
			FUZZY_TO *pto=item->to+i;
			if(!pto->code[0]) break;
			if(pto->mode!=FUZZY_CORRECT)
				continue;
			char *p;
			int pos=0;
			int from_len=strlen(item->from);
			while((p=strstr(s+pos,item->from))!=NULL)
			{
				int to_len=strlen(pto->code);
				if(from_len<to_len && pos+strlen(p+pos)+to_len-from_len>=Y_MB_KEY_SIZE)
					break;
				memmove(p+to_len,p+from_len,strlen(p+from_len)+1);
				memcpy(p+pos,pto->code,to_len);
				pos+=to_len;
			}
		}
	}

	return len;
}

#ifdef FUZZY_TEST

void list_key(FUZZY_TABLE *ft,const char *code,int split)
{
	LArray *list;
	int i;
	list=fuzzy_key_list(ft,code,-1,split);
	
	for(i=0;i<list->len;i++)
	{
		printf("%s\n",(char*)l_ptr_array_nth(list,i));
	}
	
	l_ptr_array_free(list,l_free);
}

//gcc fuzzy.c -g -Wall -O0 ../common/pinyin.c ../common/trie.c -DFUZZY_TEST -I../llib -I../include -I../common -L../llib/l64 -ll -lm

int main(void)
{
	FUZZY_TABLE *ft;
	ft=fuzzy_table_load("fuzzy.txt");
	//fuzzy_table_dump(ft);
	
	py_init('\'',NULL);
	//list_key(ft,"a's'den'a's'dei'f'da's'den'a's'den'e'da's'den'sa'dei'f'e'za'sen'za's",'\'');
	list_key(ft,"shenme",'\'');

	fuzzy_table_free(ft);
	return 0;
}
#endif
