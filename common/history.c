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
static char temp_history[MAX_CAND_LEN+1];
static int temp_len;
static uint32_t temp_last;

void y_im_history_free(void)
{
	if(fp_hist)
	{
		fprintf(fp_hist,"\n");
		fclose(fp_hist);
		fp_hist=0;
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
		int ret=gb_strlen((const uint8_t *)temp_history);
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

static void put_history2(const char *s)
{
	if(gb_is_biaodian((const uint8_t*)s) && s[2]==0)
	{
		flush_history();
		return;
	}
	if(s[0]=='\b')
	{
		if(temp_len<=0)
			return;
		int t;
		temp_history[temp_len]=0;
		t=gb_strlen((const uint8_t*)temp_history);
		char *p=gb_offset((const uint8_t*)temp_history,t-1);
		if(!p)
			return;
		*p=0;
		temp_len=(int)(size_t)(p-temp_history);
		return;
	}
	while(s[0]!=0)
	{
		if(!(s[0]&0x80))
		{
			temp_last=s[0];
			if(s[0]<20 || isspace(s[0]) || ispunct(s[0]) || isdigit(s[0]))
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
			temp_last=0;
			if(gb_is_biaodian((const uint8_t*)s))
			{
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
			temp_last=0;
			flush_history();
			s++;
		}
		if(temp_len>=MAX_CAND_LEN-5)
			flush_history();
	}
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
	t=gb_strlen((const uint8_t*)temp_history);
	if(t<len)
		return 0;
	return gb_offset((const uint8_t*)temp_history,t-len);
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
	char *fn;
	
	if(fp_hist)
		y_im_history_free();
	
	fn=y_im_get_config_string("IM","history");
	if(!fn || !fn[0])
	{
		if(fn)
			l_free(fn);
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

void y_im_history_write(const char *s)
{
	if(temp_last=='\n' && s[0]=='\n')
		s++;
	put_history2(s);
	if(!fp_hist || !s)
		return;
	fprintf(fp_hist,"%s",s);
	hist_dirty++;
	if(hist_dirty>=16)
	{
		hist_dirty=0;
		fflush(fp_hist);
	}
}

void y_im_history_flush(void)
{
	if(hist_dirty>0)
	{
		hist_dirty=0;
		fflush(fp_hist);
	}

}

