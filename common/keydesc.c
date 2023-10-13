#include "common.h"
#include "gbk.h"

struct desc_item{
	int pos;
	char desc[20];
};

struct key_array{
	char key[4];
	LArray *array;
};

struct key_desc_idx{
	void *next;
	LArray *key;
	uint8_t above;
	uint8_t nchar;
	uint8_t split;
	uint8_t at;
};

struct cand_desc_item{
	char *code;
	char *key_desc;
	char *comment;
};

struct cand_desc{
	void *next;
	char *cand;
	struct cand_desc_item arr[4];
	int count;
};

static void cand_desc_free(struct cand_desc *p)
{
	if(!p)
		return;
	for(int i=0;i<p->count;i++)
	{
		struct cand_desc_item *it=&p->arr[i];
		l_free(it->code);
		l_free(it->key_desc);
		l_free(it->comment);
	}
	l_free(p->cand);
	l_free(p);
}

static LArray *key_array_lookup(LArray *key,const char *s)
{
	int i;
	for(i=0;i<key->len;i++)
	{
		struct key_array *p=l_array_nth(key,i);
		if(!strcmp(p->key,s))
			return p->array;
	}
	return 0;
}

static const char *desc_item_lookup(LArray *array,int pos)
{
	int i;
	for(i=0;i<array->len;i++)
	{
		struct desc_item *p=l_array_nth(array,i);
		if(pos==p->pos)
			return p->desc;
	}
	return 0;
}

static void key_array_free(struct key_array *key)
{
	l_array_free(key->array,0);
}

static void key_desc_idx_free(struct key_desc_idx *idx)
{
	l_array_free(idx->key,(LFreeFunc)key_array_free);
	l_free(idx);
}

static struct key_desc_idx *p_key_desc_idx;
static LHashTable *p_cand_desc_idx;
static char *p_cand_desc_format;
static void y_im_key_desc_free(void)
{
	l_slist_free(p_key_desc_idx,(LFreeFunc)key_desc_idx_free);
	p_key_desc_idx=NULL;
	l_hash_table_free(p_cand_desc_idx,(LFreeFunc)cand_desc_free);
	p_cand_desc_idx=NULL;
	l_free(p_cand_desc_format);
	p_cand_desc_format=NULL;
}

static void load_cand_desc(FILE *fp)
{
	char line[1024];
	p_cand_desc_idx=L_HASH_TABLE_STRING(struct cand_desc,cand);
	for(;l_get_line(line,sizeof(line),fp)>=0;)
	{
		if(line[0]==0 || line[0]=='#')
			continue;
		char *key_desc=strchr(line,'=');
		if(!key_desc)
			continue;
		char *code=strchr(line,':');
		*key_desc++=0;
		if(code!=NULL)
		{
			*code++=0;
			if(strlen(code)>MAX_CODE_LEN)
				continue;
		}
		char *cand=line;
		char *comment=strchr(key_desc,' ');
		if(comment)
		{
			if(comment==key_desc)
			{
				key_desc=NULL;
			}
			*comment++=0;
		}
		if(key_desc && strlen(key_desc)>MAX_CAND_LEN)
		{
			continue;
		}
		if(comment && strlen(comment)>MAX_CAND_LEN)
		{
			continue;
		}
		struct cand_desc *desc=l_hash_table_lookup(p_cand_desc_idx,cand);
		if(!desc)
		{
			desc=l_new0(struct cand_desc);
			desc->cand=l_strdup(cand);
			l_hash_table_insert(p_cand_desc_idx,desc);
		}
		if(desc->count<L_ARRAY_SIZE(desc->arr))
		{
			struct cand_desc_item *item=&desc->arr[desc->count++];
			item->code=code?l_strdup(code):NULL;
			item->key_desc=key_desc?l_strdup(key_desc):NULL;
			item->comment=comment?l_strdup(comment):NULL;
		}
	}
}

int y_im_key_desc_update(void)
{
	const char *desc_file;
	FILE *fp;
	char line[1024];
	int i;
	struct key_desc_idx *idx=NULL;
	LArray *ikey;
	char **list;
	char *p;

	y_im_key_desc_free();
	desc_file=y_im_get_im_config_data(im.Index,"key_desc");
	if(!desc_file)
		desc_file=y_im_get_config_data("IM","key_desc");
	if(!desc_file)
		return 0;
	fp=y_im_open_file(desc_file,"r");
	if(!fp)
		return 0;
	for(;l_get_line(line,sizeof(line),fp)>=0;)
	{
		if(line[0]==0 || line[0]=='#')
			continue;
		if(!strncmp(line,"cand=",5))
		{
			p_cand_desc_format=l_strdup(line+5);
			load_cand_desc(fp);
			break;
		}
		if(!strncmp(line,"size=",5))
		{
			uint8_t cur_above,cur_size;
			if(line[5]=='e') cur_above=0;
			else if(line[5]=='a') cur_above=1;
			else continue;
			cur_size=atoi(line+6);
			idx=l_new(struct key_desc_idx);
			idx->above=cur_above;
			idx->nchar=cur_size;
			idx->split=0;
			idx->at=0;
			idx->key=l_array_new(4,sizeof(struct key_array));
			p_key_desc_idx=l_slist_prepend(p_key_desc_idx,idx);
			continue;
		}
		if(!idx) continue;
		if(!strncmp(line,"split=",6))
		{
			int at,ret;
			char temp[32];
			ret=l_sscanf(line+6,"%d %s",&at,temp);
			if(ret!=2 || at<=0 || at>9) continue;
			idx->at=at;
			if(!strcmp(temp,"SPACE"))
				idx->split=' ';
			else
				idx->split=temp[0];
			continue;
		}
		p=strchr(line,'=');if(!p) continue;*p=0;
		ikey=key_array_lookup(idx->key,line);
		if(!ikey)
		{
			struct key_array temp;
			ikey=l_array_new(26,sizeof(struct desc_item));
			l_strcpy(temp.key,4,line);
			temp.array=ikey;
			l_array_append(idx->key,&temp);
		}
		list=l_strsplit(p+1,'|');
		for(i=0;(p=list[i])!=0;i++)
		{
			struct desc_item temp;
			int ret;
			int pos;
			char desc[32];
			desc[0]=0;
			ret=l_sscanf(p,"%d %s",&pos,desc);
			if(ret<1) continue;
			if(pos<=0 || pos>=32) continue;
			if(ret==1) desc[0]=0;
			temp.pos=pos;
				snprintf(temp.desc,sizeof(temp.desc),"%s",desc);
			l_array_append(ikey,&temp);
		}
		l_strfreev(list);
	}
	fclose(fp);
	return 0;
}

static struct cand_desc_item *cand_desc_item_get(const char *code,const char *tip,const char *cand)
{
	struct cand_desc *p=l_hash_table_lookup(p_cand_desc_idx,cand);
	if(!p)
		return NULL;
	if(code==NULL)
	{
		for(int i=0;i<p->count;i++)
		{
			struct cand_desc_item *it=&p->arr[i];
			if(!it->code)
				return it;
		}
		return NULL;
	}
	int len;
	if(tip && tip[0])
	{
		char *temp=l_alloca(MAX_CODE_LEN+1);
		len=sprintf(temp,"%s%s",code,tip);
		code=temp;
	}
	else
	{
		len=strlen(code);
	}
	for(int i=0;i<p->count;i++)
	{
		struct cand_desc_item *it=&p->arr[i];
		if(!it->code || !strncmp(it->code,code,len))
		{
			return it;
		}
	}
	return NULL;
}

bool y_im_cand_desc_translate(const char *data,const char *code,const char *tip,char *res,int size)
{
	if(!p_cand_desc_idx|| !p_cand_desc_format)
		return false;
	if(p_cand_desc_format[0]==0 || !strcmp(p_cand_desc_format,"."))
		return false;
	struct cand_desc_item *item=cand_desc_item_get(code,tip,data);
	if(!item)
		return false;
	int pos=0,c;
	for(int i=0;(c=p_cand_desc_format[i])!=0;i++)
	{
		switch(c){
			case '.':
				{
					int len=strlen(data);
					if(len>=size-1)
						goto end;
					strcpy(res+pos,s2t_conv(data));
					pos+=len;
				}
				break;
			case '$':
				if(item->key_desc)
				{
					int len=strlen(item->key_desc);
					if(len>=size-1)
						goto end;
					strcpy(res+pos,item->key_desc);
					pos+=len;
				}
				break;
			case '/':
				if(item->comment)
				{
					int len=strlen(item->comment);
					if(len>=size-1)
						goto end;
					strcpy(res+pos,item->comment);
					pos+=len;
				}
				break;
			default:
				if(pos>=size-1)
					goto end;
				res[pos++]=c;
				break;
		}
	}
end:
	res[pos]=0;
	return true;
}

int y_im_key_desc_translate(const char *code,const char *tip,int pos,const char *data,char *res,int size)
{
	char out[size*2+1];
	int i,zi;
	struct key_desc_idx *idx;
	LArray *ikey;
	const char *desc;

	if(im.EnglishMode)
	{
		y_english_key_desc(pos?tip:code,out);
		y_im_str_encode(out,res,DONT_ESCAPE);
		return 0;
	}
	if(code[0]=='@')
		goto QUIT;
	if(p_cand_desc_idx)
	{
		struct cand_desc_item *item=cand_desc_item_get(code,tip,data);
		if(item && item->key_desc!=NULL)
		{
			const char *p=gb_offset((const uint8_t*)item->key_desc,pos);
			if(p!=NULL)
			{
				int len=strlen(pos?tip:code);
				const char *e=gb_offset((const uint8_t*)p,len);
				if(e!=NULL)
				{
					len=(int)(size_t)(e-p);
					p=l_strndupa(p,len);
				}
				y_im_str_encode(p,res,DONT_ESCAPE);
			}
			else
			{
				res[0]=0;
			}
			return 0;
		}
	}
	
	if(pos!=0)
		code=tip;
	if(!p_key_desc_idx)
	{
		goto QUIT;
	}
	if(code[0]=='@')
		goto QUIT;

	/* get n chars phrase's desc */
	zi=gb_strlen((const uint8_t*)data);
	for(idx=p_key_desc_idx;idx;idx=idx->next)
	{
		if(zi==idx->nchar || (zi>idx->nchar && idx->above))
			break;
	}
	if(!idx)
	{
		goto QUIT;
	}
	/* get desc of code at special pos */
	out[0]=0;
	code-=pos;
	i=pos;
NEXT:
	for(;code[i]!=0;i++)
	{
		char temp[4];
		int tlen;
		if(code[i]==' ')
		{
			/* skip space, so let this can work with pinyin */
			if(idx->split)
			{
				temp[0]=idx->split;
				temp[1]=0;
				strcat(out,temp);
			}
			code=code+i+1;
			i=0;
			goto NEXT;
		}
		if(i!=0 && idx->split && (i%idx->at)==0)
		{			
			temp[0]=idx->split;
			temp[1]=0;
			strcat(out,temp);
		}
		l_strncpy(temp,code+i,3);
		for(tlen=strlen(temp);;temp[--tlen]=0)
		{		
			ikey=key_array_lookup(idx->key,temp);
			if(ikey)
			{
				i+=tlen-1;
				break;
			}
			if(!ikey && tlen==1)
			{
				goto QUIT;
			}
		}
		desc=desc_item_lookup(ikey,i+1);
		if(!desc && idx->split)
		{
			desc=desc_item_lookup(ikey,(i+1)%idx->at+1);
			if(!desc)
				desc=desc_item_lookup(ikey,1);
		}
		if(!desc)
		{
			goto QUIT;
		}
		strcat(out,desc);
	}
	y_im_str_encode(out,res,DONT_ESCAPE);
	return 0;
QUIT:
	y_im_str_encode(code,res,DONT_ESCAPE);
	return 0;
}
