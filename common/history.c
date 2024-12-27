#include "common.h"
#include "gbk.h"

#include <assert.h>

struct history_item{
	void *next;
	void *prev;
	char data[];
};

static FILE *fp_hist;
static int hist_dirty;

static int auto_sentence;
static uint8_t auto_sentence_begin;
static uint8_t auto_sentence_end;

static int assoc_history;
static LQueue *last_history;
static uint32_t flush_char;
static char temp_history[MAX_CAND_LEN+1];
static int temp_len;
static uint32_t temp_last;

typedef struct{
	void *next;
	char *from;
	char *to;
}REDIR_ITEM;

typedef struct{
	uint32_t pull;
	bool pull_is_flush_char;
	bool auto_run;
	bool recursive;
	int max;
	LHashTable *items;

	int back_repeat;
	const char *replace_str;
}REDIR_CONFIG;

static REDIR_CONFIG redir_config;
static int redirect_run(int len);

void y_im_history_free(void)
{
	if(fp_hist)
	{
		fprintf(fp_hist,"\n");
		fclose(fp_hist);
		fp_hist=NULL;
		hist_dirty=0;
	}
	l_queue_free(last_history);
	last_history=NULL;
	temp_len=0;
}

static int history_item_cmp(const struct history_item *v1,const struct history_item *v2)
{
	return strcmp(v1->data,v2->data);
}

static void flush_history(void)
{
	struct history_item *p,*o;
	if(!temp_len)
		return;
	if(temp_len<=2)
	{
		temp_len=0;
		return;
	}
	temp_history[temp_len]=0;
	if(auto_sentence && im.eim && im.eim->Call)
	{
		int ret=l_gb_strlen(temp_history,-1);
		if(ret>=auto_sentence_begin && ret<=auto_sentence_end)
		{
			int index=auto_sentence==1?EIM_CALL_ADD_PHRASE:EIM_CALL_ADD_SENTENCE;
			im.eim->Call(index,temp_history);
		}
	}
	if(last_history==NULL)
	{
		temp_len=0;
		return;
	}
	p=l_alloc(sizeof(struct history_item)+temp_len+1);
	strcpy(p->data,temp_history);
	temp_len=0;
	
	o=l_queue_find(last_history,p,(LCmpFunc)history_item_cmp);
	if(o!=NULL)
	{
		l_queue_remove(last_history,o);
		l_free(o);
	}
	l_queue_push_head(last_history,p);
	if(l_queue_length(last_history)>assoc_history)
	{
		p=l_queue_pop_head(last_history);
		l_free(p);
	}
}

static void put_history(const char *s)
{
	while(s[0]!=0)
	{
		if(!(s[0]&0x80))
		{
			if(s[0]<20 || isspace(s[0]) || ispunct(s[0]))
			{
				flush_history();
			}
			else
			{
				temp_history[temp_len++]=s[0];
			}
			s++;
		}
		else if(gb_is_gbk((const uint8_t*)s))
		{
			if(gb_is_biaodian((const uint8_t*)s))
			{
				flush_history();
			}
			else
			{
				temp_history[temp_len++]=s[0];
				temp_history[temp_len++]=s[1];
			}
			s+=2;
		}
		else if(gb_is_gb18030_ext((const uint8_t*)s))
		{
			temp_history[temp_len++]=*s++;
			temp_history[temp_len++]=*s++;
			temp_history[temp_len++]=*s++;
			temp_history[temp_len++]=*s++;
		}
		else
		{
			flush_history();
			s++;
		}
		if(temp_len>=MAX_CAND_LEN-5)
			flush_history();
	}
}

static int put_history2(const char *s,bool flush)
{
	if(gb_is_biaodian((const uint8_t*)s) && s[2]==0)
	{
		flush_char=l_gb_to_char(s);
		flush_history();
		return 0;
	}
	if(s[0]=='\b')
	{
		if(temp_len<=0)
			return 0;
		int t;
		temp_history[temp_len]=0;
		t=l_gb_strlen(temp_history,-1);
		char *p=l_gb_offset((const uint8_t*)temp_history,t-1);
		if(!p)
			return 0;
		*p=0;
		temp_len=(int)(size_t)(p-temp_history);
		return 0;
	}
	int len=l_gb_strlen(s,-1);
	while(s[0]!=0)
	{
		if(!(s[0]&0x80))
		{
			temp_last=s[0];
			if(s[0]<20 || isspace(s[0]) || ispunct(s[0]) || isdigit(s[0]))
			{
				flush_char=s[0];
				flush_history();
			}
			else
			{
				temp_history[temp_len++]=s[0];
			}
			s++;
		}
		else if(gb_is_gbk((const uint8_t*)s))
		{
			temp_last=0;
			if(gb_is_biaodian((const uint8_t*)s))
			{
				flush_char=l_gb_to_char(s);
				flush_history();
				s+=2;
				continue;
			}
			temp_history[temp_len++]=s[0];
			temp_history[temp_len++]=s[1];
			s+=2;
		}
		else if(gb_is_gb18030_ext((const uint8_t*)s))
		{
			temp_last=0;
			temp_history[temp_len++]=*s++;
			temp_history[temp_len++]=*s++;
			temp_history[temp_len++]=*s++;
			temp_history[temp_len++]=*s++;
		}
		else
		{
			flush_char=0;
			temp_last=0;
			flush_history();
			s++;
		}
		if(temp_len>=MAX_CAND_LEN-5)
		{
			flush_char=0;
			flush_history();
		}
	}
	if(flush && redir_config.auto_run)
	{
		return redirect_run(len);
	}
	return 0;
}

static void load_history(void)
{
	char line[256];
	if(!fp_hist)
		return;
	if(hist_dirty>=16)
	{
		hist_dirty=0;
		fflush(fp_hist);
	}
	if(ftell(fp_hist)>(assoc_history+1)*(MAX_CAND_LEN+1))
		fseek(fp_hist,-(assoc_history+1)*(MAX_CAND_LEN+1),SEEK_END);
	else
		fseek(fp_hist,0,SEEK_SET);
	while(l_get_line(line,sizeof(line),fp_hist)>=0)
	{
		put_history(line);
		flush_history();
	}
	fseek(fp_hist,0,SEEK_END);
}

int y_im_history_query(const char *src,char out[][MAX_CAND_LEN+1],int max)
{
	int i;
	struct history_item *p;
	int slen;
	if(!last_history || max<=0)
		return 0;
	slen=strlen(src);
	for(i=0,p=last_history->head;p!=NULL && i<max;p=p->next)
	{
		if(!memcmp(p->data,src,slen) && p->data[slen]!=0)
		{
			strcpy(out[i++],p->data+slen);
		}
	}
	return i;
}

const char *y_im_history_get_last(int len)
{
	int t;
	temp_history[temp_len]=0;
	t=l_gb_strlen(temp_history,-1);
	if(t<len)
		return 0;
	return l_gb_offset((const uint8_t*)temp_history,t-len);
}

void y_im_history_update(void)
{
	assoc_history=y_im_get_im_config_int(im.Index,"assoc_history");
	if(assoc_history<0 || assoc_history>65536) assoc_history=0;
	if(!assoc_history)
	{
		if(last_history)
		{
			l_queue_free(last_history);
			last_history=NULL;
			temp_len=0;
		}			
	}
	else
	{
		if(!last_history)
		{
			temp_len=0;
			last_history=l_queue_new(l_free);
			load_history();
		}
		while(l_queue_length(last_history)>assoc_history)
		{
			void *p=l_queue_pop_head(last_history);
			l_free(p);
		}
	}
	if(fp_hist)
		fprintf(fp_hist,"\n");

	auto_sentence=0;
	char *config=y_im_get_im_config_string(im.Index,"auto_sentence");
	if(config)
	{
		char file[128];
		int begin,end,count;
		int ret=l_sscanf(config,"%d %d %s %d",&begin,&end,file,&count);
		if(begin>=2 && end>=begin && end<=20)
		{
			if(ret==2)
				auto_sentence=1;
			else if(ret==4)
				auto_sentence=2;
			auto_sentence_begin=begin;
			auto_sentence_end=end;
		}
		l_free(config);
	}
}

void y_im_history_init(void)
{
	if(fp_hist)
		y_im_history_free();
	
	const char *fn=y_im_get_config_data("IM","history");
	if(!fn || !fn[0])
	{
		return;
	}
	fp_hist=y_im_open_file(fn,"rb+");
	if(!fp_hist)
	{
		fp_hist=y_im_open_file(fn,"wb+");
		if(!fp_hist) return;
	}
	fseek(fp_hist,0,SEEK_END);
}

int y_im_history_write(const char *s,bool flush)
{
	if(temp_last=='\n' && s[0]=='\n')
		s++;
	int back=put_history2(s,flush);
	if(!fp_hist || !s)
		return back;
	fprintf(fp_hist,"%s",s);
	hist_dirty++;
	if(hist_dirty>=16)
	{
		hist_dirty=0;
		fflush(fp_hist);
	}
	return back;
}

void y_im_history_flush(void)
{
	if(hist_dirty>0)
	{
		hist_dirty=0;
		fflush(fp_hist);
	}
}

static void redirect_item_free(REDIR_ITEM *p)
{
	l_free(p->from);
	l_free(p->to);
	l_free(p);
}

void y_im_history_redirect_free(void)
{
	if(!redir_config.items)
		return;
	redir_config.pull=0;
	redir_config.auto_run=true;
	redir_config.pull_is_flush_char=false;
	l_hash_table_free(redir_config.items,(LFreeFunc)redirect_item_free);
}

void y_im_history_redirect_init(void)
{
	y_im_history_redirect_free();
	const char *fn=y_im_get_config_data("IM","redirect");
	if(!fn)
		return;
	FILE *fp=y_im_open_file(fn,"rb");
	if(!fp)
		return;
	redir_config.items=L_HASH_TABLE_STRING(REDIR_ITEM,from,31);
	redir_config.max=8;
	redir_config.auto_run=true;
	redir_config.recursive=false;
	while(1)
	{
		char line[1204];
		int len=l_get_line(line,sizeof(line),fp);
		if(len<0)
		{
			fclose(fp);
			return;
		}
		if(len==0 || line[0]=='#')
			continue;
		if(!strcasecmp(line,"[data]"))
			break;
		if(l_str_has_prefix(line,"pull="))
		{
			const char *s=line+5;
			if(!s[0])
				continue;
			redir_config.pull_is_flush_char=s[0]<20 || isspace(s[0]) || ispunct(s[0]) || isdigit(s[0]) || gb_is_biaodian((const void*)s);
			redir_config.pull=l_gb_to_char(s);
		}
		else if(l_str_has_prefix(line,"max="))
		{
			redir_config.max=atoi(line+4);
		}
		else if(l_str_has_prefix(line,"auto="))
		{
			redir_config.auto_run=atoi(line+5)?true:false;
		}
	}
	while(1)
	{
		char line[256];
		int len=l_get_line(line,sizeof(line),fp);
		if(len<0)
			break;
		if(len==0 || line[0]=='#')
			continue;
		char *arr[2];
		if(2!=l_strtok0(line,' ',arr,2))
			continue;
		REDIR_ITEM *item=l_new(REDIR_ITEM);
		item->from=l_strdup(arr[0]);
		item->to=l_strdup(arr[1]);
		if(!l_hash_table_insert(redir_config.items,item))
		{
			redirect_item_free(item);
		}
	}
	fclose(fp);
}

static void send_string_no_recursive(const char *s)
{
	redir_config.recursive=true;
	y_xim_send_string(s);
	redir_config.recursive=false;
}

static void redirect_run_delay_cb(REDIR_CONFIG *p)
{
	if(p->back_repeat)
	{
		y_xim_forward_key(YK_BACKSPACE,p->back_repeat);
		y_ui_timer_add(50,(void*)send_string_no_recursive,(void*)p->replace_str);
	}
	else
	{
		send_string_no_recursive(p->replace_str);
	}
}

static void redirect_run_delay(int back_repeat,const char *s)
{
	redir_config.back_repeat=back_repeat;
	redir_config.replace_str=s;
	y_ui_idle_add((void*)redirect_run_delay_cb,&redir_config);
}

static int redirect_run(int last)
{
	if(!redir_config.items)
		return 0;
	if(temp_len<2)
		return 0;
	if(redir_config.recursive)
		return 0;
	temp_history[temp_len]=0;
	int len=l_gb_strlen(temp_history,-1);
	if(redir_config.pull_is_flush_char)
	{
		if(flush_char!=redir_config.pull)
			return 0;
		if(len<2 || len>redir_config.max)
			return 0;
		REDIR_ITEM *item=l_hash_table_lookup(redir_config.items,temp_history);
		if(!item)
			return 0;
		if(im.InAssoc)
			YongResetIM();
		temp_history[0]=0;
		temp_len=0;
		flush_char=0;
		int repeat=len-last+1;
		redirect_run_delay(repeat,item->to);
		return 1;
	}
	if(len<1)
	{
		return 0;
	}
	char *p;
	if(len<=redir_config.max)
	{
		p=temp_history;
	}
	else
	{
		p=l_gb_offset(temp_history,len-redir_config.max);
		len=redir_config.max;
	}
	for(int i=len;i>=1;i--)
	{
		REDIR_ITEM *item=l_hash_table_lookup(redir_config.items,p);
		if(item!=NULL)
		{
			if(im.InAssoc)
			{
				YongResetIM();
			}
			p[0]=0;
			temp_len=(int)(size_t)(p-temp_history);
			flush_history();
			int repeat=i-last;
			redirect_run_delay(repeat,item->to);
			return 1;
		}
		p=l_gb_next_char(p);
	}
	return 0;
}

int y_im_history_redirect_run(void)
{
	if(redir_config.auto_run)
		return -1;
	return redirect_run(0);
}
