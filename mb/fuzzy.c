#include <llib.h>

#include "fuzzy.h"
#include "pinyin.h"

#ifndef FUZZY_TEST
#include "mb.h"
#endif

// 递归添加模糊码，把to的目标编码添加到item的目标中去
static void fuzzy_recursive(FUZZY_TABLE *ft,FUZZY_ITEM *item,const char *to)
{
	FUZZY_ITEM *next=l_hash_table_lookup(ft,to);
	if(!next)
	{
		// 现在的to没有模糊，所以直接返回
		return;
	}
	for(int i=0;i<FUZZY_TO_MAX;i++)
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
		for(int j=0;j<FUZZY_TO_MAX;j++)
		{
			char *t=item->to[j].code;
			if(!t[0])
			{
				int len=strlen(s);
				memcpy(t,s,len+1);
				item->to[j].len=(uint8_t)len;
				break;
			}
			if(!strcmp(s,t))
				break;
		}
	}
}

static void fuzzy_table_insert(FUZZY_TABLE *ft,const char *from,const char *to,bool valid)
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
		for(i=0;i<FUZZY_TO_MAX;i++)
		{
			FUZZY_TO *pto=res->to+i;
			if(pto->code[0]==0)
			{
				strcpy(pto->code,to);
				pto->valid=valid;
				pto->len=(uint8_t)strlen(to);
				break;
			}
			if(!strcmp(pto->code,to))
			{
				break;
			}
		}
		if(i!=FUZZY_TO_MAX)
			fuzzy_recursive(ft,res,to);
		l_free(item);
	}
	else
	{
		item->next=NULL;
		strcpy(item->to[0].code,to);
		item->to[0].valid=valid;
		item->to[0].len=strlen(to);
		l_hash_table_insert(ft,item);
		fuzzy_recursive(ft,item,to);
	}
}

static bool is_valid_target(struct y_mb *mb,const char *s)
{
	if(!mb || !mb->pinyin || mb->split!='\'')
		return true;
	return py2_is_valid_code(s);
}

FUZZY_TABLE *fuzzy_table_load(const char *file,struct y_mb *mb)
{
	char line[1024];
	char **prefix=NULL;
	char **suffix=NULL;
	
	if(!file || !file[0])
		return NULL;
	
#ifdef FUZZY_TEST
	FILE *fp=fopen(file,"rb");
#else
	FILE *fp=y_mb_open_file(file,"rb");
#endif
	if(!fp)
		return NULL;
	LHashTable *ft=L_HASH_TABLE_STRING(FUZZY_ITEM,from,401);
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
			ret=l_sscanf(line,"%7[^>=]%c%7s",from,&op,to);
			if(ret!=3) continue;
			if(op=='>') mode=FUZZY_FORCE;
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
					if(!is_valid_target(mb,rfrom)) continue;
					if(!is_valid_target(mb,rto)) continue;
					fuzzy_table_insert(ft,rfrom,rto,true);
					if(mode==FUZZY_DEFAULT)
						fuzzy_table_insert(ft,rto,rfrom,true);
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
				for(i=0;suffix[i]!=NULL;i++)
				{
					char rfrom[16],rto[16];
					ret=snprintf(rfrom,sizeof(rfrom),"%s%s",from,suffix[i]);
					if(ret>=8) continue;
					ret=snprintf(rto,sizeof(rto),"%s%s",to,suffix[i]);
					if(ret>=8) continue;
					if(!is_valid_target(mb,rfrom)) continue;
					if(!is_valid_target(mb,rto)) continue;
					fuzzy_table_insert(ft,rfrom,rto,true);
					if(mode==FUZZY_DEFAULT)
						fuzzy_table_insert(ft,rto,rfrom,true);
				}
			}
			else
			{
				bool valid=is_valid_target(mb,to);
				fuzzy_table_insert(ft,from,to,valid);
				if(mode==FUZZY_DEFAULT)
				{
					valid=is_valid_target(mb,from);
					fuzzy_table_insert(ft,to,from,valid);
				}
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
	while(true)
	{
		FUZZY_ITEM *item=l_hash_iter_next(&iter);
		if(!item)
			break;
		int i;
		printf("%s",item->from);
		for(i=0;i<4;i++)
		{
			FUZZY_TO *pto=item->to+i;
			if(!pto->code[0]) break;
			printf(" >%s",pto->code);
		}
		printf("\n");
	}
}
#endif

FUZZY_ITEM *fuzzy_table_lookup(FUZZY_TABLE *ft,const char *code)
{
	return l_hash_table_lookup(ft,code);
}

typedef struct{
	FUZZY_TABLE *ft;
	LPtrArray *list;
}FUZZY_LIST;

static inline void fuzzy_key_add(LArray *list,const char *code,int code_len)
{
	l_ptr_array_append(list,l_memdup(code,code_len+1));
}

static void fuzzy_enum_key(FUZZY_LIST *fl,const char *prev,int prev_len,py_item_t *input,int count)
{
	char code[8];
	FUZZY_ITEM *it;
	char me[128];
	int code_len=py2_build_string_no_split(code,input,1);
	int me_len=prev_len+code_len;
	memcpy(me,prev,prev_len);
	memcpy(me+prev_len,code,code_len+1);
	if(count>1)
	{
		fuzzy_enum_key(fl,me,me_len,input+1,count-1);
	}
	else
	{
		fuzzy_key_add(fl->list,me,me_len);
	}
	it=fuzzy_table_lookup(fl->ft,code);
	if(it!=NULL) for(int i=0;i<4;i++)
	{
		FUZZY_TO *to=it->to+i;
		if(to->code[0]==0)
			break;
		if(!to->valid)
			continue;
		me_len=prev_len+to->len;
		memcpy(me,prev,prev_len);
		memcpy(me+prev_len,to->code,to->len+1);
		if(count>1)
		{
			fuzzy_enum_key(fl,me,me_len,input+1,count-1);
		}
		else
		{
			fuzzy_key_add(fl->list,me,me_len);
		}
	}
}

int fuzzy_key_list_py(FUZZY_TABLE *ft,py_item_t *input,int count,LPtrArray *list)
{
        FUZZY_LIST fl;
        fl.list=list;
        fl.ft=ft;
        fuzzy_enum_key(&fl,"",0,input,count);
        return list->len;
}

int fuzzy_key_list_simple(FUZZY_TABLE *ft,const char *code,LPtrArray *list)
{
        FUZZY_ITEM *it;
        it=fuzzy_table_lookup(ft,code);
        if(it!=NULL)
        {
                for(int i=0;i<FUZZY_TO_MAX;i++)
                {
                        FUZZY_TO *to=it->to+i;
                        if(to->code[0]==0)
                                break;
                        fuzzy_key_add(list,to->code,to->len);
                }
                return list->len;
        }
        return list->len;
}

int fuzzy_key_list2(FUZZY_TABLE *ft,const char *code,int split,LPtrArray *list)
{
        fuzzy_key_list_simple(ft,code,list);
        if(list->len)
                return list->len;
        if(split<2)
                return 0;

        py_item_t input[PY_MAX_TOKEN];
        int count=py2_parse_string(code,input,NULL,NULL);
        count=py2_remove_split(input,count);
        if(count==0)
                return 0;
        return fuzzy_key_list_py(ft,input,count,list);
}

#ifdef FUZZY_TEST

// gcc fuzzy.c -g -Wall -O0 ../common/pinyin.c ../common/trie.c -DFUZZY_TEST -I../llib -I../include -I../common -L../llib/l64 -ll -lm -Wl,-rpath='$ORIGIN'

int main(int argc,char *argv[])
{
	FUZZY_TABLE *ft;
	ft=fuzzy_table_load("fuzzy.txt",NULL);
	if(!ft)
	{
		printf("load fuzzy fail\n");
		return -1;
	}
	// printf("load %p\n",ft);
	if(argc==1)
	{
		fuzzy_table_dump(ft);
	}
	else
	{
		LPtrArray *arr=l_ptr_array_new(4);
		fuzzy_key_list_simple(ft,argv[1],arr);
		for(int i=0;i<l_ptr_array_length(arr);i++)
		{
			printf("%s\n",(char*)l_ptr_array_nth(arr,i));
		}
		l_ptr_array_free(arr,l_free);
	}
	
	fuzzy_table_free(ft);
	return 0;
}
#endif
