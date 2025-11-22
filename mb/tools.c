#include "llib.h"
#include "mb.h"
#include "gbk.h"

#define mb_hash_find(h,v) l_hash_table_find((h),(v))

char *mb_data_conv_r(uint32_t in);
char *mb_key_conv_r(struct y_mb *mb,uint16_t index,uintptr_t in);
void mb_rule_dump(struct y_mb *mb,FILE *fp);
int mb_load_data(struct y_mb *mb,FILE *fp,int dic);

int y_mb_pick(struct y_mb *mb,FILE *fp,int option,int clen,int dlen,int filter,char *pre)
{
	struct y_mb_index *index;

	if(!mb)
		return 0;
	for(index=mb->index;index;index=index->next)
	{
		struct y_mb_item *it=index->item;
		while(it)
		{
			char *code;
			char *data;
			struct y_mb_ci *cp;
			code=mb_key_conv_r(mb,index->index,it->code);				
			cp=it->phrase;
			if(clen && clen!=strlen(code))
			{
			}
			else if(!cp)
			{
			}
			else if(mb->english)
			{
			}
			else
			{
				while(cp)
				{
					if(cp->del)
						goto dump_next;
					data=y_mb_ci_string(cp);
					if(dlen && dlen!=l_gb_strlen(data,-1))
						goto dump_next;
					if(filter && cp->zi && cp->ext)
						goto dump_next;
					if(cp->dic==Y_MB_DIC_MAIN)
					{
						if(!(option&MB_DUMP_MAIN))
							goto dump_next;
					}
					else if(cp->dic==Y_MB_DIC_USER)
					{
						if(!(option&MB_DUMP_USER))
							goto dump_next;
					}
					else if(cp->dic==Y_MB_DIC_TEMP)
					{
						if(!(option&MB_DUMP_TEMP))
							goto dump_next;
					}
					else if(!(option&MB_DUMP_DICTS))
					{
						goto dump_next;
					}
					if(pre) fprintf(fp,"%s",pre);
					fprintf(fp,"%s %s\n",code,y_mb_ci_string(cp));
dump_next:
					cp=cp->next;
				}
			}
			it=it->next;
		}
	}
	return 0;
}

int y_mb_encode(struct y_mb *mb,FILE *fp,char *fn)
{
	FILE *in;
	char line[Y_MB_DATA_SIZE+1];
	char code[Y_MB_KEY_SIZE+1];
	int len;
	
	if(!strcmp(fn,"-"))
		in=stdin;
	else
		in=fopen(fn,"r");
	if(!in)
		return -1;
	for(;(len=l_get_line(line,sizeof(line),in))>=0;)
	{
		if(len==0 || line[0]==0)
			continue;
		if(y_mb_get_exist_code(mb,line,code))
		{
			fprintf(fp,"%s %s\n",code,line);
		}
		else
		{
			char *codes[]={code,NULL};
			if(0==y_mb_code_by_rule(mb,line,strlen(line),codes,NULL))
			{
				fprintf(fp,"%s %s\n",code,line);
			}
		}
	}
	if(in!=stdin)
		fclose(in);
	return 0;
}

int y_mb_encode_py(struct y_mb *mb,FILE *fp,char *fn)
{
	FILE *in;
	char line[Y_MB_DATA_SIZE+1];
	char code[Y_MB_KEY_SIZE+1];
	int len;
	char *p;
	
	if(!strcmp(fn,"-"))
		in=stdin;
	else
		in=fopen(fn,"r");
	if(!in)
		return -1;
	for(;(len=l_get_line(line,sizeof(line),in))>=0;)
	{
		/* if it is null line */
		if(len==0 || line[0]==0)
			continue;
		/* skip space and english */
		p=line;	while(p[0] && !(p[0]&0x80)) p++;
		if(y_mb_get_exist_code(mb,p,code) && strlen(code)<=11)
		{
			fprintf(fp,"%s %s\n",code,line);
		}
		else
		{
			char *codes[]={code,NULL};
			if(0==y_mb_code_by_rule(mb,p,strlen(line),codes,NULL))
			{
				fprintf(fp,"%s %s\n",code,line);
			}
		}
	}
	if(in!=stdin)
		fclose(in);
	return 0;
}

static void mb_strip_zi(char *s,int len)
{
	int t;

	while(*s && len>0)
	{
		if(len>=4 && gb_is_gb18030_ext((uint8_t*)s))
		{
			s+=4;
			len-=2;
			continue;
		}
		else if(len>=2 && gb_is_gbk((uint8_t*)s))
		{
			s+=2;
			len-=2;
			continue;
		}
		t=strlen(s+1)+1;
		memmove(s,s+1,t);
		len--;
	}
}

int y_mb_diff(struct y_mb *mb,FILE *fp,char *fn,int strict)
{
	FILE *in;
	char line[Y_MB_DATA_SIZE+1];
	char code[Y_MB_KEY_SIZE+1];
	int len;

	if(!strcmp(fn,"-"))
		in=stdin;
	else
		in=fopen(fn,"r");
	if(!in)
		return -1;
	for(;(len=l_get_line(line,sizeof(line),in))>=0;)
	{
		mb_strip_zi(line,len);
		if(len==0 || line[0]==0)
			continue;
		if(y_mb_get_exist_code(mb,line,code))
			continue;
		if(strict)
		{
			char *codes[]={code,NULL};
			if(y_mb_code_by_rule(mb,line,strlen(line),codes,NULL)!=0)
				continue;
			if(y_mb_code_exist(mb,code,strlen(code),-1))
				continue;
		}
		fprintf(fp,"%s\n",line);
	}
	if(in!=stdin)
		fclose(in);
	return 0;
}

static void mb_hash_foreach(LHashTable *h,LEnumFunc cb,void *arg)
{
	LHashIter iter;
	void *data;
	l_hash_iter_init(&iter,h);
	while((data=l_hash_iter_next(&iter))!=NULL)
	{
		cb(data,arg);
	}
}


#ifdef TOOLS_TEST

#include <time.h>
EXTRA_IM EIM;

int main(int arc,char *arg[])
{
	clock_t start;
	struct y_mb *mb;
	
	if(arc!=2)
		return 0;
	y_mb_init();
	start=clock();
	mb=y_mb_load(arg[1],0,NULL);
	//y_mb_dump(mb,stdout,MB_DUMP_HEAD|MB_DUMP_MAIN,MB_FMT_YONG,NULL);
	y_mb_free(mb);
	fprintf(stderr,"load in %.2f seconds\n",((double)(clock()-start))/(double)CLOCKS_PER_SEC);
	y_mb_cleanup();
	return 0;
}
#endif

static void mb_zi_virt_count(struct y_mb_zi *z,int *count)
{
	/* don't dump virt code of not main */
	if(!z->code)
		return;
	if(z->code->virt && z->code->main)
		(*count)++;
}

static void mb_zi_virt_store(struct y_mb_zi *z,struct y_mb_zi ***pz)
{
	/* is z removed or moved z->code will be 0 */
	if(!z->code || !z->code->virt || !z->code->main)
		return;
	**pz=z;
	*pz=(*pz)+1;
}

static int mb_zi_virt_cmp(const void *p1,const void *p2)
{
	struct y_mb_zi *z1=*(struct y_mb_zi**)p1;
	struct y_mb_zi *z2=*(struct y_mb_zi**)p2;
	return y_mb_code_cmp(z1->code,z2->code,Y_MB_KEY_SIZE);
}

#ifndef CFG_XIM_ANDROID

static void mb_head_dump(struct y_mb *mb,FILE *fp)
{
	fprintf(fp,"name=%s\n",mb->name);
	fprintf(fp,"key=%s\n",mb->key+1);
	if(mb->key0[0])
		fprintf(fp,"key0=%s\n",mb->key0);
	fprintf(fp,"len=%d\n",mb->len);
	if(mb->push[0])
		fprintf(fp,"push=%s\n",mb->push);
	if(mb->pull[0])
		fprintf(fp,"pull=%s\n",mb->pull);
	if(mb->suffix>=0)
	{
		if(mb->suffix==0)
			fprintf(fp,"suffix=NONE\n");
		else
			fprintf(fp,"suffix=%c\n",mb->suffix);
	}
	if(mb->match)
		fprintf(fp,"match=1\n");
	if(mb->wildcard_orig)
		fprintf(fp,"wildcard=%c\n",mb->wildcard);
	if(mb->dwf)
		fprintf(fp,"dwf=1\n");
	if(mb->english)
		fprintf(fp,"english=1\n");
	if(mb->simple)
		fprintf(fp,"simple=%d\n",mb->simple);
	if(mb->compact)
		fprintf(fp,"compact=%d\n",mb->compact);
	if(mb->yong)
		fprintf(fp,"yong=%d\n",mb->yong);
	if(mb->pinyin)
		fprintf(fp,"pinyin=%d\n",mb->pinyin);
	if(!mb->hint)
		fprintf(fp,"hint=0\n");
	if(mb->auto_clear)
		fprintf(fp,"auto_clear=%d\n",mb->auto_clear);
	else if(mb->nomove[0])
		fprintf(fp,"nomove=%s\n",mb->nomove);
	else if(mb->auto_move)
		fprintf(fp,"auto_move=%d\n",mb->auto_move);
	else if(mb->nsort)
		fprintf(fp,"nsort=1\n");
	else if(mb->sloop)
		fprintf(fp,"sloop=%d\n",mb->sloop);
	else if(mb->split>0 && mb->split<10)
		fprintf(fp,"split=%d\n",mb->split);
	else if(mb->split)
		fprintf(fp,"split=%c\n",mb->split);
	if(mb->commit_mode || mb->commit_len || mb->commit_which)
		fprintf(fp,"commit=%d %d %d\n",mb->commit_mode,
			mb->commit_len,mb->commit_which);
	if(mb->dicts[0])
	{
		int i,len;
		char temp[1024];
		len=sprintf(temp,"dicts=");
		for(i=0;i<10 && mb->dicts[i];i++)
		{
			if(i) len+=sprintf(temp+len," ");
			len+=sprintf(temp+len,"%s",mb->dicts[i]);
		}
		fprintf(fp,"%s\n",temp);
	}
	if(mb->user && strcmp(mb->user,"user.txt"))
		fprintf(fp,"user=%s\n",mb->user);
	if(mb->normal && strcmp(mb->normal,"normal.txt"))
		fprintf(fp,"normal=%s\n",mb->normal);
	if(mb->skip[0])
		fprintf(fp,"skip=%s\n",mb->skip);
	if(mb->bihua[0])
		fprintf(fp,"bihua=%s\n",mb->bihua);
	if(mb->ass_lead && mb->ass_main)
	{
		if(!mb->ass_lead0)
			fprintf(fp,"assist=%c %s\n",mb->ass_lead,mb->ass_main);
		else
			fprintf(fp,"assist=%c %s 1\n",mb->ass_lead,mb->ass_main);
	}
	else if(mb->ass_main)
		fprintf(fp,"assist=%s\n",mb->ass_main);
	if(mb->rule)
	{
		mb_rule_dump(mb,fp);
		fprintf(fp,"code_hint=%d\n",mb->code_hint);
	}
	fprintf(fp,"[DATA]\n");
}



int y_mb_dump(struct y_mb *mb,FILE *fp,int option,int format,char *pre)
{
	struct y_mb_index *index;

	if(!mb)
		return 0;

	if(format==MB_FMT_YONG && (option&MB_DUMP_HEAD))
	{
#if 1
		mb_head_dump(mb,fp);
#else
		fprintf(fp,"name=%s\n",mb->name);
		fprintf(fp,"key=%s\n",mb->key+1);
		if(mb->key0[0])
			fprintf(fp,"key0=%s\n",mb->key0);
		fprintf(fp,"len=%d\n",mb->len);
		if(mb->push[0])
		{
			fprintf(fp,"push=%s",mb->push);
			if(mb->stop)
			{
				int i;
				fprintf(fp," ");
				for(i=0;i<8;i++)
				{
					if(mb->stop & (1<<i))
						fprintf(fp,"%d",i);
				}
			}
			fprintf(fp,"\n");
		}
		if(mb->pull[0])
			fprintf(fp,"pull=%s\n",mb->pull);
		if(mb->suffix>=0)
		{
			if(mb->suffix==0)
				fprintf(fp,"suffix=NONE\n");
			else
				fprintf(fp,"suffix=%c\n",mb->suffix);
		}
		if(mb->match)
			fprintf(fp,"match=1\n");
		if(mb->wildcard_orig)
			fprintf(fp,"wildcard=%c\n",mb->wildcard);
		if(mb->dwf)
			fprintf(fp,"dwf=1\n");
		if(mb->english)
			fprintf(fp,"english=1\n");
		if(mb->simple)
			fprintf(fp,"simple=%d\n",mb->simple);
		if(mb->compact)
			fprintf(fp,"compact=%d\n",mb->compact);
		if(mb->yong)
			fprintf(fp,"yong=%d\n",mb->yong);
		if(mb->pinyin)
			fprintf(fp,"pinyin=%d\n",mb->pinyin);
		if(!mb->hint)
			fprintf(fp,"hint=0\n");
		if(mb->auto_clear)
			fprintf(fp,"auto_clear=%d\n",mb->auto_clear);
		if(mb->nomove[0])
			fprintf(fp,"nomove=%s\n",mb->nomove);
		if(mb->auto_move)
			fprintf(fp,"auto_move=%d\n",mb->auto_move);
		if(mb->nsort)
			fprintf(fp,"nsort=1\n");
		if(mb->sloop)
			fprintf(fp,"sloop=%d\n",mb->sloop);
		if(mb->split>0 && mb->split<10)
			fprintf(fp,"split=%d\n",mb->split);
		else if(mb->split)
			fprintf(fp,"split=%c\n",mb->split);
		if(mb->commit_mode || mb->commit_len || mb->commit_which)
			fprintf(fp,"commit=%d %d %d\n",mb->commit_mode,
				mb->commit_len,mb->commit_which);
		if(mb->dicts[0])
		{
			int i;
			fprintf(fp,"dicts=");
			for(i=0;i<10 && mb->dicts[i];i++)
			{
				if(i) fprintf(fp," ");
				fprintf(fp,"%s",mb->dicts[i]);
			}
			fprintf(fp,"\n");
		}
		if(mb->user && strcmp(mb->user,"user.txt"))
			fprintf(fp,"user=%s\n",mb->user);
		if(mb->normal && strcmp(mb->normal,"normal.txt"))
			fprintf(fp,"normal=%s\n",mb->normal);
		if(mb->skip[0])
			fprintf(fp,"skip=%s\n",mb->skip);
		if(mb->bihua[0])
			fprintf(fp,"bihua=%s\n",mb->bihua);
		if(mb->ass_lead && mb->ass_main)
		{
			if(!mb->ass_lead0)
				fprintf(fp,"assist=%c %s\n",mb->ass_lead,mb->ass_main);
			else
				fprintf(fp,"assist=%c %s 1\n",mb->ass_lead,mb->ass_main);
		}
		else if(mb->ass_main)
			fprintf(fp,"assist=%s\n",mb->ass_main);
		if(mb->rule)
		{
			mb_rule_dump(mb,fp);
			fprintf(fp,"code_hint=%d\n",mb->code_hint);
		}
		fprintf(fp,"[DATA]\n");
#endif
	}
	for(index=mb->index;index;index=index->next)
	{
		struct y_mb_item *it=index->item;
		while(it)
		{
			char *code;
			char *data;
			struct y_mb_ci *cp;
			code=mb_key_conv_r(mb,index->index,it->code);
			cp=it->phrase;
			if(!cp)
			{
				//fprintf(fp,"%s\n",code);
			}
			else if(mb->english)
			{
				fprintf(fp,"%s\n",y_mb_ci_string(cp));
			}
			else if(mb->pinyin && mb->split=='\'' && format==MB_FMT_YONG)
			{
				struct y_mb_ci *h=cp;
				int has_zi=0,has_ci=0,cped=0;
				for(;cp!=NULL;cp=cp->next)
				{
					if(cp->del)
						continue;
					if(cp->dic==Y_MB_DIC_MAIN && !(option&MB_DUMP_MAIN))
						continue;
					if(cp->dic==Y_MB_DIC_USER && !(option&MB_DUMP_USER))
						continue;
					if(cp->dic==Y_MB_DIC_TEMP && !(option&MB_DUMP_TEMP))
						continue;
					if(cp->dic==Y_MB_DIC_ASSIST)
						continue;
					if(cp->dic==Y_MB_DIC_SUB && !(option&MB_DUMP_DICTS))
						continue;
					data=y_mb_ci_string(cp);
					int revert=0;
					if(cp->zi)
					{
						has_zi=1;
						if(cp->len>1 && gb_is_normal((uint8_t*)data) == cp->ext)
							revert=1;
					}
					else
					{
						has_ci=1;
						continue;
					}
					if(!cped)
					{
						cped=1;
						if(pre) fprintf(fp,"%s",pre);
						fprintf(fp,"%s",code);
					}
					if(revert)
						fprintf(fp," ~%s",data);
					else
						fprintf(fp," %s",data);
				}
				if(has_zi)
					fprintf(fp,"\n");
				if(has_ci)
				{
					cped=0;
					int pos=0;
					for(cp=h;cp!=NULL;cp=cp->next,pos++)
					{
						if(cp->del)
							continue;
						if(cp->dic==Y_MB_DIC_MAIN && !(option&MB_DUMP_MAIN))
							continue;
						if(cp->dic==Y_MB_DIC_USER && !(option&MB_DUMP_USER))
							continue;
						if(cp->dic==Y_MB_DIC_TEMP && !(option&MB_DUMP_TEMP))
							continue;
						if(cp->dic==Y_MB_DIC_ASSIST)
							continue;
						if(cp->dic==Y_MB_DIC_SUB && !(option&MB_DUMP_DICTS))
							continue;
						if(cp->zi)
						{
							continue;
						}
						data=y_mb_ci_string(cp);
						if(has_zi)
						{
							fprintf(fp,"{%d}%s %s\n",pos,code,data);
						}
						else
						{
							if(!cped)
							{
								cped=1;
								if(pre) fprintf(fp,"%s",pre);
								fprintf(fp,"%s",code);
							}
							fprintf(fp," %s",data);
						}
					}
					if(!has_zi)
						fprintf(fp,"\n");
				}
			}
			else
			{
				int cped=0;

				while(cp)
				{
					if(cp->del)
						goto dump_next;
					if(cp->dic==Y_MB_DIC_MAIN)
					{
						if(!(option&MB_DUMP_MAIN))
							goto dump_next;
					}
					else if(cp->dic==Y_MB_DIC_USER)
					{
						if(!(option&MB_DUMP_USER))
							goto dump_next;
					}
					else if(cp->dic==Y_MB_DIC_TEMP)
					{
						if(!(option&MB_DUMP_TEMP))
							goto dump_next;
					}
					else if(cp->dic==Y_MB_DIC_ASSIST)
					{
						/* never dump assist mb */
						goto dump_next;
					}
					else if(!(option&MB_DUMP_DICTS))
					{
						goto dump_next;
					}
					data=y_mb_ci_string(cp);
					if(format==MB_FMT_YONG)
					{
						int revert=0;
						if(!cped)
						{
							cped=1;
							if(pre) fprintf(fp,"%s",pre);
							fprintf(fp,"%s",code);
						}
						if(cp->zi && cp->len>1)
						{
							//if(gb_is_gb2312((uint8_t*)data) == cp->ext)
							if(gb_is_normal((uint8_t*)data) == cp->ext)
								revert=1;
						}
						if(revert)
							fprintf(fp," ~%s",data);
						else
							fprintf(fp," %s",data);
					}
					else if(format==MB_FMT_WIN)
					{
						if(pre) fprintf(fp,"%s",pre);
						fprintf(fp,"%s%s\n",y_mb_ci_string(cp),code);
					}
					else if(format==MB_FMT_FCITX)
					{
						if(pre) fprintf(fp,"%s",pre);
						fprintf(fp,"%s %s\n",code,y_mb_ci_string(cp));
					}
					else if(format==MB_FMT_SCIM)
					{
						if(pre) fprintf(fp,"%s",pre);
						fprintf(fp,"%s\t%s\t0\n",code,y_mb_ci_string(cp));
					}
dump_next:
					cp=cp->next;
				}
				if(cped && format==MB_FMT_YONG)
					fprintf(fp,"\n");
			}
			it=it->next;
		}
	}
	if(format==MB_FMT_YONG && (option&MB_DUMP_ADJUST) && mb->zi)
	{
		int count=0;
		mb_hash_foreach(mb->zi,(LEnumFunc)mb_zi_virt_count,&count);
		if(count>0)
		{
			struct y_mb_zi **list,**p;
			int i;
			p=list=calloc(count,sizeof(struct y_mb_zi*));
			mb_hash_foreach(mb->zi,(LEnumFunc)mb_zi_virt_store,&p);
			qsort(list,count,sizeof(struct y_mb_zi*),mb_zi_virt_cmp);
			for(i=0;i<count;i++)
			{
				struct y_mb_zi *z=list[i];
				char code[Y_MB_KEY_SIZE+1],data[8];
				y_mb_code_get_string(mb,z->code,code);
				int len=l_char_to_gb(z->data,data);
				data[len]=0;
				fprintf(fp,"^%s %s\n",code,data);
			}
			free(list);
		}
	}
	return 0;
}

int y_mb_pick(struct y_mb *mb,FILE *fp,int option,int clen,int dlen,int filter,char *pre);
int y_mb_encode(struct y_mb *mb,FILE *fp,char *fn);
int y_mb_encode_py(struct y_mb *mb,FILE *fp,char *fn);
int y_mb_diff(struct y_mb *mb,FILE *fp,char *fn,int strict);

static inline char y_mb_code_n_key(struct y_mb *mb,struct y_mb_code *c,int i)
{
	if(i<0)
		i=c->len+i+1;
	if(c->len<=4)
	{
		return mb->key[(c->val>>(8+i*6))&0x3f];
	}
	else
	{
		return mb->key[c->data[i]];
	}
}

/* 获得按键最少的编码序列 */
static int mb_code_least(struct y_mb *mb,struct y_mb_zi *z,char *code)
{
	struct y_mb_code *c;
	int len;
	int ret=Y_MB_KEY_SIZE;
	int key;
	for(c=z->code;c;c=c->next)
	{
		int space=1;
		if(c->virt) continue;
		len=c->len;
		if((mb->yong && len==2) || (!mb->yong && len>1))
		{
			key=y_mb_code_n_key(mb,c,len-1);
			if(y_mb_is_stop(mb,key,len))
			{
				len--;
				space=0;
			}
		}
		if(len<ret)
		{
			ret=len;
			if(code)
			{
				y_mb_code_get_string(mb,c,code);
				if(space)
					strcat(code,"_");
			}
		}
	}
	return ret;
}

struct mb_stat{
	struct y_mb *mb;
	int level;
	int count[3];
};

static void mb_stat_cb(struct y_mb_zi *z,struct mb_stat *st)
{
	char *s;
	int len;
	
	s=mb_data_conv_r(z->data);
	if(gb_is_biaodian((uint8_t*)s))
		return;
	if(st->level && !gb_is_hz((uint8_t*)s))
		return;
	if(st->level==1 && !gb_is_hz1((uint8_t*)s))
		return;
	len=mb_code_least(st->mb,z,0);
	if(len<=3)
		st->count[len-1]++;
}

int y_mb_stat(struct y_mb *mb,FILE *fp,int level)
{
	struct mb_stat st={mb,level,{0,0,0}};
	
	if(!mb->zi)
		return -1;
	mb_hash_foreach(mb->zi,(LEnumFunc)mb_stat_cb,&st);
	fprintf(fp,"%d %d %d\n",st.count[0],st.count[1],st.count[2]);
	return 0;
}

int y_mb_play(struct y_mb *mb,FILE *fp,const char *fn)
{
	char line[4096];
	char code[Y_MB_KEY_SIZE+1];
	uint8_t *s;
	int len;
	FILE *in;
	struct y_mb_zi kz,*z;
	
	int zi_count=0;
	int key_count=0;
	int lost_count=0;
	
	if(!strcmp(fn,"-"))
		in=stdin;
	else
		in=fopen(fn,"r");
	if(!in)
		return -1;
		
	for(;(len=l_get_line(line,sizeof(line),in))>=0;)
	{
		if(len==0 || line[0]==0)
			continue;
		s=(uint8_t *)line;
		while(len>0)
		{
			if(len>=4 && gb_is_gb18030_ext(s))
			{
				kz.data=*(uint32_t*)s;
				z=mb_hash_find(mb->zi,&kz);
				if(!z)
				{
					lost_count++;
				}
				else
				{
					mb_code_least(mb,z,code);
					zi_count++;
					key_count+=strlen(code);
				}
				s+=4;
				len-=4;
			}
			else if(len>=2 && gb_is_biaodian(s))
			{
				s+=2;
				len-=2;
			}
			else if(len>=2 && gb_is_gbk(s))
			{
				kz.data=*(uint16_t*)s;
				z=mb_hash_find(mb->zi,&kz);
				if(!z)
				{
					lost_count++;
				}
				else
				{
					mb_code_least(mb,z,code);
					zi_count++;
					key_count+=strlen(code);
				}
				s+=2;
				len-=2;
			}
			else
			{
				s++;
				len--;
			}
		}
	}
	
	/* todo: 1,2,3 level count, select count */
	if(zi_count)
		fprintf(fp,"%.2f\n",(double)key_count/(double)zi_count);
	(void)lost_count;

	if(in!=stdin)
		fclose(in);
	return 0;
}

int y_mb_sort_file(const char *fn,int flag,const char *with)
{
	FILE *fp;
	LArray *list;
	char line[1024];
	int i;
	
	fp=fopen(fn,"rb");
	if(!fp) return 0;
	list=l_ptr_array_new(4096);
	while(!feof(fp))
	{
		int ret=l_get_line(line,sizeof(line),fp);
		if(ret==-1) break;
		char *p=l_strdup(line);
		l_ptr_array_append(list,p);
	}
	fclose(fp);
	if(with!=NULL)
	{
		char *p=l_strdup(with);
		l_ptr_array_append(list,p);
	}
	l_ptr_array_sort(list,(LCmpFunc)strcmp);	
	fp=fopen(fn,"w");
	if(fp!=NULL)
	{
		const char *prev=NULL;
		for(i=0;i<list->len;i++)
		{
			const char **p=l_array_nth(list,i);
			if(!flag || !prev || (prev && strcmp(prev,*p)))
				fprintf(fp,"%s\n",*p);
			prev=*p;
		}
		fclose(fp);
	}
	l_ptr_array_free(list,l_free);
	return 0;
}

#endif

#ifndef CFG_XIM_ANDROID
L_EXPORT(int tool_main(int arc,char **arg))
{
	struct y_mb *mb;
	int i;
	FILE *fp;
	
	if(arc>=2 && !strcmp(arg[0],"sort"))
	{
		int flag=0;
		const char *with=NULL;
		for(i=1;i<arc;i++)
		{
			if(!strcmp(arg[i],"--merge"))
				flag|=1;
			else if(!strncmp(arg[i],"--with=",7))
				with=arg[i]+7;
			else
				y_mb_sort_file(arg[i],flag,with);
		}
		return 0;
	}

	if(arc<3)
		return -1;
		
	y_mb_init();
	mb=y_mb_load(arg[0],MB_FLAG_SLOW,NULL);
	if(!mb)
	{
		y_mb_cleanup();
		return -2;
	}
	if(!strcmp(arg[1],"-"))
		fp=stdout;
	else
	{
		if(strstr(arg[1],".dat") || strstr(arg[1],".bin"))
			fp=y_mb_open_file(arg[1],"wb");
		else
			fp=y_mb_open_file(arg[1],"w");
	}
	if(!fp)
	{
		y_mb_free(mb);
		y_mb_cleanup();
		return -3;
	}
	if(!strcmp(arg[2],"dump"))
	{
		int option=0,format=MB_FMT_YONG;
		char *pre=NULL;
		for(i=3;i<arc;i++)
		{
			if(!strncmp(arg[i],"--format=",9))
			{
				if(!strcmp(arg[i]+9,"win"))
					format=MB_FMT_WIN;
				else if(!strcmp(arg[i]+9,"fcitx"))
					format=MB_FMT_FCITX;
				else if(!strcmp(arg[i]+9,"scim"))
					format=MB_FMT_SCIM;
				else if(!strcmp(arg[i]+9,"yong"))
					format=MB_FMT_YONG;
			}
			else if(!strncmp(arg[i],"--option=",9))
			{
				char **list;
				int j;
				list=l_strsplit(arg[i]+9,',');
				for(j=0;list[j];j++)
				{
					if(!strcmp(list[j],"main"))
						option|=MB_DUMP_MAIN;
					else if(!strcmp(list[j],"temp"))
						option|=MB_DUMP_TEMP;
					else if(!strcmp(list[j],"user"))
						option|=MB_DUMP_USER;
					else if(!strcmp(list[j],"dicts"))
						option|=MB_DUMP_DICTS;
					else if(!strcmp(list[j],"adjust"))
						option|=MB_DUMP_ADJUST;
					else if(!strcmp(list[j],"head"))
						option|=MB_DUMP_HEAD;
					else if(!strcmp(list[j],"all"))
						option|=MB_DUMP_ALL;
				}
				l_strfreev(list);
			}
			else if(!strncmp(arg[i],"--add=",6))
			{
				FILE *tmp;
				tmp=fopen(arg[i]+6,"r");
				if(tmp)
				{
					mb_load_data(mb,tmp,Y_MB_DIC_TEMP);
					option|=MB_DUMP_TEMP;
					fclose(tmp);
				}
			}
			else if(!strncmp(arg[i],"--prefix=",9))
			{
				pre=arg[i]+9;
			}
		}
		y_mb_dump(mb,fp,option,format,pre);
	}
	else if(!strcmp(arg[2],"pick"))
	{
		int option=0,clen=0,dlen=0,filter=0;
		char *pre=NULL;
		for(i=3;i<arc;i++)
		{
			if(!strncmp(arg[i],"--option=",9))
			{
				char **list;
				int j;
				list=l_strsplit(arg[i]+9,',');
				for(j=0;list[j];j++)
				{
					if(!strcmp(list[j],"main"))
						option|=MB_DUMP_MAIN;
					else if(!strcmp(list[j],"temp"))
						option|=MB_DUMP_TEMP;
					else if(!strcmp(list[j],"user"))
						option|=MB_DUMP_USER;
					else if(!strcmp(list[j],"dicts"))
						option|=MB_DUMP_DICTS;
					else if(!strcmp(list[j],"adjust"))
						option|=MB_DUMP_ADJUST;
					else if(!strcmp(list[j],"all"))
						option|=MB_DUMP_ALL;
				}
				l_strfreev(list);
			}
			else if(!strncmp(arg[i],"--prefix=",9))
			{
				pre=arg[i]+9;
			}
			else if(!strncmp(arg[i],"--dlen=",7))
			{
				dlen=atoi(arg[i]+7);
			}
			else if(!strncmp(arg[i],"--clen=",7))
			{
				clen=atoi(arg[i]+7);
			}
			else if(!strcmp(arg[i],"--normal"))
			{
				filter=1;
			}
		}
		//printf("%x %d %d\n",option,clen,dlen);
		y_mb_pick(mb,fp,option,clen,dlen,filter,pre);
	}
	else if(!strcmp(arg[2],"encode_py"))
	{
		for(i=3;i<arc;i++)
		{
			y_mb_encode_py(mb,fp,arg[i]);
		}
	}
	else if(!strcmp(arg[2],"encode"))
	{
		for(i=3;i<arc;i++)
		{
			y_mb_encode(mb,fp,arg[i]);
		}
	}
	else if(!strcmp(arg[2],"diff"))
	{
		int strict=0;
		for(i=3;i<arc;i++)
		{
			if(!strcmp(arg[i],"--code"))
			{
				strict=1;
				continue;
			}
			y_mb_diff(mb,fp,arg[i],strict);
		}
	}
	else if(!strcmp(arg[2],"stat"))
	{
		int level=0;
		for(i=3;i<arc;i++)
		{
			if(!strncmp(arg[i],"--level=",8))
			{
				if(!strcmp(arg[i]+8,"hz1"))
					level=1;
				else if(!strcmp(arg[i]+8,"hz12"))
					level=2;
			}
		}
		y_mb_stat(mb,fp,level);
	}
	else if(!strcmp(arg[2],"play"))
	{
		for(i=3;i<arc;i++)
		{
			y_mb_play(mb,fp,arg[i]);
		}
	}
	if(fp!=stdout)
		fclose(fp);
	y_mb_free(mb);
	return 0;
}
#endif
