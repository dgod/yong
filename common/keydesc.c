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
static void y_im_key_desc_free(void)
{
	l_slist_free(p_key_desc_idx,(LFreeFunc)key_desc_idx_free);
	p_key_desc_idx=NULL;
}

int y_im_key_desc_update(void)
{
	char *desc_file;
	FILE *fp;
	char line[1024];
	int i;
	struct key_desc_idx *idx=NULL;
	LArray *ikey;
	char **list;
	char *p;

	y_im_key_desc_free();
	desc_file=y_im_get_config_string("IM","key_desc");
	if(!desc_file)
		return 0;
	fp=y_im_open_file(desc_file,"r");
	l_free(desc_file);
	if(!fp)
		return 0;
	for(;l_get_line(line,sizeof(line),fp)>0;)
	{
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
			snprintf(temp.key,4,"%s",line);
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

int y_im_key_desc_translate(const char *code,int pos,char *data,char *res,int size)
{
	char out[size*2+1];
	int i,zi;
	struct key_desc_idx *idx;
	LArray *ikey;
	const char *desc;

	if(im.EnglishMode)
	{
		y_english_key_desc(code,out);
		y_im_str_encode(out,res,DONT_ESCAPE);
		return 0;
	}
	if(!p_key_desc_idx)
		goto QUIT;
	if(code[0]=='@')
		goto QUIT;
	/* get n chars phrase's desc */
	zi=gb_strlen((uint8_t*)data);
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
		strncpy(temp,code+i,3);temp[3]=0;
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
