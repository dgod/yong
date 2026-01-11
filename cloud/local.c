#include "llib.h"
#include "gbk.h"
#include "yong.h"

extern EXTRA_IM EIM;

static char l_chars_assist[GBK_SIZE][2];
static uint8_t l_assist_key[128];

struct pinyin_item{
	void *next;
	LString *list;
	char pinyin[16];
};

struct data_item{
	void *next;
	char *data;
};

struct phrase_item{
	void *next;
	char pinyin[8];
	void *head;
};

static void pinyin_item_free(struct pinyin_item *item)
{
	if(!item) return;
	l_string_free(item->list);
	l_free(item);
}

static void data_item_free(struct data_item *item)
{
	if(!item) return;
	if((((uintptr_t)item->data) & 0x01)==0)
		l_free(item->data);
	l_free(item);
}

static void  phrase_item_free(struct phrase_item *item)
{
	if(!item) return;
	l_slist_free(item->head,(LFreeFunc)data_item_free);
	l_free(item);
}

static LHashTable *l_chars_pinyin;
static LHashTable *l_user;

void local_load_pinyin(const char *fn)
{
	char line[4096];
	FILE *fp;
	int in_data=0;
	
	fp=EIM.OpenFile(fn,"rb");
	if(!fp) return;
	
	l_chars_pinyin=L_HASH_TABLE_STRING(struct pinyin_item,pinyin,7001);
	
	while(1)
	{
		char **list;
		int len;
		len=l_get_line(line,sizeof(line),fp);
		if(len<0)
			break;
		if(len==0)
			continue;
		if(!in_data && line[0]=='[' && !strcasecmp(line,"[DATA]"))
		{
			in_data=1;
			continue;
		}
		if(!in_data)
		{
			continue;
		}
		if(line[0]<'a' || line[0]>'z')
			continue;
		list=l_strsplit(line,' ');
		if(!list)
			continue;
		len=l_strv_length(list);
		if(len>1 && strlen(list[0])<13)
		{
			int i;
			struct pinyin_item *item;
			item=l_hash_table_lookup(l_chars_pinyin,list[0]);
			for(i=1;i<len;i++)
			{
				char *p=list[i];
				int dlen;
				if(*p=='~') p++;
				dlen=strlen(p);
				if(dlen!=2 && dlen!=4)
					continue;
				if(!gb_is_gbk((uint8_t*)p))
					continue;
				if(dlen==4 && !gb_is_gbk((uint8_t*)p+2))
					continue;
				
				if(!item)
				{
					item=l_new(struct pinyin_item);
					strcpy(item->pinyin,list[0]);
					item->list=l_string_new(8);
					l_hash_table_replace(l_chars_pinyin,item);
				}
				l_string_append(item->list,p,dlen);
				if(i+1<len)
					l_string_append_c(item->list,' ');
			}
		}
		l_strfreev(list);
	}
	fclose(fp);
}

void local_load_assist(const char *fn,int pos)
{
	char line[4096];
	FILE *fp;
	int in_data=0;
	if(pos<0) return;
	
	fp=EIM.OpenFile(fn,"rb");
	if(!fp) return;
	
	while(1)
	{
		char **list;
		int len;
		len=l_get_line(line,sizeof(line),fp);
		if(len<0)
			break;
		if(len==0)
			continue;
		if(line[0]=='#')
			continue;
		if(!in_data && line[0]=='[' && !strcasecmp(line,"[DATA]"))
		{
			in_data=1;
			continue;
		}
		if(!in_data)
		{
			if(!strchr(line,'=') && strchr(line,' '))
				in_data=1;
			else
				continue;
		}
		if(line[0]<'a' || line[0]>'z')
			continue;
		list=l_strsplit(line,' ');
		if(!list)
			continue;
		len=l_strv_length(list);
		if(len>1 && strlen(list[0])>pos)
		{
			int i;
			for(i=1;i<len;i++)
			{
				char *p=list[i];
				uint16_t hz;
				char *assist;
				if(*p=='~') p++;
				if(strlen(p)!=2)
					continue;
				if(!gb_is_gbk((uint8_t*)p))
					continue;
				hz=GBK_MAKE_CODE(p[0],p[1]);
				
				int key=list[0][pos];
				assist=&l_chars_assist[GBK_OFFSET(hz)][0];
				if(!assist[0]) assist[0]=key;
				else assist[1]=key;
				
				l_assist_key[key]=1;
			}
		}
		l_strfreev(list);
	}
	fclose(fp);
}

void local_free_all(void)
{
	l_hash_table_free(l_user,(LFreeFunc)phrase_item_free);
	l_user=NULL;
	l_hash_table_free(l_chars_pinyin,(LFreeFunc)pinyin_item_free);
	l_chars_pinyin=NULL;
}


bool local_assist_match(const char *p,int c)
{
	char temp[256];
	char *assist;
	uint16_t hz;
	if(!gb_is_gbk((uint8_t*)p))
		return false;
	hz=GBK_MAKE_CODE(p[0],p[1]);
	assist=&l_chars_assist[GBK_OFFSET(hz)][0];
	l_gb_to_utf8(p,temp,sizeof(temp));
	return (c==assist[0] || c==assist[1]);
}

bool local_is_assist_key(int key)
{
	if(key<=0 || (key&~0x7f))
		return false;
	return l_assist_key[key]?true:false;
}

const char *local_pinyin_get(const char *pinyin)
{
	struct pinyin_item *item;
	if(!l_chars_pinyin || !pinyin[0])
		return NULL;
	item=l_hash_table_lookup(l_chars_pinyin,pinyin);
	if(item) return item->list->str;
	if(!pinyin[1]) switch(pinyin[0]){
	case 'b':pinyin="bu";break;
	case 'c':pinyin="ci";break;
	case 'd':pinyin="de";break;
	case 'f':pinyin="fei";break;
	case 'g':pinyin="ge";break;
	case 'h':pinyin="he";break;
	case 'j':pinyin="ji";break;
	case 'k':pinyin="ke";break;
	case 'l':pinyin="le";break;
	case 'm':pinyin="mei";break;
	case 'n':pinyin="ni";break;
	case 'p':pinyin="pai";break;
	case 'q':pinyin="qi";break;
	case 'r':pinyin="ren";break;
	case 's':pinyin="san";break;
	case 't':pinyin="ta";break;
	case 'w':pinyin="wo";break;
	case 'x':pinyin="xiao";break;
	case 'y':pinyin="yi";break;
	case 'z':pinyin="zai";break;
	}
	else if(!pinyin[2] && pinyin[1]=='h') switch(pinyin[0]){
	case 'c':pinyin="chu";break;
	case 's':pinyin="shi";break;
	case 'z':pinyin="zhe";break;
	}
	else
	{
		return NULL;
	}
	item=l_hash_table_lookup(l_chars_pinyin,pinyin);
	if(!item) return NULL;
	return item->list->str;
}

void local_load_user(const char *fn)
{
	char line[4096];
	FILE *fp;
	int in_data=0;
	
	fp=EIM.OpenFile(fn,"rb");
	if(!fp) return;
	
	l_user=L_HASH_TABLE_STRING(struct phrase_item,pinyin,7001);
	
	while(1)
	{
		char **list;
		int len;
		len=l_get_line(line,sizeof(line),fp);
		if(len<0)
			break;
		if(len==0)
			continue;
		if(line[0]=='#')
			continue;
		if(!in_data && line[0]=='[' && !strcasecmp(line,"[DATA]"))
		{
			in_data=1;
			continue;
		}
		if(!in_data)
		{
			if(!strchr(line,'=') && strchr(line,' '))
				in_data=1;
			else
				continue;
		}
		if(line[0]<'a' || line[0]>'z')
			continue;
		list=l_strsplit(line,' ');
		if(!list)
			continue;
		len=l_strv_length(list);
		if(len>1 && strlen(list[0])<8)
		{
			int i;
			struct phrase_item *item,key;
			struct data_item *data;
			strcpy(key.pinyin,list[0]);
			item=l_hash_table_find(l_user,&key);
			//printf("%s %p\n",list[0],item);
			for(i=1;i<len;i++)
			{
				char *p=list[i];
				if(*p=='~') p++;
				
				if(!item)
				{
					item=l_new(struct phrase_item);
					strcpy(item->pinyin,list[0]);
					item->next=NULL;
					item->head=NULL;
					l_hash_table_replace(l_user,item);
				}
				data=l_new(struct data_item);
				data->next=NULL;
				data->data=l_strdup(p);
				item->head=l_slist_append(item->head,data);
				//printf("%s\n",p);
			}
		}
		l_strfreev(list);
	}
	fclose(fp);
}

const void *local_phrase_set(const char *pinyin)
{
	struct phrase_item *item;
	if(!pinyin || !pinyin[0] || strlen(pinyin)>7)
		return NULL;
	if(!l_user)
		return NULL;
	item=l_hash_table_find(l_user,pinyin);
	return item;
}

int local_phrase_count(const void *phrase)
{
	const struct phrase_item *item=phrase;
	if(!item) return 0;
	return l_slist_length(item->head);
}

int local_phrase_get(const void *phrase,int at,int num,
	char cand[][MAX_CAND_LEN+1])
{
	const struct phrase_item *item=phrase;
	struct data_item *p;
	int i;
	if(!item) return 0;
	for(p=item->head,i=0;i<at && p!=NULL;p=p->next,i++);
	for(i=0;i<num && p!=NULL;p=p->next,i++)
		strcpy(cand[i],p->data);
	return i;
}
