#include "mb.h"
#define GB_LOAD_NORMAL
#include "llib.h"
#include "gbk.h"
#include "pinyin.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

#include "fuzzy.h"
#include "pyzip.h"

/* prev delcare */
int mb_load_data(struct y_mb *mb,FILE *fp,int dic);
static int mb_load_user(struct y_mb *mb,const char *fn);

static LSlices *mb_slices;

static inline void *mb_slice_alloc(size_t size)
{
	return l_slice_alloc(mb_slices,(int)size);
}

#define mb_slice_new(t) mb_slice_alloc(sizeof(t))

/* mb memory start */

static inline void *mb_malloc(size_t size)
{
	return l_slice_alloc(mb_slices,(int)size);
}

static inline void mb_free(void *p,size_t size)
{
	l_slice_free(mb_slices,p,size);
}

/* mb slist start */
typedef void *mb_slist_t;
#define mb_slist_free(t,h) mb_slice_free_chain(t,h)

static mb_slist_t mb_slist_insert_custom(mb_slist_t h,mb_slist_t n,int pos,bool (*cb)(void *))
{
	void *prev=NULL,*cur=h;
	for(int i=0;i<pos && cur;)
	{
		if(cb(cur))
			i++;
		prev=cur;
		cur=L_CPTR_NEXT(cur);
	}
	*(l_cptr_t*)n=L_CPTR_T(cur);
	if(prev)
	{
		*(l_cptr_t*)prev=L_CPTR_T(n);
		return h;
	}
	return n;
}

static int mb_slist_pos_custom(mb_slist_t h,mb_slist_t n,bool (*cb)(void *))
{
	void *cur=h;
	for(int i=0;cur;)
	{
		if(cur==n)
			return i;
		if(cb(cur))
			i++;
		cur=L_CPTR_NEXT(cur);
	}
	return -1;
}

static inline int mb_key_len2(const uint8_t *s)
{
	return bs_get_len(s);
}

static uint8_t *mb_key_conv2(struct y_mb *mb,const char *in,int len,uint8_t *p,int method)
{
	static uint8_t out[Y_MB_KEY_SIZE];
	if(!p) p=out;
	bs_zip(in,len,p,method,(const uint8_t*)mb->map);
	return p;
}

char *mb_key_conv2_r(struct y_mb *mb,uint16_t index,const uint8_t *in)
{
	static char out[Y_MB_KEY_SIZE+1];
	uint8_t temp[Y_MB_KEY_SIZE];
	int pos=0,len;
	if(index)
	{
		out[0]=mb->key[index>>8];
		out[1]=mb->key[index&0xff];
		if(out[1])
			pos=2;
		else
			pos=1;
	}
	len=mb_key_len2(in);
	in=bs_unzip(in,temp);
	for(int i=0;i<len;i++)
	{
		int c=in[i];
		out[pos++]=mb->key[c];
	}
	out[pos]=0;
	return out;
}

static bool mb_key_is_part2(const uint8_t *full,const uint8_t *part)
{
	int full_len=mb_key_len2(full);
	int part_len=mb_key_len2(part);
	if(full_len<part_len)
		return false;
	uint8_t temp1[Y_MB_KEY_SIZE];
	uint8_t temp2[Y_MB_KEY_SIZE];
	full=bs_unzip(full,temp1);
	part=bs_unzip(part,temp2);
	for(int i=0;i<part_len;i++)
	{
		if(full[i]!=part[i])
			return false;
	}
	return true;
}

static int mb_key_cmp_direct2(const uint8_t *s1,const uint8_t *s2,int n)
{
	int s1_len=mb_key_len2(s1);
	int s2_len=mb_key_len2(s2);
	int m=MIN(s1_len,s2_len);
	if(m>n) m=n;

	if(m>0)
	{
		uint8_t temp1[Y_MB_KEY_SIZE];
		uint8_t temp2[Y_MB_KEY_SIZE];
		s1=bs_unzip(s1,temp1);
		s2=bs_unzip(s2,temp2);

		assert(s1_len!=0 && s2_len!=0);

		for(int i=0;i<m;i++)
		{
			int c1=s1[i];
			int c2=s2[i];
			if(c1<c2)
				return -1;
			else if(c1>c2)
				return 1;
		}
	}
	if(m==n)
		return 0;
	if(s1_len<s2_len)
		return -1;
	else if(s1_len>s2_len)
		return 1;
	return 0;	
}

static int mb_key_cmp_wildcard2(const uint8_t *s1,const uint8_t *s2,int n)
{
	uint8_t temp1[Y_MB_KEY_SIZE];
	uint8_t temp2[Y_MB_KEY_SIZE];
	int s1_len=mb_key_len2(s1);
	int s2_len=mb_key_len2(s2);
	int m=MIN(s1_len,s2_len);
	if(m>n) m=n;

	s1=bs_unzip(s1,temp1);
	s2=bs_unzip(s2,temp2);

	for(int i=0;i<m;i++)
	{
		int c1=s1[i];
		int c2=s2[i];
		if(c1==Y_MB_WILDCARD || c2==Y_MB_WILDCARD)
			continue;
		if(c1<c2)
			return -1;
		else if(c1>c2)
			return 1;
	}
	if(m==n)
		return 0;
	if(s1_len<s2_len)
		return -1;
	else if(s1_len>s2_len)
		return 1;
	return 0;
}

void y_mb_key_map_init(const char *key,int wildcard,char *map)
{
	if(key)
	{
		int i,c;
		for(i=1;(c=key[i])!=0;i++)
			map[c]=i;
	}
	if(wildcard)
	{
		map[wildcard]=Y_MB_WILDCARD;
	}
}

char *y_mb_ci_string(struct y_mb_ci *ci)
{
	static char out[Y_MB_DATA_SIZE+1];
	int len=ci->len;
	memcpy(out,ci->data,ci->len);
	out[len]=0;
	return out;
}

int y_mb_ci_string2(struct y_mb_ci *ci,char *out)
{
	int len=ci->len;
	memcpy(out,ci->data,len);
	out[len]=0;
	return len;
}

static int mb_ci_equal(const struct y_mb_ci *ci,const char *data,int dlen)
{
	if(ci->len!=dlen)
		return 0;
	return memcmp((void*)ci->data,data,dlen)==0;
}

char enc_key[16];
static int need_enc;
L_EXPORT(int tool_set_key(void *arg,void **out))
{
	memcpy(enc_key,arg,16);
	need_enc=1;
	return 0;
}



static int get_line_iconv(char *line, size_t n, FILE *fp)
{
	char temp[4096];
	int len;
	if(!fgets(temp,sizeof(temp),fp))
		return -1;
	l_utf8_to_gb(temp,line,n);
	len=strcspn(line,"\r\n");
	line[len]=0;
	return len;
}

static int mb_add_rule(struct y_mb *mb,const char *s)
{
	struct y_mb_rule *r;
	
	r=l_alloc0(sizeof(*r));
	char c=*s++;
	if(c=='a') r->a=1;
	else if(c=='e') r->a=0;
	else goto err;
	c=*s++;
	if(c<'1' || c>'9') goto err;
	r->n=c-'1'+1;
	c=*s++;
	if(r->n==1 && c>='0' && c<='5')
	{
		r->n=10+c-'0';
		c=*s++;
	}
	if(c!='=') goto err;

	for(int i=0;i<Y_MB_KEY_SIZE;i++)
	{
		c=*s++;
		if(c=='p')
			r->code[i].r=0;
		else if(c=='n')
			r->code[i].r=1;
		else if(c && mb->map[(int)c])
		{
			r->code[i].d=1;
			r->code[i].p=c;
			goto next;
		}
		else
		{
			printf("not pn. %c\n",c);
			goto err;
		}
		c=*s++;
		if(c=='.')
			r->code[i].i=Y_MB_WILDCARD;
		else if(c>='1' && c<='9')
		{
			r->code[i].i=c-'1'+1;
			if(r->code[i].i > r->n || r->code[i].i<1)
			{
				printf("out of range\n");
				goto err;
			}
		}
		else
		{
			printf("1-9.\n");
			goto err;
		}
		c=*s++;
		if(c=='.')
			r->code[i].p=Y_MB_WILDCARD;
		else if(c>='1' && c<='9')
			r->code[i].p=c-'1'+1;
		else if(c=='-' && (c=*s++)>='1' && c<='9')
			r->code[i].p=-(c-'1'+1);
		else
		{
			printf("not 1-9.\n");
			goto err;
		}
next:
		c=*s++;
		if(c==0) break;
		else if(c=='+') continue;
		else
		{
			printf("not end or next\n");
			goto err;
		}
	}
	mb->rule=l_slist_append(mb->rule,r);
	return 0;
err:
	l_free(r);
	return -1;
}

void mb_rule_dump(struct y_mb *mb,FILE *fp)
{
	struct y_mb_rule *r=mb->rule;
	int i;
	if(!r)
		return;

	while(r)
	{
		if(!r->n)
			break;
		fprintf(fp,"code_%c%d=",r->a?'a':'e',r->n);
		for(i=0;r->code[i].p;i++)
		{
			if(i!=0)
				fprintf(fp,"+");
			if(r->code[i].d)
			{
				fprintf(fp,"%c",r->code[i].p);
				continue;
			}
			fprintf(fp,"%c",r->code[i].r?'n':'p');
			if(r->code[i].i==Y_MB_WILDCARD)
				fprintf(fp,".");
			else
				fprintf(fp,"%d",r->code[i].i);
			if(r->code[i].p==Y_MB_WILDCARD)
				fprintf(fp,".");
			else
				fprintf(fp,"%d",r->code[i].p);
		}
		fprintf(fp,"\n");
		r=r->next;
	}
}

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

void y_mb_code_get_string(const struct y_mb *mb,const struct y_mb_code *c,char *out)
{
	int len=c->len,i;
	if(len<=4)
	{
		int val=c->val;
		for(i=0;i<len;i++)
			out[i]=mb->key[(val>>(8+i*6))&0x3f];
	}
	else
	{
		for(i=0;i<len;i++)
			out[i]=mb->key[c->data[i]];
	}
	out[len]=0;
}

int y_mb_code_cmp(const struct y_mb_code *c1,const struct y_mb_code *c2,int len)
{
	int i,mlen;
	mlen=MIN(c1->len,c2->len);
	for(i=0;i<mlen;i++)
	{
		int t1,t2,ret;
		t1=(c1->len<=4)?((c1->val>>(8+i*6))&0x3f):c1->data[i];
		t2=(c2->len<=4)?((c2->val>>(8+i*6))&0x3f):c2->data[i];
		ret=t1-t2;
		if(ret) return ret;
	}
	if(mlen<len)
	{
		return c1->len-c2->len;
	}
	return 0;
}

static bool mb_code_in_skip(struct y_mb *mb,struct y_mb_code *c)
{
	char p=y_mb_code_n_key(mb,c,0);
	if(strchr(mb->skip,p))
		return true;
	if(mb->skip[0]=='*')
	{
		for(int i=1;i<c->len;i++)
		{
			if(strchr(mb->skip+1,y_mb_code_n_key(mb,c,i)))
			{
				return true;
			}
		}
	}
	return false;
}

static struct y_mb_code *mb_code_for_rule(struct y_mb *mb,struct y_mb_code *c,int n,const char *hint)
{
	struct y_mb_code *ret=NULL;
	if(n==Y_MB_WILDCARD)
		return c;
	if(n<0)
		n=-n;
	if(hint && hint[0])
	{
		struct y_mb_code *save=c;
		for(;c!=NULL;c=L_CPTR(c->next))
		{
			if(y_mb_code_n_key(mb,c,0)!=hint[0])
				continue;
			if(hint[1] && y_mb_code_n_key(mb,c,1)!=hint[1])
				continue;
			if(ret && c->len<=ret->len)
				continue;
			if(mb_code_in_skip(mb,c))
				continue;
			ret=c;
			if(c->virt)
				break;
		}
		if(ret)
			return ret;
		c=save;
	}
	for(;c!=NULL;c=L_CPTR(c->next))
	{
		if(c->virt)
		{
			if(c->len<n)
				return NULL;
			ret=c;
			break;
		}
		if(c->len<n)
			continue;
		if(ret && c->len<=ret->len)
			continue;
		if(mb_code_in_skip(mb,c))
			continue;
		ret=c;
	}
	return ret;
}

int y_mb_code_by_rule(struct y_mb *mb,const char *s,int len,char *outs[],char hint[][2])
{
	struct y_mb_zi *z[Y_MB_DATA_SIZE+1];
	int i;
	struct y_mb_rule *r;
	int ret=-1;
	int outp;
	char *out;

	if(unlikely(mb->english))
	{
		outp=0;
		out=outs[outp];
		if(!out)
			return -1;
		for(i=0;i<len;i++)
		{
			int c=s[i];
			if(c>='A' && c<='Z' && !y_mb_is_key(mb,c))
				c='a'+(c-'A');
			if(!y_mb_is_key(mb,c))
			{
				out[0]=0;
				return -1;
			}
			out[i]=c;
		}
		out[i]=0;
		return 0;
	}

	r=mb->rule;
	if(!r || len<2 || len%2)
	{
		//printf("bad len %d line: %s\n",len,s);
		return -1;
	}
	/* calc hz count */
	// len>>=1;
	len=l_gb_strlen(s,len);
	/* find hz info */
	for(i=0;i<len;i++)
	{
		uint32_t hz=l_gb_to_char(s);
		s=l_gb_next_char(s);
		if(!s)
		{
			// printf("not pure gb18030 string %d/%d\n",i,len);
			return -1;
		}
		z[i]=L_HASH_TABLE_LOOKUP_INT(mb->zi,hz);
		if(!z[i])
		{
			// printf("not found zi %04x\n",l_read_u32be(&kz.data));
			return -1;
		}
	}
	outp=0;out=outs[outp];
	for(;r&&out;r=r->next)
	{
		int pos=0;
		struct y_mb_code *c;
		if(!r->a && r->n!=len)
			continue;
		if(r->a && r->n>len)
			continue;
		if(pos+1>Y_MB_KEY_SIZE)
			return -1;
		for(i=0;i<Y_MB_KEY_SIZE+1;i++)
		{
			if(r->code[i].p==0)
				break;
			if(r->code[i].d)
			{
				out[pos++]=r->code[i].p;
			}
			else if(r->code[i].i==Y_MB_WILDCARD)
			{
				int j;
				if(i!=0)
					break;
				for(j=0;j<len;j++)
				{
					c=L_CPTR(z[j]->code);					
					if(r->code[i].p!=Y_MB_WILDCARD)
					{
						const char *h=NULL;
						if(mb->code_hint && hint)
							h=hint[r->code[i].i-1];
						c=mb_code_for_rule(mb,c,r->code[i].p,h);
						if(pos+1>Y_MB_KEY_SIZE)
						{
							pos=0;
							break;
						}
						out[pos++]=y_mb_code_n_key(mb,c,r->code[i].p-1);
					}
					else
					{
						int len;
						if(c->len+pos>Y_MB_KEY_SIZE)
						{
							pos=0;
							break;
						}
						len=(c->len>mb->len)?mb->len:c->len;
						if(pos+len>Y_MB_KEY_SIZE)
							return -1;
						y_mb_code_get_string(mb,c,out+pos);
						pos+=len;
					}
				}
				break;
			}
			else if(r->code[i].p==Y_MB_WILDCARD)
			{
				if(r->code[i].r)
					c=L_CPTR(z[len-r->code[i].i]->code);
				else
					c=L_CPTR(z[r->code[i].i-1]->code);
				if(c->len+pos>Y_MB_KEY_SIZE)
				{
					pos=0;
					break;
				}
				if(pos+c->len>Y_MB_KEY_SIZE)
					return -1;
				y_mb_code_get_string(mb,c,out+pos);
				pos+=c->len;	
			}
			else
			{
				if(r->code[i].r)
					c=L_CPTR(z[len-r->code[i].i]->code);
				else
					c=L_CPTR(z[r->code[i].i-1]->code);
				const char *h=NULL;
				if(mb->code_hint && hint)
				{
					if(r->code[i].r)
						h=hint[len-r->code[i].i];
					else
						h=hint[r->code[i].i-1];
				}
				// printf("%d %02x%02x\n",r->code[i].i,h[0],h[1]);
				c=mb_code_for_rule(mb,c,r->code[i].p,h);
				if(!c)
				{
					pos=0;
					break;
				}
				out[pos++]=y_mb_code_n_key(mb,c,r->code[i].p-1);
			}
		}
		out[pos]=0;
		if(out[0]) ret=0;
		if(pos>0)
		{
			out=outs[++outp];
		}
	}
	return ret;
}

static struct y_mb_code *mb_code_new(struct y_mb *mb,const char *s,int len)
{
	struct y_mb_code *c;
	int i;
	int size=sizeof(struct y_mb_code);
	if(len>4) size+=len-3;
	c=mb_malloc(size);
	c->val=0;
	c->len=len;
	if(len<=4)
	{
		for(i=0;i<len;i++)
			c->val|=mb->map[(int)s[i]]<<(i*6+8);
	}
	else
	{
		for(i=0;i<len;i++)
			c->data[i]=mb->map[(int)s[i]];
	}
	return c;
}

static void mb_code_free(struct y_mb_code *c)
{
	int size=sizeof(struct y_mb_code);
	if(c->len>4)
		size+=c->len-3;
	mb_free(c,size);
}

static void mb_zi_free(struct y_mb_zi *p)
{
	l_cslist_free(L_CPTR(p->code),(LFreeFunc)mb_code_free);
	l_slice_free(mb_slices,p,sizeof(struct y_mb_zi));
}

static struct y_mb_ci *mb_ci_new(const char *data,int dlen)
{
	int size;
	if(dlen<=2)
		size=sizeof(struct y_mb_ci);
	else
		size=sizeof(struct y_mb_ci)-2+dlen;
	struct y_mb_ci *c=mb_malloc(size);
	c->len=dlen;
	memcpy(c->data,data,dlen);
	return c;
}

static void mb_ci_free(struct y_mb_ci *ci)
{
	int size=sizeof(struct y_mb_ci)-2+ci->len;
	mb_free(ci,size);
}

static inline uint16_t mb_ci_index(struct y_mb *mb,const char *code,int len,uint8_t **key)
{
	int c1,c2;
	c1=mb->map[(int)code[0]];
	if(len==1 || mb->nsort)
		c2=0;
	else
		c2=mb->map[(int)code[1]];
	if(key)
	{
		if(mb->nsort)
			*key=mb_key_conv2(mb,code+1,len-1,NULL,0);
		else
			*key=mb_key_conv2(mb,code+2,len-2,NULL,0);
	}
	return (uint16_t)(c1<<8)|c2;
}

static inline int mb_ci_index_code_len(uint16_t index)
{
	if(index&0xff)
		return 2;
	else
		return 1;
}

#define mb_ci_index_wildcard(mb,code,len,wildcard,key) mb_ci_index(mb,code,len,key)

static inline int mb_index_cmp_direct(uint16_t i1,uint16_t i2,int n)
{
	if(n==1)
		return (i1>>8)-(i2>>8);
	else
		return i1-i2;
}

/* wildcard in i2 is not treated as wildcard */
static int mb_index_cmp_wildcard(struct y_mb *mb,uint16_t i1,uint16_t i2,int n,char wildcard)
{
	int ret;
	int c1,c2;
	
	if(!wildcard)
		return mb_index_cmp_direct(i1,i2,n);

	/* if not equal length */
	if(!(i1&0xff) != !(i2&0xff))
		return 1;

	c1=i1>>8;c2=i2>>8;
	if(n==1)
	{
		if(!mb->dwf && c1==Y_MB_WILDCARD)
			return 0;
		return c1-c2;
	}
	ret=c1-c2;
	if(ret && (c1!=Y_MB_WILDCARD || mb->dwf))
		return ret;
	c1=i1&0xff;c2=i2&0xff;
	ret=c1-c2;
	if(!c1 || !c2)
		return ret;
	if(c1==Y_MB_WILDCARD)
		return 0;
	return ret;
}

static void mb_item_free1(struct y_mb_item *it)
{
	l_cslist_free(L_CPTR(it->phrase),(LFreeFunc)mb_ci_free);
	int code_size=bs_get_alloc_size(it->code);
	mb_free(it,sizeof(struct y_mb_item)+code_size);
}

struct y_mb_zi *mb_find_zi(struct y_mb *mb,const char *s)
{
	if(s[0]=='$')
		s=y_mb_skip_display(s,-1);
	uint32_t data=l_gb_to_char(s);
	struct y_mb_zi *z=L_HASH_TABLE_LOOKUP_INT(mb->zi,data);
	return z;
}

int y_mb_find_code(struct y_mb *mb,const char *hz,char (*tab)[MAX_CAND_LEN+1],int max)
{
	int len;
	struct y_mb_zi *z;
	struct y_mb_code *p;
	int i;
	
	if(!mb->zi) return 0;
	
	len=strlen(hz);
	if(len!=2 && len!=4)
		return 0;
	z=mb_find_zi(mb,hz);
	if(!z)
		return 0;
	for(p=L_CPTR(z->code),i=0;p && i<max;p=L_CPTR(p->next))
	{
		if(p->virt)
		{
			continue;
		}
		if(mb->flag & MB_FLAG_ASSIST)
		{
			tab[i][0]='@';
			y_mb_code_get_string(mb,p,tab[i]+1);
		}
		else
		{
			y_mb_code_get_string(mb,p,tab[i]);
		}
		i++;
	}
	if(mb->ass_mb && i<max-1)
	{
		i+=y_mb_find_code(mb->ass_mb,hz,tab+i,max-i);
	}
	else if(mb->flag & MB_FLAG_ADICT && i<max-1)
	{
		struct y_mb_index *index;
		struct y_mb_item *item;
		struct y_mb_ci *c;
		for(index=mb->index;index;index=L_CPTR(index->next))
		{
			if(index->ci_count==0)
				continue;
			for(item=L_CPTR(index->item);item;item=L_CPTR(item->next))
			{
				for(c=L_CPTR(item->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del || !c->zi || c->dic!=Y_MB_DIC_ASSIST)
						continue;
					if(!mb_ci_equal(c,hz,len))
						continue;
					char *code=mb_key_conv2_r(mb,index->index,item->code);
					sprintf(tab[i],"@%s",code);
					i++;
					if(i>=max-1) goto out;
				}
			}
		}
out:;
	}
	return i;
}

int y_mb_find_simple_code(struct y_mb *mb,const char *hz,const char *code,char *out,int filter,int total)
{
	int minimum;
	struct y_mb_context ctx;
	
	if(!mb->zi) return 0;
	
	int len=l_gb_strlen(hz,-1);
	if(len!=1)
		return 0;
	struct y_mb_zi *z=mb_find_zi(mb,hz);
	if(!z)
		return 0;
	len=minimum=strlen(code);
	y_mb_push_context(mb,&ctx);
	for(struct y_mb_code *p=L_CPTR(z->code);p!=NULL;p=L_CPTR(p->next))
	{
		struct y_mb_ci *c;
		int pos;
		if(p->virt)
			continue;
		if(p->len>minimum)
			continue;

		int clen=p->len;
		char temp[clen+1];
		y_mb_code_get_string(mb,p,temp);
		if(0==y_mb_set(mb,temp,clen,filter))
			continue;
		for(pos=0,c=L_CPTR(mb->ctx.result_first->phrase);c!=NULL && pos<=total;c=L_CPTR(c->next))
		{
			if(c->del) continue;
			if(filter && c->zi && c->ext) continue;
			pos++;
			if(!c->zi) continue;
			const char *s=y_mb_skip_display((const char*)c->data,c->len);
			if(l_gb_to_char(s)!=z->data) continue;
			break;
		}
		if(!c || pos>total) continue;
		if(y_mb_is_stop(mb,temp[clen-1],clen) && !y_mb_has_next(mb,filter))
			clen--;
		if(clen>=minimum) continue;
		if(!strcmp(code,temp)) continue;
		strcpy(out,temp);
		minimum=clen;
	}
	y_mb_pop_context(mb,&ctx);
	if(minimum<len)
		return 1;
	return 0;
}

const char *y_mb_skip_display(const char *s,int len)
{
	if(s[0]!='$' || s[1]!='[')
		return s;
	const char *end=s+2;
	int surround=1;
	while(1)
	{
		uint32_t hz=l_gb_to_char(end);
		end=l_gb_next_char(end);
		if(!end || hz==' ')
			return s;
		if(hz=='$')
		{
			hz=l_gb_to_char(end);
			end=l_gb_next_char(end);
			if(!end)
				return s;
			if(hz=='[' || hz==']')
				continue;
		}
		if(hz=='[')
		{
			surround++;
		}
		if(hz==']')
		{
			surround--;
			if(surround==0)
				break;
		}
	}
	return end;
}

int y_mb_zi_has_code(struct y_mb *mb,const char *zi,const char *code)
{
	uint32_t hz=l_gb_to_char((const uint8_t*)zi);
	struct y_mb_zi *z=L_HASH_TABLE_LOOKUP_INT(mb->zi,hz);
	if(!z)
	{
		return 0;
	}
	struct y_mb_code *c=L_CPTR(z->code);
	while(c!=NULL)
	{
		char temp[32];
		y_mb_code_get_string(mb,c,temp);
		if(!strcmp(temp,code))
		{
			return 1;
		}
		c=L_CPTR(c->next);
	}
	return 0;
}


/* 如果设定了组词码，那么就只有组词码才是好的编码 */
int y_mb_is_good_code(struct y_mb *mb,const char *code,const char *s)
{
	struct y_mb_zi *z;
	struct y_mb_code *c;
	char key[Y_MB_KEY_SIZE+1];
	int clen;
		
	z=mb_find_zi(mb,s);
	if(!z || !z->code) return 1;
	c=L_CPTR(z->code);
	if(!c->virt) return 1;
	clen=strlen(code);
	if(clen>c->len)
		return 0;
	y_mb_code_get_string(mb,c,key);
	return !strncmp(code,key,clen);
}

int y_mb_get_full_code(struct y_mb *mb,const char *data,char *code)
{
	struct y_mb_zi *z;
	struct y_mb_code *full=0,*p,*virt=0;
	int virt_found=0;

	z=mb_find_zi(mb,data);
	if(!z)
		return -1;
	for(p=L_CPTR(z->code);p;p=L_CPTR(p->next))
	{
		if(p->virt)
		{
			virt=p;
			continue;
		}
		if(p->len>mb->len)
			continue;
		if(!full)
		{
			full=p;
		}
		else if(p->len>=full->len)
		{
			if(!virt_found)
				full=p;
			if(virt && virt->len<=p->len && 
					!y_mb_code_cmp(virt,p,virt->len))
			{
				full=p;
				virt_found=1;
			}
		}
		else
		{
			if(!virt || virt_found) continue;
			if(virt->len<=p->len && !y_mb_code_cmp(virt,p,virt->len))
			{
				full=p;
				virt_found=1;
			}
		}
	}
	if(!full && virt) full=virt;
	if(!full) return -1;
	y_mb_code_get_string(mb,full,code);
	return 0;
}

void y_mb_calc_yong_tip(struct y_mb *mb,const char *code,const char *cand,char *tip)
{
	int len;
	struct y_mb_zi *z;
	struct y_mb_code *p;
	char temp[Y_MB_KEY_SIZE+1];
	int prev=5;
	
	z=mb_find_zi(mb,cand);
	if(!z)
	{
		return;
	}
	len=strlen(code);
	for(p=L_CPTR(z->code);p;p=L_CPTR(p->next))
	{
		if(p->virt || p->len<=len || p->len>prev)
			continue;			
		y_mb_code_get_string(mb,p,temp);
		if(!memcmp(code,temp,len))
		{
			if(!tip[0] || mb->map[(int)temp[len]]<mb->map[(int)tip[0]])
				tip[0]=temp[len];
			tip[1]=0;
			prev=p->len;
		}
	}
	return;
}

static struct y_mb_zi *mb_add_zi(struct y_mb *mb,const char *code,int clen,const char *data,int dlen,int mv)
{
	struct y_mb_code *c;
	struct y_mb_zi *z;
	int virt=(mv&0x01);

	if(!mb->zi)
		return NULL;
	
	z=mb_find_zi(mb,data);
	if(z)
	{
		for(struct y_mb_code *p=L_CPTR(z->code);p;p=L_CPTR(p->next))
		{
			/* code equal is the same virt and code it self */
			char temp[Y_MB_KEY_SIZE+1];
			if(clen!=p->len || virt!=p->virt)
				continue;
			y_mb_code_get_string(mb,p,temp);
			if(!memcmp(temp,code,clen))
			{
				z->code=L_CPTR_T(l_cslist_remove(L_CPTR(z->code),p));
				mb_code_free(p);
				break;
			}
		}
	}
	else
	{
		uint32_t key;
		if(mv==-1)
			return NULL;
		data=y_mb_skip_display(data,dlen);			
		key=l_gb_to_char(data);
		z=mb_slice_new(struct y_mb_zi);
		if(!z)
			return NULL;
		z->code=0;
		z->data=key;
		l_hash_table_insert(mb->zi,z);
	}
	if(mv==-1)
		return NULL;
	c=mb_code_new(mb,code,clen);
	c->virt=virt;
	struct y_mb_code *h=L_CPTR(z->code);
	if(c->virt || !h || !h->virt)
	{
		z->code=L_CPTR_T(l_cslist_prepend(h,c));
	}
	else
	{
		z->code=L_CPTR_T(l_cslist_append(h,c));
	}
	return z;
}

static struct y_mb_index *mb_get_index(struct y_mb *mb,uint16_t code)
{
	struct y_mb_index *p;
	int ret;
	
	if((p=mb->half)!=NULL)
	{
		ret=(int)p->index-(int)code;
		if(ret==0) return p;
		else if(ret>0) p=mb->index;
	}
	else
	{
		p=mb->index;
	}
	while(p)
	{
		ret=(int)p->index-(int)code;
		if(ret==0)
			return p;
		if(ret>0)
			break;
		p=L_CPTR(p->next);
	}
	return 0;
}

static struct y_mb_item *mb_get_item(struct y_mb_index *index,uint8_t *key)
{
	struct y_mb_item *p;
	int ret;
	
	if((p=L_CPTR(index->half))!=NULL)
	{
		ret=mb_key_cmp_direct2(p->code,key,Y_MB_KEY_SIZE);
		if(ret==0) return p;
		else if(ret>0) p=L_CPTR(index->item);
		else p=L_CPTR(p->next);
	}
	else
	{
		p=L_CPTR(index->item);
	}
	for(;p!=NULL;p=L_CPTR(p->next))
	{
		ret=mb_key_cmp_direct2(p->code,key,Y_MB_KEY_SIZE);
		if(ret==0)
			return p;
		if(ret>0)
			break;
	}
	return 0;
}

static void mb_index_free1(struct y_mb_index *index)
{
	l_cslist_free(L_CPTR(index->item),(LFreeFunc)mb_item_free1);
	mb_free(index,sizeof(struct y_mb_index));
}

static struct y_mb_index *mb_add_index(struct y_mb *mb,uint16_t code)
{
	struct y_mb_index *p,*prev,*n;
	
	prev=NULL;
	p=mb->index;
	while(p)
	{
		int ret=p->index-code;
		if(ret==0)
			return p;
		if(ret>0)
			break;
		prev=p;
		p=L_CPTR(p->next);
	}
	n=mb_slice_new(struct y_mb_index);
	if(!n)
		return NULL;
	n->next=L_CPTR_T(p);
	n->item=l_cptr_null;
	n->half=l_cptr_null;
	n->ci_count=0;
	n->zi_count=0;
	n->ext_count=0;
	n->index=code;
	if(prev)
		prev->next=L_CPTR_T(n);
	else
		mb->index=n;
	return n;
}

static struct y_mb_item *mb_add_one_code_nsort(struct y_mb *mb,struct y_mb_index *index,const char *code,int clen,int pos)
{
	struct y_mb_item *it;
	int method=mb->key_compress;
	int code_size=bs_alloc_size(clen-1,&method);
	it=mb_slice_alloc(sizeof(struct y_mb_item)+code_size);
	if(!it)
		return NULL;
	it->next=0;
	it->phrase=0;
	mb_key_conv2(mb,code+1,clen-1,it->code,method);
	if(pos==0)
	{
		index->item=L_CPTR_T(l_cslist_prepend(L_CPTR(index->item),it));
	}
	else if(pos==Y_MB_APPEND)
	{
		if(mb->last_index==index && mb->last_link)
		{
			l_cslist_insert_after(L_CPTR(index->item),mb->last_link,it);
			mb->last_link=it;
		}
		else
		{
			index->item=L_CPTR_T(l_cslist_insert(L_CPTR(index->item),it,Y_MB_APPEND));
			mb->last_link=it;
			mb->last_index=index;
		}
	}
	else
	{
		index->item=L_CPTR_T(l_cslist_insert(L_CPTR(index->item),it,pos));
	}
	return it;
}

static struct y_mb_item *mb_add_one_code(struct y_mb *mb,struct y_mb_index *index,const char *code,int clen)
{
	int method=mb->key_compress;
	uint8_t *key;
	struct y_mb_item *h,*l,*it;
	int ret;
	h=l=L_CPTR(index->item);
	if(!l || clen<=2)
	{
		if(!l || l->code[0]!=0)
		{
			int code_size=bs_alloc_size(clen-2,&method);
			it=mb_slice_alloc(sizeof(struct y_mb_item)+code_size);
			if(!it)
				return NULL;
			it->phrase=l_cptr_null;
			it->next=l_cptr_null;
			mb_key_conv2(mb,code+2,clen-2,it->code,method);
			l=l_cslist_prepend(l,it);
			index->item=L_CPTR_T(l);
		}
		mb->last_index=index;
		mb->last_link=l;
		return l;
	}
	key=mb_key_conv2(mb,code+2,clen-2,NULL,0);
	if(mb->last_index==index && mb->last_link)
	{
		it=mb->last_link;
		ret=mb_key_cmp_direct2(key,it->code,Y_MB_KEY_SIZE);
		if(ret==0) /* just got it */
			return it;
		else if(ret>0) /* search from last */
		{
			l=it;
			goto NEXT;
		}
	}
	// compare with head
	ret=mb_key_cmp_direct2(key,l->code,Y_MB_KEY_SIZE);
	if(ret==0)
	{
		mb->last_index=index;
		mb->last_link=l;
		return l;
	}
	else if(ret<0)
	{
		int code_size=bs_alloc_size(clen-2,&method);
		it=mb_slice_alloc(sizeof(struct y_mb_item)+code_size);
		if(!it)
			return NULL;
		memset(it->code,0,code_size);
		if(method==0)
			memcpy(it->code,key,code_size);
		else
			mb_key_conv2(mb,code+2,clen-2,it->code,method);
		it->phrase=l_cptr_null;
		it->next=l_cptr_null;
		index->item=L_CPTR_T(l_cslist_insert_before(h,l,it));
		mb->last_index=index;
		mb->last_link=it;
		return it;
	}
NEXT:
	while(1)
	{
		if(l->next)
		{
			struct y_mb_item *it=L_CPTR(l->next);
			ret=mb_key_cmp_direct2(key,it->code,Y_MB_KEY_SIZE);
		}
		if(!l->next || ret<0)
		{
			int code_size=bs_alloc_size(clen-2,&method);
			assert(code_size>=4);
			it=mb_slice_alloc(sizeof(struct y_mb_item)+code_size);
			if(!it)
				return NULL;
			memset(it->code,0,code_size);
			if(method==0)
				memcpy(it->code,key,code_size);
			else
				mb_key_conv2(mb,code+2,clen-2,it->code,method);
			it->phrase=l_cptr_null;
			it->next=l_cptr_null;
			index->item=L_CPTR_T(l_cslist_insert_after(h,l,it));
			mb->last_index=index;
			mb->last_link=it;
			return it;
		}
		else if(ret==0)
		{
			mb->last_index=index;
			mb->last_link=L_CPTR(l->next);
			return L_CPTR(l->next);
		}
		l=L_CPTR(l->next);
	}
	/* should never here */
	return NULL;
}

static struct y_mb_item *mb_get_one_code(struct y_mb *mb,struct y_mb_index *index,const char *code,int clen)
{
	uint8_t *key;
	struct y_mb_item *it;
	int ret;

	it=L_CPTR(index->item);
	if(!it)
	{
		return NULL;
	}
	/* even clen<2,it can get the good result */
	if(mb->nsort)
		key=mb_key_conv2(mb,code+1,clen-1,NULL,0);
	else
		key=mb_key_conv2(mb,code+2,clen-2,NULL,0);
	for(;it;it=L_CPTR(it->next))
	{
		ret=mb_key_cmp_direct2(key,it->code,Y_MB_KEY_SIZE);
		if(ret==0)
		{
			return it;
		}
		else if(ret<0)
		{
			break;
		}
	}
	return NULL;
}

/* only normal data should use this */
static struct y_mb_ci *mb_app_one(struct y_mb *mb,struct y_mb_ci *pos,const char *code,int clen,const char *data,int dlen,int dic)
{
	struct y_mb_ci *c;
	struct y_mb_index *index;
	int revert=0;

	if(data[0]=='~')
	{
		if(dlen==3 && gb_is_gbk((uint8_t*)data+1))
			revert=1;
		else if(dlen==5 && gb_is_gb18030_ext((uint8_t*)data+1))
			revert=1;
		if(revert)
		{
			data++;
			dlen--;
		}
	}
	else if(code[0]=='^') /* is it used to construct ci */
	{
		if(dic==Y_MB_DIC_ASSIST)
			return NULL;
		if((dlen==2 && gb_is_gbk((uint8_t*)data)) ||
					(dlen==4 && gb_is_gb18030_ext((uint8_t*)data)))
		{
			
			if(dic==Y_MB_DIC_MAIN)
			{
				int mv=1;
				code+=1;clen-=1;
				mb_add_zi(mb,code,clen,data,dlen,mv);
			}
			return NULL;
		}
		else
		{
			return NULL;
		}
	}

	c=mb_ci_new(data,dlen);
	c->dic=dic;
	c->del=0;
	c->zi=c->ext=0;
	c->simp=0;
	
	if(data[0]=='$' && data[1]=='[')
	{
	
		const char *real=y_mb_skip_display(data,dlen);
		int rlen=dlen-(int)(size_t)(real-data);
		if((rlen==2 && gb_is_gbk((uint8_t*)real)) ||
					(rlen==4 && gb_is_gb18030_ext((uint8_t*)real)))
		{
			c->zi=1;
			c->ext=!gb_is_normal((uint8_t*)real);
			if(revert) c->ext=!c->ext;
			if(dic==Y_MB_DIC_MAIN)
				mb_add_zi(mb,code,clen,real,rlen,0);
		}
		else
		{
			if(rlen==1)
				c->zi=1;
			else if(data[0]=='$')
				c->ext=1;
		}
	}
	else
	{
		if((dlen==2 && gb_is_gbk((uint8_t*)data)) ||
					(dlen==4 && gb_is_gb18030_ext((uint8_t*)data)))
		{
			c->zi=1;
			c->ext=!gb_is_normal((uint8_t*)data);
			if(revert) c->ext=!c->ext;
			if(dic==Y_MB_DIC_MAIN)
				mb_add_zi(mb,code,clen,data,dlen,0);
		}
		else
		{
			if(dlen==1)
				c->zi=1;
			else if(data[0]=='$')
				c->ext=1;
		}
	}
	l_cslist_insert_after(NULL,pos,c);
	index=mb->last_index;
	index->ci_count++;
	if(c->zi)
	{
		index->zi_count++;
		if(c->ext)
			index->ext_count++;
	}

	return c;
}

static bool mb_ci_test_pos(void *p)
{
	struct y_mb_ci *c=p;
	return c->del==0;
}

static inline int mb_code_is_valid(struct y_mb *mb,const char *code,int clen)
{
	int c;
	int i;
	for(i=0;i<clen;i++)
	{
		c=code[i];
		if((c&0x80) || !mb->map[c])
			return 0;
	}
	return 1;
}

static inline struct y_mb_ci *mb_add_one_ci(
				struct y_mb *mb,
				struct y_mb_index *index,
				struct y_mb_item *it,
				const char *code,int clen,
				const char *data,int dlen,
				int pos,int dic,int revert)
{
	int a_head=0;
	struct y_mb_ci *a_node=NULL;
	struct y_mb_ci *p=L_CPTR(it->phrase);
	struct y_mb_ci *c=NULL;

	if(!p)
	{
		a_head=1;
	}
	else
	{
		if(pos==Y_MB_APPEND)
		{
			for(;p!=NULL;p=L_CPTR(p->next))
			{
				if(p->len==dlen && mb_ci_equal(p,data,dlen))
				{
					if(p->del)
					{
						p->del=0;
						if(p->zi)
							mb_add_zi(mb,code,clen,data,dlen,0);
					}
					if(dic!=Y_MB_DIC_ASSIST && dic!=Y_MB_DIC_TEMP)
					{
						p->dic=dic;
					}
					return p;
				}
				if(!p->next)
				{
					a_node=p;
					break;
				}
			}
		}
		else if(pos==0)
		{
			if(p->len==dlen && mb_ci_equal(p,data,dlen))
			{
				if(p->del)
				{
					p->del=0;
					if(p->zi)
						mb_add_zi(mb,code,clen,data,dlen,0);
				}
				if(dic==Y_MB_DIC_PIN)
					p->dic=Y_MB_DIC_PIN;
				else if(dic==Y_MB_DIC_USER && p->dic==Y_MB_DIC_TEMP)
					p->dic=Y_MB_DIC_USER;
				return p;
			}
			else
			{
				struct y_mb_ci *n=L_CPTR(p->next);
				a_head=1;
				for(;n!=NULL;p=n,n=L_CPTR(p->next))
				{
					if(n->len==dlen && mb_ci_equal(n,data,dlen))
					{
						p->next=n->next;
						c=n;c->dic=dic;
						break;
					}
				}
			}
		}
		else
		{
			struct y_mb_ci *n=L_CPTR(p->next);
			int i;
			int del=0;
			if(p->len==dlen && mb_ci_equal(p,data,dlen))
			{
				if(p->del)
				{
					p->del=0;
					if(p->zi)
						mb_add_zi(mb,code,clen,data,dlen,0);
				}
				p->dic=dic;
				if(!n) return p;
				c=p;p=n;
				it->phrase=L_CPTR_T(p);
				n=L_CPTR(p->next);
			}
			for(i=1;n!=NULL;p=n,n=L_CPTR(p->next))
			{
				if(i==pos)
				{
					a_node=p;
					if(del) break;
				}
				if(n->len==dlen && mb_ci_equal(n,data,dlen))
				{
					if(n->del)
						n->del=0;
					if(c) mb_ci_free(c);
					if(i==pos)
					{
						/* 词已经在正常位置，不需要用户词库调整 */
						if(dic==Y_MB_DIC_PIN)
							n->dic=Y_MB_DIC_PIN;
						return n;
					}
					p->next=n->next;
					c=n;c->dic=dic;
					if(a_node)
						break;
					del=1;
					n=p;
				}
				else
				{
					if(!n->del)
						i++;
				}
			}
			if(!a_node)
				a_node=p;
		}
	}
	if(a_head || a_node)
	{
		if(!c)
		{
			c=mb_ci_new(data,dlen);
			
			int ishz=revert;
			if(!ishz)
			{
				if(data[0]=='$' && data[1]=='[')
				{
					const char *real=y_mb_skip_display(data,dlen);
					int rlen=dlen-(int)(size_t)(real-data);
					ishz=(rlen==2 && gb_is_gbk((uint8_t*)real)) ||
						(rlen==4 && gb_is_gb18030_ext((uint8_t*)real));
					if(ishz)
						data=real;
				}
				else
				{
					 ishz=dlen==1 || (dlen==2 && gb_is_gbk((uint8_t*)data)) ||
						(dlen==4 && gb_is_gb18030_ext((uint8_t*)data));
				}
			}
			
			c->dic=dic;
			c->del=0;
			c->zi=ishz;
			c->ext=0;
			c->simp=0;
			if(ishz && dlen>1)
			{
				c->ext=!gb_is_normal((uint8_t*)data);
				if(revert) c->ext=!c->ext;
				if(dic==Y_MB_DIC_MAIN)
				{
					mb_add_zi(mb,code,clen,data,dlen,0);
				}
			}
			else
			{
				if(!ishz && data[0]=='$')
				{
					c->ext=1;
				}
			}
			index->ci_count++;
			if(c->zi)
			{
				index->zi_count++;
				if(c->ext)
					index->ext_count++;
			}
		}
		if(a_head)
		{
			c->next=L_CPTR_T(it->phrase);
			it->phrase=L_CPTR_T(c);
		}
		else
		{
			c->next=a_node->next;
			a_node->next=L_CPTR_T(c);
		}
		return c;
	}
	return 0;
}

uint32_t py_first_code(uint32_t hz,struct y_mb *mb)
{
	if(GB2312_IS_SYMBOL(hz))
		return ~0;
	struct y_mb_zi *z=L_HASH_TABLE_LOOKUP_INT(mb->zi,hz);
	if(!z)
		return 0;
	uint32_t code=0;
	for(struct y_mb_code *p=L_CPTR(z->code);p;p=L_CPTR(p->next))
	{
		int key=y_mb_code_n_key(mb,p,0);
		code|=1<<(key-'a');
	}
	return code;
}

static struct y_mb_ci *mb_add_one(struct y_mb *mb,const char *code,int clen,const char *data,int dlen,int pos,int dic)
{
	struct y_mb_ci *ci;
	struct y_mb_item *it=NULL;
	struct y_mb_index *index=NULL;
	uint16_t index_val;
	int revert=0;
	const char *orig_code=NULL;

	if(code[0]=='^') /* is it used to construct ci */
	{
		if(dic==Y_MB_DIC_ASSIST)
			return NULL;
		if((dlen==2 && gb_is_gbk((uint8_t*)data)) ||
					(dlen==4 && gb_is_gb18030_ext((uint8_t*)data)))
		{
			if(dic==Y_MB_DIC_MAIN)
			{
				int mv=1;
				code+=1;clen-=1;
				mb_add_zi(mb,code,clen,data,dlen,mv);
			}
			return NULL;
		}
		else
		{
			return NULL;
		}
	}
	else if(code[0]=='{') /* move the pos of ci */
	{
		int temp;
		temp=strcspn(code,"}");
		if(clen>=3 && code[temp]=='}')
		{
			temp++;
			if(code[1]=='-')
			{
				pos=Y_MB_DELETE;
				code+=temp;
				clen-=temp;
			}
			else if(code[1]>='0' && code[1]<='9')
			{
				pos=atoi(code+1);
				code+=temp;
				clen-=temp;
			}
			if(clen==0)
			{
				return NULL;
			}
		}
	}
	if(mb->pinyin==1 && mb->split=='\'')
	{
		char *temp=l_alloca(clen);
		int len=0;
		for(int i=0;i<clen;i++)
		{
			int c=code[i];
			if(c=='\'')
				continue;
			temp[len++]=c;
		}
		if(len!=clen)
		{
			temp[len]=0;
			orig_code=code;
			code=temp;
			clen=len;
		}
	}
	
	if(!mb_code_is_valid(mb,code,clen))
	{
		return NULL;
	}

	index_val=mb_ci_index(mb,code,clen,0);
	
	if(unlikely(data[0]=='~'))
	{
		if(dlen==3 && gb_is_gbk((uint8_t*)data+1))
			revert=1;
		else if(dlen==5 && gb_is_gb18030_ext((uint8_t*)data+1))
			revert=1;
		if(revert)
		{
			data++;
			dlen--;
		}
	}
	
	if(unlikely(dlen<=0))
		return NULL;
	if(pos==Y_MB_DELETE)
	{
		struct y_mb_ci *p;
		int found=0;
		mb->last_link=NULL;
		mb->last_index=NULL;
		if(!dlen) return NULL;
		if(mb->error)
		{
			char *temp=l_memdupa0(data,dlen);
			y_mb_error_add(mb,temp);
		}
		index=mb_get_index(mb,index_val);
		if(!index) return NULL;
		it=mb_get_one_code(mb,index,code,clen);
		if(!it) return NULL;
		for(p=L_CPTR(it->phrase);p;p=L_CPTR(p->next))
		{
			if(p->del) continue;
			if(!mb_ci_equal(p,data,dlen)) continue;
			if(p->zi)
			{
				index->zi_count--;
				if(p->ext)
					index->ext_count--;
				if(p->dic==Y_MB_DIC_MAIN)
					mb_add_zi(mb,code,clen,data,dlen,-1);
			}
			p->del=1;
			p->dic=dic;
			index->ci_count--;
			found=1;
			break;
		}
		if(found==0 && dic==Y_MB_DIC_USER)
			mb->dirty++;
		return NULL;
	}

	if(!index)
	{
		if(mb->last_index && mb->last_index->index==index_val)
		{
			index=mb->last_index;
		}
		else
		{
			index=mb_add_index(mb,index_val);
			if(!index)
				return NULL;
		}
	}
	if(mb->nsort)
	{
		it=mb_add_one_code_nsort(mb,index,code,clen,pos);
	}
	else
	{
		it=mb_add_one_code(mb,index,code,clen);
	}
	if(!it)
	{
		//printf("add code %s fail\n",code);
		return NULL;
	}
	
	ci=mb_add_one_ci(mb,index,it,code,clen,data,dlen,pos,dic,revert);
	if(mb->trie)
	{
		int ret;
		char temp[64];
		if(orig_code)
		{
			ret=py2_conv_to_sp3(orig_code,temp);
			if(!ci->zi)
				ci->ext=1;
		}
		else
		{
			ret=py2_conv_to_sp2(code,y_mb_skip_display(data,-1),temp,(void*)py_first_code,mb);
			if(ret==-1 && clen<=2)
			{
				ret=py2_conv_to_sp3(code,temp);
			}
		}
		if(ret>=0)
		{
			trie_node_t *n=trie_tree_add(mb->trie,temp,strlen(temp));
			n->data=it;
		}
		// else
		// {
			// memcpy(temp,code,clen);
			// temp[clen]=0;
			// printf("conv sp %s fail\n",temp);
		// }
	}
	return ci;
}

int y_mb_error_init(struct y_mb *mb)
{
	if(mb->error)
		return 0;
	mb->error=l_string_set_new(0);
	return 0;
}

static int y_mb_error_free(struct y_mb *mb)
{
	if(!mb->error)
		return 0;
	l_string_set_free(mb->error);
	mb->error=NULL;
	return 0;
}

bool y_mb_error_has(struct y_mb *mb,const char *phrase)
{
	if(!mb->error)
		return false;
	return l_string_set_has(mb->error,phrase);
}

int y_mb_error_add(struct y_mb *mb,const char *phrase)
{
	if(!mb->error)
		return 0;
	l_string_set_add(mb->error,phrase);
	return 0;
}

int y_mb_error_del(struct y_mb *mb,const char *phrase)
{
	if(!mb->error)
		return 0;
	l_string_set_del(mb->error,phrase);
	return 0;
}

static int mb_add_phrase_qp(struct y_mb *mb,const char *code,const char *phrase,int pos)
{
	struct y_mb_ci *c;
	char temp[Y_MB_KEY_SIZE+1];
	int i;
	for(i=0;*code!=0;code++)
	{
		if(*code==mb->split)
			continue;
		if(i==Y_MB_KEY_SIZE)
			return -1;
		temp[i++]=*code;
	}
	temp[i]=0;
	c=mb_add_one(mb,temp,strlen(temp),phrase,strlen(phrase),pos,Y_MB_DIC_USER);
	if(c)
	{
		mb->dirty++;
		if(mb->dirty>=mb->dirty_max)
			y_mb_save_user(mb);
	}
	return c?0:-1;
}

static int mb_escape_data(const char *in,char *out)
{
	int i,j;
	
	for(i=0,j=0;in[i];i++)
	{
		if(in[i]==' ')
		{
			out[j++]='$';
			out[j++]='_';
		}
		else
		{
			out[j++]=in[i];
		}
	}
	out[j]=0;
	return j;
}

static int mb_del_temp_phrase(struct y_mb *mb,const char *data);
int y_mb_add_phrase(struct y_mb *mb,const char *code,const char *phrase,int pos,int dic)
{
	struct y_mb_ci *c;
	int clen=strlen(code);
	int dlen;
	char temp[Y_MB_DATA_SIZE+1];
	if(clen<=0 || clen>Y_MB_KEY_SIZE)
		return -1;
	if(mb->user_words>1 && (dic==Y_MB_DIC_USER || dic==Y_MB_DIC_TEMP) &&
			l_gb_strlen(phrase,-1)>mb->user_words)
		return -1;
	dlen=mb_escape_data(phrase,temp);
	if(mb->split=='\'')
		return mb_add_phrase_qp(mb,code,temp,pos);
	if(dlen<=0 || dlen>=Y_MB_DATA_SIZE)
		return -1;
	mb_del_temp_phrase(mb,phrase);
	if(mb->nsort) pos=Y_MB_APPEND;
	c=mb_add_one(mb,code,clen,temp,dlen,pos,dic);
	if(c && dic==Y_MB_DIC_USER)
	{
		mb->dirty++;
		if(mb->dirty>=mb->dirty_max)
			y_mb_save_user(mb);
	}
	return c?0:-1;
}

struct y_mb_ci *y_mb_code_exist(struct y_mb *mb,const char *code,int len,int count)
{
	uint16_t index_val;
	struct y_mb_index *index;
	struct y_mb_item *p;
	struct y_mb_ci *c;
	uint8_t *key;

	index_val=mb_ci_index(mb,code,len,&key);
			
	count<<=1;
	index=mb_get_index(mb,index_val);
	if(index==NULL)
		return 0;
	p=mb_get_item(index,key);
	if(p==NULL)
		return 0;
	for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
	{
		if(c->data[0]<0x80)
			continue;
		if(count>0 && c->len!=count)
			continue;
		if(!c->del) return c;
	}

	return 0;
}

static struct y_mb_ci * mb_find_one(struct y_mb *mb,
	const char *code,const char *phrase,
	struct y_mb_index **pindex,struct y_mb_item **pitem)
{
	uint16_t index_val;
	struct y_mb_index *index;
	int ret;
	int len;
	struct y_mb_item *p;
	uint8_t *key;

	len=strlen(code);
	index_val=mb_ci_index(mb,code,len,&key);
	
	for(index=mb->index;index;index=L_CPTR(index->next))
	{
		ret=mb_index_cmp_direct(index_val,index->index,len);
		if(mb->nsort && ret<0) continue;
		if(ret<0) break;
		if(ret!=0) continue;
		if(index->ci_count==0)
			continue;
		for(p=L_CPTR(index->item);p;p=L_CPTR(p->next))
		{
			struct y_mb_ci *c;
			ret=mb_key_cmp_direct2(key,p->code,Y_MB_KEY_SIZE);
			if(ret>0) continue;
			if(ret<0) break;
			for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
			{
				if(c->del) continue;
				if(strcmp(phrase,y_mb_ci_string(c)))
				{
					continue;
				}
				*pindex=index;
				*pitem=p;
				return c;
			}
			return 0;
		}
	}
	return 0;
}

static int mb_move_phrase_qp(struct y_mb *mb,const char *code,const char *phrase,int dir)
{
	char temp[64];
	int i;
	for(i=0;*code!=0;code++)
	{
		if(*code==mb->split)
			continue;
		temp[i++]=*code;
	}
	temp[i]=0;
	return y_mb_move_phrase(mb,temp,phrase,dir);
}

static int mb_pin_phrase(struct y_mb *mb,const char *code);
int y_mb_move_phrase(struct y_mb *mb,const char *code,const char *phrase,int dir)
{
	struct y_mb_index *index;
	struct y_mb_item *item;
	struct y_mb_ci *c;
	int pos=0;
	
	if(mb->split=='\'' && strchr(code,mb->split))
		return mb_move_phrase_qp(mb,code,phrase,dir);
	
	c=mb_find_one(mb,code,phrase,&index,&item);
	if(!c) return -1;
	if(dir!=1 && dir!=-1 && dir!=0)
		return -1;
	if(dir==1 && !c->next)
		return 0;
	if(dir==-1 && L_CPTR(item->phrase)==c)
		return 0;
	if(dir==0 && L_CPTR(item->phrase)==c)
		return 0;
	if(dir!=0)
	{
		pos=mb_slist_pos_custom(L_CPTR(item->phrase),c,mb_ci_test_pos);
		if(pos<0 || pos>=Y_MB_APPEND)
			return -1;
		pos+=dir;
		if(pos<0 || pos>=Y_MB_APPEND)
			return -1;
	}
	else
	{
		dir=0;
	}
	c->dic=Y_MB_DIC_USER;
	item->phrase=L_CPTR_T(l_cslist_remove(L_CPTR(item->phrase),c));
	item->phrase=L_CPTR_T(mb_slist_insert_custom(L_CPTR(item->phrase),c,pos,mb_ci_test_pos));
	mb->dirty++;
	
	mb_pin_phrase(mb,code);

	if(mb->dirty>=mb->dirty_max)
		y_mb_save_user(mb);
	return 0;
}

static int mb_auto_move_qp(struct y_mb *mb,const char *code,const char *phrase,int auto_move)
{
	char temp[64];
	int i;
	for(i=0;*code!=0;code++)
	{
		if(*code==mb->split)
			continue;
		temp[i++]=*code;
	}
	temp[i]=0;
	return y_mb_auto_move(mb,temp,phrase,auto_move);
}

int y_mb_auto_move(struct y_mb *mb,const char *code,const char *phrase,int auto_move)
{
	struct y_mb_index *index;
	struct y_mb_item *item;
	struct y_mb_ci *c;
	int pos=0;
	
	if(!auto_move)
		return 0;
	if(mb->nomove[0])
	{
		int l=strlen(mb->nomove);
		if(!strncmp(code,mb->nomove,l))
			return 0;
	}
	
	if(mb->split=='\'' && strchr(code,mb->split))
		return mb_auto_move_qp(mb,code,phrase,auto_move);
	
	c=mb_find_one(mb,code,phrase,&index,&item);
	if(!c) return -1;
	if(L_CPTR(item->phrase)==c)
		return 0;
	else if(auto_move==2)
		pos=mb_slist_pos_custom(L_CPTR(item->phrase),c,mb_ci_test_pos)/2;
	if(c->dic!=Y_MB_DIC_TEMP)
		c->dic=Y_MB_DIC_USER;
	item->phrase=L_CPTR_T(l_cslist_remove(L_CPTR(item->phrase),c));
	item->phrase=L_CPTR_T(mb_slist_insert_custom(L_CPTR(item->phrase),c,pos,mb_ci_test_pos));
	mb->dirty++;
	
	mb_pin_phrase(mb,code);

	if(mb->dirty>=mb->dirty_max)
		y_mb_save_user(mb);
	return 0;
}

int y_mb_del_phrase(struct y_mb *mb,const char * code,const char *phrase)
{
	struct y_mb_ci *c;
	struct y_mb_index *index;
	struct y_mb_item *item;
	
	c=mb_find_one(mb,code,phrase,&index,&item);
	if(!c) return -1;
	if(c->zi)
	{
		index->zi_count--;
		if(c->ext)
			index->ext_count--;
		if(c->dic==Y_MB_DIC_MAIN)
			mb_add_zi(mb,code,strlen(code),phrase,strlen(phrase),-1);
	}
	// 用户移动过的主词库词可能删不掉，标记为del即可
	c->del=1;
	index->ci_count--;
	mb->dirty++;
	
	mb_pin_phrase(mb,code);

	if(mb->dirty>=mb->dirty_max)
		y_mb_save_user(mb);
	return 0;
}

FILE *y_mb_open_file(const char *fn,const char *mode)
{
	char temp[256];

	l_gb_to_utf8(fn,temp,sizeof(temp));fn=temp;

	if(EIM.OpenFile)
		return EIM.OpenFile(fn,mode);
	else
		return fopen(fn,mode);
}

int mb_load_english(struct y_mb *mb,FILE *fp)
{
	char line[Y_MB_KEY_SIZE+1];
	int len;
	
	for(;(len=l_get_line(line,sizeof(line),fp))>=0;)
	{
		if(len==0 || len>Y_MB_KEY_SIZE || line[0]=='#')
			continue;
		if(isupper(line[0])) //only detect the first
		{
			char key[Y_MB_KEY_SIZE+1];
			int i;
			for(i=0;i<len;i++)
				key[i]=tolower(line[i]);
			key[i]=0;
			mb_add_one(mb,key,len,line,len,Y_MB_APPEND,0);
		}
		else
		{
			mb_add_one(mb,line,len,line,len,Y_MB_APPEND,0);
		}
	}
	return 0;
}

static inline int mb_zi_has_simple(struct y_mb_zi *z,int clen)
{
	if(z) for(struct y_mb_code *p=L_CPTR(z->code);p;p=L_CPTR(p->next))
	{
		if(p->virt)
			continue;
		if(p->len<clen)
			return 1;
	}
	return 0;
}

static int mb_has_simple(struct y_mb *mb,int clen,const char *data,int dlen)
{
	struct y_mb_zi *z=mb_find_zi(mb,data);
	return mb_zi_has_simple(z,clen);
}

static void mb_mark_simple(struct y_mb *mb)
{
	struct y_mb_index *index;
	struct y_mb_zi *z;
	
	/* 只考虑简码处理和全码不重复显示的情况 */
	if(!mb->simple && !mb->compact)
		return;
	/* 只考虑单字的情况 */
	if(!mb->zi)
		return;

	if(mb->simple) for(index=mb->index;index;index=L_CPTR(index->next))
	{
		struct y_mb_item *it;
		for(it=L_CPTR(index->item);it;it=L_CPTR(it->next))
		{
			char *cur=0;
			int len=0;		/* 不用初始化，但是gcc有一个错误的未初始化警告 */
			struct y_mb_ci *c,*first=NULL,*prev=NULL;
			for(c=L_CPTR(it->phrase);c;prev=c,c=L_CPTR(c->next))
			{
				if(c->del)				/* 已被删除 */
					continue;
				if(!first)				/* 得到第一个词组 */
					first=c;
				if(!c->zi)				/* 跳过不是字的 */
					continue;
				if(!cur)
				{
					/* 获得当前系列词组的编码 */
					cur=mb_key_conv2_r(mb,index->index,it->code);
					len=strlen(cur);
					if(len==1) break; /* 不考虑一简 */
				}
				/* 查找字对应的信息 */
				z=mb_find_zi(mb,(const char*)&c->data);
				if(!z)		/* 找不到相关信息 */
					continue;
				if(mb->simple==2 &&	c==L_CPTR(it->phrase) && !c->next)
				{
					/* 无重码，对“重码时隐藏简码”功能来说不需要处理 */
					continue;
				}
				else if(mb->simple==1 || mb->simple==2)
				{
					/* 对“重码时隐藏简码”和“出简不出全“功能，标记简码 */
					if(c->dic!=Y_MB_DIC_ASSIST && mb_zi_has_simple(z,len))
					{
						c->simp=1;
						index->ci_count--;
						index->zi_count--;
						if(c->ext) index->ext_count--;
						continue;
					}
				}
				else if(mb->simple==3)
				{
					if(c==first && c->next && mb_zi_has_simple(z,len))
					{
						do{
							if(!c->zi || !mb_has_simple(mb,len,(char*)&c->data,c->len))
								break;
							struct y_mb_ci *h=L_CPTR(c->next);
							if(prev)
								prev->next=L_CPTR_T(h);
							else
								it->phrase=L_CPTR_T(h);
							l_cslist_append(h,c);
							c=h;
						}while(c!=first && c->next);
					}
					break;
				}
			}
		}
	}
	if(mb->compact)  for(index=mb->index;index;index=L_CPTR(index->next))
	{
		struct y_mb_item *it;
		for(it=L_CPTR(index->item);it;it=L_CPTR(it->next))
		{
			char *cur=0;
			int len=0;
			struct y_mb_ci *c;
			for(c=L_CPTR(it->phrase);c;c=L_CPTR(c->next))
			{
				struct y_mb_code *p;
				if(c->del)				/* 已被删除 */
					continue;
				if(!c->zi)				/* 跳过不是字的 */
					continue;
				if(!cur)
				{
					/* 获得当前系列词组的编码 */
					cur=mb_key_conv2_r(mb,index->index,it->code);
					len=strlen(cur);
					if(len==1) break; /* 不考虑一简 */
				}
				/* 查找字对应的信息 */
				z=mb_find_zi(mb,(const char*)&c->data);
				if(!z)		/* 找不到相关信息 */
					continue;
				for(p=L_CPTR(z->code);p;p=L_CPTR(p->next))
				{
					int i;
					if(p->virt) continue;
					if(p->len>=len) continue;
					for(i=0;i<p->len;i++)
					{
						/* 在发现编码不匹配时跳出循环 */
						if(cur[i]!=y_mb_code_n_key(mb,p,i))
							break;
					}
					if(i==p->len)
					{
						/* 有更长的编码，表明这是一个简码 */
						c->simp=1;
						break;
					}
				}
			}
		}
	}
}

static void mb_half_index(struct y_mb *mb)
{
	struct y_mb_index *p;
	int len;
	if((mb->flag&MB_FLAG_ASSIST))
		return;
	len=l_cslist_length(mb->index);
	if(len<7) return;
	mb->half=l_cslist_nth(mb->index,len>>1);
	if(mb->nsort)
		return;
	for(p=mb->index;p!=NULL;p=L_CPTR(p->next))
	{
		len=l_cslist_length(L_CPTR(p->item));
		if(len<7) continue;
		p->half=L_CPTR_T(l_cslist_nth(L_CPTR(p->item),len>>1));
		
		struct y_mb_item *item,*half;
		for(item=L_CPTR(p->item);item!=(half=L_CPTR(p->half));item=L_CPTR(item->next))
		{
			int ret=mb_key_cmp_direct2(item->code,half->code,Y_MB_KEY_SIZE);
			assert(ret<0);
		}		
	}
}

static int mb_load_zi(struct y_mb *mb,FILE *fp)
{
	while(!mb->cancel)
	{
		char line[4096];
		int len,clen,dlen;
		const char *data;

			if(mb->encode==0)
			len=l_get_line(line,sizeof(line),fp);
		else
			len=get_line_iconv(line,sizeof(line),fp);

		if(unlikely(len<0))
			break;
		else if(unlikely((len==0 || line[0]=='#') && !mb->jing_used))
			continue;
		if(unlikely(line[0] & 0x80))
			continue;

		for(clen=0;clen<=Y_MB_KEY_SIZE && line[clen] && line[clen]!=' ';clen++);
		if(line[clen]!=' ') continue;
		if(clen==0) continue;

		const char *code=line;
		int mv=0x0;
		if(code[0]=='^')
		{
			mv=0x02;
			code++;
			clen--;
		}
		if(code[0]=='{')
		{
			int temp;
			temp=strcspn(code,"}");
			if(clen>=3 && code[temp]=='}')
			{
				code+=temp+1;
				clen-=temp+1;
			}
		}

		data=line+clen;
		do{
			while(*data==' ') data++;// skip the space char
			for(dlen=0;dlen<Y_MB_DATA_SIZE && data[dlen] && data[dlen]!=' ';dlen++);
			if(dlen<=0) break;
			if(data[0]=='$' && data[1]=='[')
			{
				const char *real=y_mb_skip_display(data,dlen);
				dlen-=(int)(size_t)(real-data);
				data=real;
			}
			if((dlen==2 && gb_is_gbk((const uint8_t*)data)) || (dlen==4 && gb_is_gb18030_ext((const uint8_t*)data)))
			{
				mb_add_zi(mb,code,clen,data,dlen,mv);
			}
			else
			{
				continue;
			}
			data+=dlen;
		}while(data[0]==' ');
	}
	return 0;
}

int mb_load_data(struct y_mb *mb,FILE *fp,int dic)
{
	char line[4096];
	int len,clen,dlen;
	char *data;
	struct y_mb_ci *c;
	int in_data=(dic==Y_MB_DIC_MAIN || dic==Y_MB_DIC_USER || dic==Y_MB_DIC_PIN);
	int has_space=y_mb_is_key(mb,' ');

	while(!mb->cancel)
	{

			if(mb->encode==0)
			len=l_get_line(line,sizeof(line),fp);
		else
			len=get_line_iconv(line,sizeof(line),fp);

		if(unlikely(len<0))
			break;
		else if(unlikely((len==0 || line[0]=='#') && !mb->jing_used))
		{
			if(!strcmp(line,"# exit"))
				break;
			continue;
		}
		
		if(unlikely(line[0] & 0x80))
		{
			/* no code phrase */			
			int ret;
			ret=gb_strbrk((uint8_t*)line);
			if(ret>Y_MB_DATA_SIZE)
				continue;
			if(ret!=len)
			{
				mb_add_one(mb,line+ret,len-ret,line,ret,Y_MB_APPEND,dic);
			}
			else
			{
				char code[3][Y_MB_KEY_SIZE+1];
				char *codes[]={code[0],code[1],code[2],NULL};
				code[0][0]=code[1][0]=code[2][0]=0;
				ret=y_mb_code_by_rule(mb,line,len,codes,NULL);
				if(ret==0)
				{
					int i;
					for(i=0;i<3;i++)
					{
						if(!code[i][0])
							break;
						mb_add_one(mb,code[i],strlen(code[i]),line,len,Y_MB_APPEND,dic);
						//printf("%s %s\n",code[i],line);
					}					
				}
			}
			continue;
		}
		if(unlikely(!in_data && line[0]=='[' && !strcasecmp(line,"[DATA]")))
		{
			in_data=1;
			continue;
		}
		for(clen=0;clen<=Y_MB_KEY_SIZE && line[clen] && line[clen]!=' ' && (in_data || line[clen]!='=');clen++);
		if(line[clen]!=' ') continue;
		if(clen==0) continue;
		in_data=1;
		data=line+clen;
		c=NULL;
		do{
			while(*data==' ') data++;// skip the space char
			for(dlen=0;dlen<Y_MB_DATA_SIZE && data[dlen] && data[dlen]!=' ';dlen++);
			if(dlen<=0) break;
			if(!c || (mb->flag&MB_FLAG_SLOW))
			{
				if(unlikely(has_space))
				{
					for(int j=0;j<clen;j++)
					{
						if(line[j]=='_')
							line[j]=' ';
					}
				}
				c=mb_add_one(mb,line,clen,data,dlen,Y_MB_APPEND,dic);
				if(unlikely(!c))
					break;
				// move to end, let next phrase append at end
				while(c->next) c=L_CPTR(c->next);
			}
			else
			{
				/* used to fast the add */
				c=mb_app_one(mb,c,line,clen,data,dlen,dic);
			}
			data+=dlen;
		}while(data[0]==' ');
	}
	return 0;
}

static int mb_load_user(struct y_mb *mb,const char *fn)
{
	FILE *fp=y_mb_open_file(fn,"rb");
	if(!fp) return 0;
	mb_load_data(mb,fp,Y_MB_DIC_USER);
	fclose(fp);
	return 0;
}

int mb_load_assist_code(struct y_mb *mb,FILE *fp,int pos)
{
	char line[4096];
	int len,clen,dlen;
	char *data;
	while(1)
	{
		len=l_get_line(line,sizeof(line),fp);
		if(len<0) break;
		if(len==0 || line[0]=='#') continue;
		for(clen=0;clen<=Y_MB_KEY_SIZE && line[clen] && line[clen]!=' ' && line[clen]!='=';clen++);
		if(line[clen]!=' ') continue;
		data=line+clen;
		if(clen<=pos) continue;
		do{
			data++;// skip the space char
			for(dlen=0;dlen<=Y_MB_DATA_SIZE && data[dlen] && data[dlen]!=' ';dlen++);
			if(dlen<=0) break;
			if(clen>=pos && ((dlen==2 && gb_is_gbk((uint8_t*)data)) || 
					(dlen==4 && gb_is_gb18030_ext((uint8_t*)data))))
			{
				mb_add_zi(mb,line+pos,clen-pos,data,dlen,0);
			}
			data+=dlen;
		}while(data[0]==' ');
	}
	return 0;
}

static int scel_read_u16_array(FILE *fp,void *out)
{
	uint16_t len;
	int ret=fread(&len,1,2,fp);
	if(ret!=2)
		return -1;
	if((ret&0x01)!=0)
		return -2;
	ret=fread(out,1,len,fp);
	if(ret!=len)
		return -3;
	((uint16_t*)out)[len/2]=0;
	return len;
}

static int mb_load_scel(struct y_mb *mb,FILE *fp)
{
	char temp[0x1544];
	ssize_t ret;
	int res=-1;
	int i;
	char py_table[449][8];
	int py_count=0;
	const uint8_t magic[][12]={
		{0x40, 0x15, 0x00, 0x00, 0x44, 0x43, 0x53, 0x01, 0x01, 0x00, 0x00, 0x00},
		{0x40, 0x15, 0x00, 0x00, 0x45, 0x43, 0x53, 0x01, 0x01, 0x00, 0x00, 0x00},
		{0x40, 0x15, 0x00, 0x00, 0xd2, 0x6d, 0x53, 0x01, 0x01, 0x00, 0x00, 0x00},
	};
	if(!fp) return -1;
	ret=fread(temp,1,sizeof(temp),fp);
	if(ret!=sizeof(temp))
	{
		return -1;
	}
	for(i=0;i<countof(magic);i++)
	{
		if(!memcmp(magic[i],temp,12))
			break;
	}
	if(i==countof(magic))
		return -2;
	uint32_t phrase_count=l_read_u32(temp+0x5c);
	uint32_t phrase_offset=l_read_u32(temp+0x60);
	uint32_t num_entries=l_read_u32(temp+0x120);
	// printf("%u %u %u\n",phrase_count,phrase_offset,num_entries);
	py_count=l_read_u32(temp+0x1540);
	if(py_count>countof(py_table))
		goto out;
	for(i=0;i<py_count;i++)
	{
		uint16_t index;
		uint16_t len;
		uint16_t data[8];
		if(1!=fread(&index,2,1,fp)) goto out;
		len=scel_read_u16_array(fp,data);
		if(len<=0)
			goto out;
		if(index!=i)
			continue;
		l_utf16_to_utf8(data,py_table[i],8);
		// printf("%d %s\n",index,py_table[i]);
	}

	while(num_entries>0)
	{
		uint16_t same;
		uint16_t py_len,dlen;
		uint16_t data[64];
		bool bad=false;
		if(1!=fread(&same,2,1,fp)) break;
		if(same<=0) goto out;
		if(1!=fread(&py_len,2,1,fp)) goto out;
		if(py_len<=0 || py_len>=64 || (py_len&0x01)!=0)
			goto out;
		num_entries--;
		py_len/=2;
		if(py_len!=fread(data,2,py_len,fp)) goto out;
		if(py_count>0)
		{
			temp[0]=0;
			for(i=0;i<py_len;i++)
			{
				if(data[i]>=py_count)
				{
					goto out;
				}
				const char *code=py_table[data[i]];
				if(isupper(code[0]))
					bad=true;
				strcat((char*)temp,code);
			}
		}
		for(int i=0;i<same;i++)
		{
			dlen=scel_read_u16_array(fp,data);
			if(dlen<=0)
				goto out;
			l_utf16_to_gb(data,temp+512,512);
			if(!bad)
			{
				if(py_count==0 || !mb->pinyin || mb->split!='\'')
				{
					char *outs[]={temp,NULL};
					ret=y_mb_code_by_rule(mb,temp+512,strlen(temp+512),outs,NULL);
					if(ret!=0) temp[0]=0;
				}
				mb_add_one(mb,temp,strlen(temp),temp+512,strlen(temp+512),Y_MB_APPEND,Y_MB_DIC_SUB);
			}
			dlen=scel_read_u16_array(fp,temp+512);
			if(dlen<=0) goto out;
		}
	}
	if(0!=fseek(fp,phrase_offset,SEEK_SET))
		goto out;
	while(phrase_count>0)
	{
		uint16_t data[64];
		phrase_count--;
		if(1!=fread(temp,17,1,fp)) goto out;
		if(temp[2]==0x01)
		{
			int len=scel_read_u16_array(fp,data);
			if(len<=0) goto out;
			len/=2;
			temp[0]=0;
			for(i=0;i<len;i++)
			{
				if(data[i]>=py_count)
				{
					goto out;
				}
				const char *code=py_table[data[i]];
				strcat((char*)temp,code);
			}
			len=scel_read_u16_array(fp,data);
			if(len<=0) goto out;
			l_utf16_to_gb(data,temp+512,512);
		}
		else
		{
			int py_len,dlen;
			py_len=scel_read_u16_array(fp,data);
			if(py_len<=0) goto out;
			l_utf16_to_gb(data,temp,512);
			dlen=scel_read_u16_array(fp,temp+512);
			if(dlen<=0) goto out;
			l_utf16_to_gb(data,temp+512,512);
		}
		if(!mb->pinyin || mb->split!='\'')
		{
			char *outs[]={temp,NULL};
			ret=y_mb_code_by_rule(mb,temp+512,strlen(temp+512),outs,NULL);
			if(ret!=0) temp[0]=0;
		}
		bool bad=false;
		for(int j=0;temp[j];j++)
		{
			if(isupper(temp[j]))
			{
				bad=true;
				break;
			}
		}
		if(bad)
			continue;
		for(int j=512;temp[j];j++)
		{
			if((temp[j]&0x80)==0)
			{
				bad=true;
				break;
			}
		}
		if(bad)
			continue;
		mb_add_one(mb,temp,strlen(temp),temp+512,strlen(temp+512),Y_MB_APPEND,Y_MB_DIC_SUB);
	}
	res=0;
out:
	return res;

}

static int mb_load_sub_dict(struct y_mb *mb,const char *dict);
int y_mb_load_quick(struct y_mb *mb,const char *quick)
{
	if(!quick || !quick[0])
		return -1;
	char quick_lead;
	int quick_lead0=0;
	char file[256],*files[10];
	int ret=l_sscanf(quick,"%c %s %d",&quick_lead,file,&quick_lead0);
	if(ret<2)
		return -1;
	l_strtok0(file,',',files,lengthof(files));
	mb->quick_mb=y_mb_new();
	mb->quick_mb->hint=1;
	strcpy(mb->quick_mb->key+1,"abcdefghijklmnopqrstuvwxyz");
	mb->quick_mb->len=Y_MB_KEY_SIZE;
	y_mb_key_map_init(mb->quick_mb->key,0,mb->quick_mb->map);
	mb->quick_mb->key_compress=1;
	for(int i=0;i<countof(files);i++)
	{
		if(!files[i])
			break;
		mb_load_sub_dict(mb->quick_mb,files[i]);
	}
	mb->quick_lead=quick_lead;
	mb->quick_lead0=quick_lead0;
	return 0;
}

static void pin_free(struct y_mb_pin_item *item)
{
	if(!item)
		return;
	l_slist_free(item->list,l_free);
	l_free(item);
}

int y_mb_load_pin(struct y_mb *mb,const char *pin)
{
	if(!pin)
		return -1;
	
	FILE *fp=y_mb_open_file(pin,"rb");
	if(!fp)
		return -1;
	//assert(mb->pin==NULL);
	if(mb->pin)
	{
		l_hash_table_free(mb->pin,(LFreeFunc)pin_free);
		mb->pin=NULL;
	}
	mb->pin=L_HASH_TABLE_STRING(struct y_mb_pin_item,data,0);	
	mb_load_data(mb,fp,Y_MB_DIC_PIN);
	fclose(fp);
	for(struct y_mb_index *index=mb->index;index;index=L_CPTR(index->next))
	{
		struct y_mb_item *it=L_CPTR(index->item);
		while(it)
		{
			char *code;
			int clen;
			struct y_mb_ci *cp;
			int pos=0;
			struct y_mb_pin_item *item=NULL;
			
			code=mb_key_conv2_r(mb,index->index,it->code);
			clen=strlen(code);
			
			cp=L_CPTR(it->phrase);
			while(cp)
			{
				if(cp->dic==Y_MB_DIC_PIN && pos<128)
				{
					struct y_mb_pin_ci *pc;
					if(!item)
					{
						item=l_hash_table_lookup(mb->pin,code);
						if(!item)
						{			
							item=l_alloc(sizeof(struct y_mb_pin_item)+clen+1);
							strcpy(item->data,code);
							item->len=(uint8_t)clen;
							item->next=NULL;
							item->list=NULL;
							l_hash_table_insert(mb->pin,item);
						}
					}
			
					pc=l_alloc(sizeof(struct y_mb_pin_ci)+cp->len);
					pc->next=NULL;
					pc->len=cp->len;
					pc->pos=(int8_t)pos;
					memcpy(pc->data,cp->data,cp->len);
					//printf("{%d}%s %s\n",pos,code,y_mb_ci_string(cp));
					if(item->list==NULL)
					{
						item->list=pc;
					}
					else if(item->list->pos>=pc->pos)
					{
						pc->next=item->list;
						item->list=pc;
					}
					else
					{
						struct y_mb_pin_ci *pp;
						for(pp=item->list;pp!=NULL;pp=pp->next)
						{
							if(pp->next==NULL || pp->next->pos>=pc->pos)
							{
								pc->next=pp->next;
								pp->next=pc;
								break;
							}
						}
					}
				}
				if(!cp->del)
					pos++;
				cp=L_CPTR(cp->next);
			}
			it=L_CPTR(it->next);
		}
	}
	return 0;
}

static int mb_pin_phrase(struct y_mb *mb,const char *code)
{
	struct y_mb_pin_item *item;
	struct y_mb_pin_ci *c;
	if(!mb->pin)
	{
		return 0;
	}
	item=l_hash_table_lookup(mb->pin,code);
	if(!item)
	{
		return 0;
	}
	for(c=item->list;c!=NULL;c=c->next)
	{
		mb_add_one(mb,code,item->len,c->data,c->len,c->pos,Y_MB_DIC_PIN);
	}
	return 0;
}

static int mb_load_sub_dict(struct y_mb *mb,const char *dict)
{
	FILE *fp=NULL;
	if(strchr(dict,'.')!=NULL)
		fp=y_mb_open_file(dict,"rb");
	if(fp)
	{
		if(l_str_has_suffix(dict,".scel"))
			mb_load_scel(mb,fp);
		else
			mb_load_data(mb,fp,Y_MB_DIC_SUB);
		fclose(fp);
	}
	else
	{
		char base[128];
		LDir *d;
		const char *f;
		int len;
		if(EIM.GetPath==NULL)
			return -1;
		len=sprintf(base,"%s/%s",EIM.GetPath("HOME"),dict);
		if(!l_file_is_dir(base))
		{
			len=sprintf(base,"%s/%s",EIM.GetPath("DATA"),dict);
			if(!l_file_is_dir(base))
			{
				return -1;
			}
		}
		d=l_dir_open(base);
		if(!d)
		{
			//printf("l_dir_open %s fail\n",base);
			return -1;
		}
		while((f=l_dir_read_name(d))!=NULL)
		{
			if(f[0]=='.') continue;
			snprintf(base+len,sizeof(base)-len,"/%s",f);
			fp=l_file_open(base,"rb",NULL);
			if(fp)
			{
				if(l_str_has_suffix(f,".scel"))
					mb_load_scel(mb,fp);
				else
					mb_load_data(mb,fp,Y_MB_DIC_SUB);
				fclose(fp);
			}
		}
		l_dir_close(d);
		
	}
	return 0;
}

struct y_mb *y_mb_new(void)
{
	struct y_mb *mb=l_new0(struct y_mb);
	mb->suffix=-1;
	return mb;
}

static void mb_load_assist_config(struct y_mb *mb,int flag,const char *line)
{
	if(!line || !line[0])
		return;
	if(isgraph(line[0]) && line[1]==' ')
	{
		int ass_lead0;
		char file[256];
		l_sscanf(line,"%c %s %d",&mb->ass_lead,file,&ass_lead0);
		mb->ass_main=strdup(file);
		mb->ass_lead0=ass_lead0;
	}
	else
	{
		char ass[256],*p;
		int pos=-1;
		strcpy(ass,line);
		p=strchr(ass,' ');
		if(p)
		{
			*p++=0;
			if(isdigit(*p))
				pos=atoi(p);
		}
		if(pos>=0 && pos<16)
		{
			struct y_mb_arg mb_arg;
			memset(&mb_arg,0,sizeof(struct y_mb_arg));
			mb_arg.apos=pos;
			mb->ass_main=strdup(line);
			if(!(flag&MB_FLAG_ASSIST))
			{
				mb->ass_mb=y_mb_load(ass,MB_FLAG_ASSIST_CODE,&mb_arg);
			}
		}
	}
}

static LHashTable *create_zi_first_table(struct y_mb *mb)
{
	LHashTable *h=L_HASH_TABLE_STRING(struct y_mb_zi_first,code,509);
	for(struct y_mb_index *index=mb->index;index;index=L_CPTR(index->next))
	{
		for(struct y_mb_item *p=L_CPTR(index->item);p;p=L_CPTR(p->next))
		{
			for(struct y_mb_ci *c=L_CPTR(p->phrase);c!=NULL;c=L_CPTR(c->next))
			{
				if(c->del)
					continue;
				if(!c->zi)
					break;
				char *code=mb_key_conv2_r(mb,index->index,p->code);
				int len=strlen(code);
				if(len>7)
					break;
				struct y_mb_zi_first *z=l_new(struct y_mb_zi_first);
				strcpy(z->code,code);
				z->ci=c;
				bool ret=l_hash_table_insert(h,z);
				if(!ret)
					l_free(z);
				break;
			}
		}
	}
	return h;
}

int y_mb_load_to(struct y_mb *mb,const char *fn,int flag,struct y_mb_arg *arg)
{
	char line[4096];
	int len,lines;

	FILE *fp=y_mb_open_file(fn,"rb");
	if(!fp)
	{
		if(!(flag &MB_FLAG_ASSIST))
			printf("yong: open mb %s fail\n",fn);
		return -1;
	}
	
	mb->flag=flag;
	mb->main=l_strdup(fn);
	mb->dirty_max=2;

	/* set some default config here */
	mb->hint=1;
	mb->len=Y_MB_KEY_SIZE;
	strcpy(mb->key+1,"abcdefghijklmnopqrstuvwxyz");
	y_mb_key_map_init(mb->key,arg?arg->wildcard:0,mb->map);
	mb->key_compress=1;
	mb->code_hint=0;
	for(lines=0;(len=l_get_line(line,sizeof(line),fp))>=0;lines++)
	{
		if(line[0]=='#')
		{
			continue;
		}
		if(len<=4 || len>200)
		{
			if(len!=0)
				printf("yong: bad line %s:%d\n",fn,lines);
			continue;
		}
		if(lines==0 && !strncmp(line,"\xef\xbb\xbf",3))
		{
			memmove(line,line+3,strlen(line+3)+1);
			mb->encode=1;
			if(line[0]=='#')
				continue;
		}
		if(!strcmp(line,"encode=UTF-8"))
		{
			mb->encode=1;
		}

		else if(!strncmp(line,"name=",5))
		{
			if(len>=20)
			{
				printf("yong: bad name\n");
			}
			else
			{
				if(mb->encode==1)
				{
					l_utf8_to_gb(line+5,mb->name,sizeof(mb->name));
				}
				else
				{
					strcpy(mb->name,line+5);
				}
			}
		}
		else if(!strncmp(line,"key=",4))
		{
			int i;
			for(i=4;line[i]!=0;i++)
			{
				if(line[i]>='A' && line[i]<='Z')
				{
					mb->capital=1;
					break;
				}
			}
			mb->jing_used=strchr(line+4,'#')?1:0;
			//strncpy(mb->key+1,line+4,60);
			//mb->key[61]=0;
			l_strcpy(mb->key+1,62,line+4);
			memset(mb->map,0,sizeof(mb->map));
			y_mb_key_map_init(mb->key,0,mb->map);
			mb->key_compress=strlen(line+4)<32?1:2;
		}
		else if(!strncmp(line,"key0=",5))
		{
			//snprintf(mb->key0,sizeof(mb->key0),"%s",line+5);
			l_strcpy(mb->key0,sizeof(mb->key0),line+5);
		}
		else if(!strncmp(line,"len=",4))
		{
			mb->len=atoi(line+4);
		}
		else if(!strncmp(line,"push=",5))
		{
			char stop[8]={0};
			int i,c;
			char **list;
			list=l_strsplit(line+5,' ');
			if(list[0])
			{
				l_strcpy(mb->push,sizeof(mb->push),list[0]);
				for(i=0;mb->push[i]!=0;i++)
				{
					if(mb->push[i]=='_')
						mb->push[i]=' ';
				}
				if(list[1]) strcpy(stop,list[1]);
			}
			l_strfreev(list);
			for(i=0;i<8 && (c=stop[i]);i++)
			{
				if(c<'1' || c>'7')
				{
					printf("yong: invalid stop setting\n");
					break;
				}
				mb->stop|=1<<(c-'0');
			}
			
		}
		else if(!strncmp(line,"pull=",5))
		{
			strcpy(mb->pull,line+5);
		}
		else if(!strncmp(line,"suffix=",7))
		{
			mb->suffix=EIM.GetKey(line+7);
		}
		else if(!strncmp(line,"match=",6))
		{
			if(line[6]=='1')
				mb->match=1;
		}
		else if(!strncmp(line,"wildcard=",9))
		{
			mb->wildcard_orig=line[9];
			mb->wildcard=((arg && arg->wildcard)?arg->wildcard:line[9]);
			y_mb_key_map_init(0,mb->wildcard,mb->map);
			mb->key[Y_MB_WILDCARD]=mb->wildcard;
		}
		else if(!strncmp(line,"dwf=",4))
		{
			mb->dwf=(line[4]=='1');
		}
		else if(!strcmp(line,"english=1"))
		{
			mb->english=1;
		}
		else if(!strncmp(line,"simple=",7))
		{
			mb->simple=atoi(line+7)&0x03;
		}
		else if(!strncmp(line,"compat=",7))
		{
			mb->compact=atoi(line+7)&0x03;
		}
		else if(!strncmp(line,"compact=",8))
		{
			mb->compact=atoi(line+8)&0x03;
		}
		else if(!strncmp(line,"yong=",5))
		{
			mb->yong=atoi(line+5);
		}
		else if(!strncmp(line,"pinyin=",7))
		{
			mb->pinyin=atoi(line+7);
			mb->dirty_max=20;
			if(mb->split==0)
				mb->split=1;
		}
		else if(!strcmp(line,"hint=0"))
		{
			mb->hint=0;
		}
		else if(!strncmp(line,"auto_clear=",11))
		{
			mb->auto_clear=atoi(line+11);
		}
		else if(!strncmp(line,"nomove=",7))
		{
			if(strlen(line+7)<4)
				strcpy(mb->nomove,line+7);
		}
		else if(!strncmp(line,"auto_move=",10))
		{
			mb->auto_move=atoi(line+10);
		}
		else if(!strcmp(line,"nsort=1"))
		{
			mb->nsort=1;
		}
		else if(!strncmp(line,"sloop=",6))
		{
			mb->sloop=atoi(line+6);
		}
		else if(!strncmp(line,"split=",6))
		{
			if(line[6]<='9' && line[6]>='1')
				mb->split=line[6]-'0';
			else
				mb->split=line[6];
		}
		else if(!strncmp(line,"assist=",7) && (!arg || !arg->assist || !arg->assist[0]))
		{
			mb_load_assist_config(mb,flag,line+7);
		}
		else if((!arg || !arg->dicts) && 
				!strncmp(line,"dicts=",6) && 
				mb->dicts[0]==NULL && 
				!(flag&MB_FLAG_ASSIST))
		{
			char *p=line+6;
			int i,j;
			for(i=0;i<10 && p[0];i++)
			{
				for(j=0;p[j]!=' ' && p[j]!=0;j++);
				mb->dicts[i]=malloc(j+1);
				memcpy(mb->dicts[i],p,j);
				mb->dicts[i][j]=0;
				if(!p[j]) break;
				p+=j+1;
			}
		}
		else if(!strncmp(line,"user=",5) && !(flag&MB_FLAG_ASSIST))
		{
			if(mb->user)
				free(mb->user);
			char file[256];
			int words=0;
			if(mb->encode==1)
			{
				l_utf8_to_gb(line+5,file,sizeof(file));
				strcpy(line+5,file);
			}
			l_sscanf(line+5,"%255s %d",file,&words);
			mb->user=l_strdup(file);
			mb->user_words=words;
		}
		else if(!strncmp(line,"normal=",7) && !(flag&MB_FLAG_ASSIST))
		{
			if(mb->normal)
				free(mb->normal);
			mb->normal=strdup(line+7);
		}
		else if(!strncmp(line,"commit=",7))
		{
			int t1=0,t2=0,t3=0;
			char *p=line+7;
			t1=atoi(p);
			p=strchr(p+1,' ');
			if(p)
			{
				t2=atoi(++p);
				p=strchr(p,' ');
				if(p) t3=atoi(++p);
			}
			//printf("%d %d %d\n",t1,t2,t3);
			mb->commit_mode=(uint8_t)t1;
			mb->commit_len=(uint8_t)t2;
			mb->commit_which=(uint8_t)t3;
		}
		else if(!strncmp(line,"skip=",5))
		{
			//snprintf(mb->skip,8,"%s",line+5);
			l_strcpy(mb->skip,9,line+5);
		}
		else if(!strncmp(line,"bihua=",6))
		{
			if(strlen(line+6)==5)
				strcpy(mb->bihua,line+6);
		}
		else if(!strncmp(line,"code_",5) && !(flag&MB_FLAG_ASSIST))
		{
			if(!strncmp(line,"code_hint=",10))
				mb->code_hint=(line[10]=='1');
			else
				mb_add_rule(mb,line+5);
		}
		else if(!strcasecmp(line,"[DATA]"))
		{
			break;
		}
		else if(!strchr(line,'='))
		{
			rewind(fp);
			break;
		}
		/* todo: add rule */
	}
	
	/* use arg->dicts before y_mb_init_pinyin, or this will be changed */
	if(!(flag&MB_FLAG_ASSIST) && arg && arg->dicts)
	{
		char *p=line;
		int i,j;
		l_utf8_to_gb(arg->dicts,line,sizeof(line));
		for(i=0;i<10 && p[0];i++)
		{
			for(j=0;p[j]!=' ' && p[j]!=0;j++);
			mb->dicts[i]=malloc(j+1);
			memcpy(mb->dicts[i],p,j);
			mb->dicts[i][j]=0;
			if(!p[j]) break;
			p+=j+1;
		}
	}
	if(arg && arg->assist)
		mb_load_assist_config(mb,flag,arg->assist);
			
	if(mb->english)
	{
		mb_load_english(mb,fp);
	}
	else
	{
		if(!(flag&MB_FLAG_ASSIST))
		{
			FILE *fp;
			if(mb->normal)
				fp=y_mb_open_file(mb->normal,"rb");
			else
				fp=y_mb_open_file("normal.txt","rb");
			gb_load_normal(fp);
			if(fp) fclose(fp);
			
			if(mb->pinyin)
			{
				y_mb_init_pinyin(mb);
			}
		}
		if(!(flag&MB_FLAG_ASSIST) || (flag&MB_FLAG_ZI))
		{
			mb->zi=L_HASH_TABLE_INT(struct y_mb_zi,data,7001);
		}
		if(flag==(MB_FLAG_ASSIST|MB_FLAG_ZI|MB_FLAG_CODE))
		{
			mb_load_assist_code(mb,fp,arg->apos);
		}
		else
		{
			if(mb->pinyin && mb->split=='\'')
			{
				long offset=ftell(fp);
				mb_load_zi(mb,fp);
				fseek(fp,offset,SEEK_SET);
			}
			mb_load_data(mb,fp,Y_MB_DIC_MAIN);
		}
	}
	fclose(fp);
	mb->encode=0;
	if(!(flag&MB_FLAG_ASSIST))
	{
		if(arg && arg->zi_freq==1 && mb->pinyin)
		{
			mb->zi_first=create_zi_first_table(mb);
			// printf("zi first %p %d\n",mb->zi_first,l_hash_table_size(mb->zi_first));
		}
		if(!mb->user)
			mb->user=strdup("user.txt");
		for(int i=0;mb->dicts[i] && i<10;i++)
		{
			if(!(flag&MB_FLAG_NODICTS))
				mb_load_sub_dict(mb,mb->dicts[i]);
		}
		if(!(flag&MB_FLAG_NOUSER))
			mb_load_user(mb,mb->user);
		if(mb->ass_main && mb->ass_lead)
		{
			if(flag&MB_FLAG_ADICT)
			{
				fp=y_mb_open_file(mb->ass_main,"rb");
				if(fp)
				{
					mb_load_data(mb,fp,Y_MB_DIC_ASSIST);
					fclose(fp);
				}
			}
			else
			{
				int flag=MB_FLAG_ASSIST;
				/*if(mb->pinyin) */flag|=MB_FLAG_ZI;
				mb->ass_mb=y_mb_load(mb->ass_main,flag,NULL);
			}
		}
	}
	if(mb->encrypt || need_enc)
		mb->encrypt=1;
	mb_mark_simple(mb);
	mb_half_index(mb);
	return 0;
}

struct y_mb *y_mb_load(const char *fn,int flag,struct y_mb_arg *arg)
{
	struct y_mb *mb;
	mb=y_mb_new();
	if(0!=y_mb_load_to(mb,fn,flag,arg))
	{
		y_mb_free(mb);
		return NULL;
	}
	return mb;
}

int y_mb_load_fuzzy(struct y_mb *mb,const char *fuzzy)
{
	FUZZY_TABLE *ft;
	if(mb->encode!=0 || mb->nsort)
		return -1;
	// 加载模糊编码表
	ft=fuzzy_table_load(fuzzy);
	if(!ft)
		return -1;
	mb->fuzzy=ft;
	return 0;
}

void y_mb_save_user(struct y_mb *mb)
{
	struct y_mb_index *index;
	if(!mb->user || !mb->dirty)
		return;

	int has_space=y_mb_is_key(mb,' ');
	LString *str=l_string_new(0x10000);

	for(index=mb->index;index;index=L_CPTR(index->next))
	{
		struct y_mb_item *it=L_CPTR(index->item);
		while(it)
		{
			char *code;
			struct y_mb_ci *cp;
			int pos=0;

			code=mb_key_conv2_r(mb,index->index,it->code);
			if(has_space)
			{
				for(int j=0;code[j]!=0;j++)
				{
					if(code[j]==' ')
						code[j]='_';
				}
			}
			cp=L_CPTR(it->phrase);
			while(cp)
			{
#if 0
				if(mb->split=='\'' && !cp->zi && cp->ext)
				{
					if(!cp->del)
						pos++;
					cp=cp->next;
					continue;
				}
#endif
				const char *ci=(const char*)y_mb_ci_string(cp);
				if(cp->del)
				{
					l_string_append(str,"{-}",3);
					l_string_append(str,code,-1);
					l_string_append_c(str,' ');
					l_string_append(str,ci,-1);
					l_string_append_c(str,'\n');
				}
				else if(cp->dic==Y_MB_DIC_USER)
				{
					if(!mb->nsort)
					{
						char temp[32];
						int len=sprintf(temp,"{%d}",pos);
						l_string_append(str,temp,len);
					}
					l_string_append(str,code,-1);
					l_string_append_c(str,' ');
					if(cp->zi && cp->len!=1 && ci[0]!='$')
					{
						int revert=(gb_is_normal((const uint8_t*)ci))==cp->ext;
						if(revert)
							l_string_append_c(str,'~');
					}
					l_string_append(str,ci,-1);
					l_string_append_c(str,'\n');
				}
				if(!cp->del)
					pos++;
				cp=L_CPTR(cp->next);
			}
			it=L_CPTR(it->next);
		}
	}
	char user[256];
	l_gb_to_utf8(mb->user,user,sizeof(user));
	EIM.Callback(EIM_CALLBACK_ASYNC_WRITE_FILE,user,str,true);
	mb->dirty=0;
}

void y_mb_init(void)
{
	mb_slices=l_slices_new(6,
			(int)sizeof(struct y_mb_item),
			(int)sizeof(struct y_mb_zi),
			(int)sizeof(struct y_mb_index),
			8,12,16,20,24);
}

void y_mb_cleanup(void)
{
	l_slices_free(mb_slices);
	mb_slices=NULL;
}

void y_mb_free(struct y_mb *mb)
{
	if(!mb)
		return;
	if(mb->user && mb->dirty)
		y_mb_save_user(mb);
	for(int i=0;i<10;i++)
		free(mb->dicts[i]);
	free(mb->user);
	free(mb->main);
	free(mb->ass_main);
	if(mb->zi)
		l_hash_table_free(mb->zi,(LFreeFunc)mb_zi_free);
	if(mb->rule)
	{
		struct y_mb_rule *r,*n;
		r=mb->rule;
		while(r)
		{
			n=r->next;
			free(r);
			r=n;
		}
	}
	if(mb->index)
	{
		l_cslist_free(mb->index,(LFreeFunc)mb_index_free1);
	}
	trie_tree_free(mb->trie);
	y_mb_free(mb->ass_mb);
	y_mb_free(mb->quick_mb);
	l_hash_table_free(mb->pin,(LFreeFunc)pin_free);
	
	if(mb->ctx.result_ci)
		l_ptr_array_free(mb->ctx.result_ci,NULL);
	fuzzy_table_free(mb->fuzzy);

	l_hash_table_free(mb->zi_first,l_free);
	y_mb_error_free(mb);
		
	free(mb);
}

int y_mb_has_wildcard(struct y_mb *mb,const char *s)
{
	if(mb->ass_mb && mb->ctx.input[0]==mb->ass_lead && mb->ctx.input[1])
	{
		if(!mb->ctx.input[1])
			return 0;
		return y_mb_has_wildcard(mb->ass_mb,s+1);
	}
	if(!mb->wildcard)
		return 0;
	return strchr(s+mb->dwf,mb->wildcard)?1:0;
}

int y_mb_is_key(struct y_mb *mb,int c)
{
#if 0
	if(mb->ass_mb && mb->ass_lead && mb->ctx.input[0]==mb->ass_lead)
		return y_mb_is_key(mb->ass_mb,c);
	if(mb->quick_mb && mb->quick_lead && mb->ctx.input[0]==mb->quick_lead)
		return y_mb_is_key(mb->quick_mb,c);
#endif
	if((KEYM_MASK&c) || (c>=0x80) || c<=0 /* just for more safe */)
		return 0;
	return mb->map[c]?1:0;
}

int y_mb_is_keys(struct y_mb *mb,const char *s)
{
	// if(s[0]==mb->ass_lead && mb->ass_mb)
		// return y_mb_is_keys(mb->ass_mb,s+1);
	for(int i=0;s[i]!=0;i++)
	{
		if(!y_mb_is_key(mb,s[i]))
			return 0;
	}
	return 1;
}

int y_mb_is_full(struct y_mb *mb,int len)
{
	if(mb->ass_mb && mb->ctx.input[0]==mb->ass_lead)
	{
		if(!mb->ctx.input[1])
			return 0;
		return y_mb_is_full(mb->ass_mb,len-1);
	}
	if(mb->len>len)
		return 0;
	else
		return len+1-mb->len;
}

int y_mb_is_stop(struct y_mb *mb,int c,int pos)
{
	if(mb->ass_mb && mb->ctx.input[0]==mb->ass_lead)
	{
		if(!mb->ctx.input[1] || pos==0)
			return 0;
		return y_mb_is_stop(mb->ass_mb,c,pos-1);
	}
	if(KEYM_MASK&c) return 0;
	if(pos<1 || pos>7) return 0;
	if(mb->stop && !(mb->stop&(1<<pos)))
		return 0;
	return strchr(mb->push,c)?1:0;
}

int y_mb_is_pull(struct y_mb *mb,int c)
{
	if(!mb->pull[0])
		return 0;
	if(mb->pull[0]=='*')
		return 1;
	return strchr(mb->pull,c)?1:0;
}

/* todo: not good enough, but can work with other workaround */
// dext 0: not filter hz 1: filter ext hz 2: only ext hz
int y_mb_has_next(struct y_mb *mb,int dext)
{
	struct y_mb_item *it,*p;
	int len=strlen(mb->ctx.input);
	
	bool test=strcmp(mb->ctx.input,"lei;")==0;
	if(mb->ctx.result_has_next)
	{
		if(test) printf("--\n");
		return 1;
	}

	if(mb->nsort)
	{
		/**
		 * TODO:
		 * 现在只在候选项个数为1时，判断是否有后续编码
		 */
		if(mb->ctx.result_count==1)
		{
			char *code;
			struct y_mb_index *index=mb->ctx.result_index;
			it=mb->ctx.result_first;
			code=mb_key_conv2_r(mb,index->index,it->code);
			if(!strcmp(code,mb->ctx.input))
				return 0;
		}
		return len<mb->len;
	}
	if(len==1)
	{
		/**
		 * TODO:
		 * 在码长为一时，没有考虑到出简不出全，单字模式，有被删除字词存在等情况 
		 * 考虑到实际使用中不太可能在一简处出问题，暂不处理
		 */
		struct y_mb_index *index;
		index=mb->ctx.result_index;
		if(index->index & 0xff)
			return 1;
		index=L_CPTR(index->next);
		if(!index) return 0;
		if(((index->index>>8)&0xff)==mb->map[(int)mb->ctx.input[0]])
			return 1;
		return 0;
	}
	
	it=mb->ctx.result_first;
	assert(it!=NULL);

	p=L_CPTR(it->next);
	while(p)
	{
		if(mb_key_is_part2(p->code,it->code))
		{
			struct y_mb_ci *c=L_CPTR(p->phrase);
			while(c)
			{
				// 单字出简不出全时，忽略这些有简码的字
				if(c->zi && mb->simple && c->simp)
				{
					c=L_CPTR(c->next);
					continue;
				}
				if(c->del)
				{
					c=L_CPTR(c->next);
					continue;
				}
				if(!dext || (dext==1 && !c->ext) || (dext==2 && c->ext))
				{
					return 1;
				}
				c=L_CPTR(c->next);
			}
		}
		else
		{
			if(!mb->nsort)
				break;
		}
		p=L_CPTR(p->next);
	}
	return 0;
}

int y_mb_get_simple(struct y_mb *mb,char *code,char *data,int p)
{
	uint16_t index_val;
	struct y_mb_index *index;
	struct y_mb_item *item;
	struct y_mb_ci *c;
	char *temp;
	int ret;
	index_val=mb_ci_index(mb,code,p,0);
	for(index=mb->index;index;index=L_CPTR(index->next))
	{
		ret=mb_index_cmp_direct(index_val,index->index,2);
		if(ret<0) break;
		if(ret>0) continue;
		if(index->ci_count==0)
			continue;
		item=L_CPTR(index->item);
		temp=mb_key_conv2_r(mb,0,item->code);
		if(temp[0] && (temp[1] || !strchr(mb->push,temp[0])))
			break;
		for(c=L_CPTR(item->phrase);c;c=L_CPTR(c->next))
		{
			if(c->del)
				continue;
			y_mb_ci_string2(c,data);
			return 0;
		}
	}
	
	return -1;
}

static int y_mb_max_match_qp(struct y_mb *mb,const char *s,int len,int dlen,
		int filter,int *good,int *less)
{
	trie_iter_t iter;
	trie_tree_t *trie;
	trie_node_t *n;
	char temp[128];
	py_item_t token[128+1];
	int count;
	int i;
	char *p;
	int match=1,exact=0,exact_l=0;

	for(i=0;i<len && s[i]!=mb->split;i++);
	if(i==len)
	{
		return -1;
	}

	if(s[len]!=0)
	{
		char temp[MAX_CODE_LEN+1];
		l_memcpy0(temp,s,len);
		count=py2_parse_string(temp,token,NULL,NULL);
		token[count]=NULL;
	}
	else
	{
		count=py2_parse_string(s,token,NULL,NULL);
		token[count]=NULL;
	}
	
	if(count<=0)
	{
		return -1;
	}
	int sp_len=py2_build_sp_string(temp,token,count);
	p=strchr(temp,mb->split);
	if(p)
	{
		*p=0;
		sp_len--;
	}
	trie=mb->trie;
	n=trie_iter_path_first(&iter,trie,NULL,64);
	while(n!=NULL)
	{
		int cur=iter.depth;
		if(cur<sp_len && n->self!=temp[cur])
		{
			trie_iter_path_skip(&iter);
			n=trie_iter_path_next(&iter);
			continue;
		}
		if(n->leaf)
		{
			struct y_mb_item *item;
			struct y_mb_ci *c;
			item=trie_node_get_leaf(trie,n)->data;
			c=L_CPTR(item->phrase);
			for(;c!=NULL;c=L_CPTR(c->next))
			{
				if(c->del) continue;
				if(dlen>0 && l_gb_strlen(c->data,c->len)!=dlen) continue;
				if(filter && c->zi && c->ext) continue;
				// if(mb->ctx.sp && c->zi && len>2 && 
				if(cur<sp_len && cur>=exact)
				{
					exact_l=exact;
					exact=cur+1;
				}
				if(cur>=match)
					match=cur+1;
				break;
			}
			if(match>=sp_len)
				break;
		}
		n=trie_iter_path_next(&iter);
	}
	if(good) *good=py2_pos_of_qp(token,exact);
	if(less) *less=py2_pos_of_qp(token,exact_l);
	if(match>sp_len)
	{
		match=len;
	}
	else
	{
		match=py2_pos_of_qp(token,match);
		if(match>=len) match=len;
	}
	return match;
}

int y_mb_max_match_fuzzy(struct y_mb *mb,const char *s,int len,int dlen,
		int filter,int *good,int *less)
{
	LPtrArray *list;
	FUZZY_TABLE *ft=mb->fuzzy;
	int ret;
	assert(ft!=NULL);
	mb->fuzzy=NULL;
	list=fuzzy_key_list(ft,s,len,mb->split);
	if(list->len==1 || mb->nsort || y_mb_has_wildcard(mb,s))
	{
		ret=y_mb_max_match(mb,s,len,dlen,filter,good,less);
	}
	else
	{
		int max=-1;
		int good2=0,less2=0;
		int i;
		*good=0;
		//printf("%d %s\n",list->len,(char*)l_ptr_array_nth(list,0));
		for(i=0;i<list->len;i++)
		{
			char *code=l_ptr_array_nth(list,i);
			int clen=strlen(code);
			int cgood=0;
			ret=y_mb_max_match(mb,code,clen,dlen,filter,&cgood,less);
			if(memcmp(code,s,len))
			{
				if(ret==clen)
				{
					ret=len;
					if(cgood==clen)
						*good=cgood;
				}
				else if(cgood==1 || !memcmp(s,code,cgood))
				{
					*good=cgood;
					ret=cgood;
				}
				else
				{
					ret=0;
				}
			}
			else if(*good<cgood)
			{
				*good=cgood;
			}
			if(ret>max)
			{
				good2=*good;
				if(less) less2=*less;
				max=ret;
			}
		}
		ret=max;
		*good=good2;
		if(less) *less=less2;
	}
	l_ptr_array_free(list,l_free);
	mb->fuzzy=ft;
	if(ret>len) ret=len;
	if(good && *good>len) *good=len;
	if(less && *less>len) *less=len;
	return ret;
}

int y_mb_max_match(struct y_mb *mb,const char *s,int len,int dlen,
		int filter,int *good,int *less)
{
	uint16_t index_val;
	struct y_mb_index *index;
	struct y_mb_item *item;
	int ret;
	int match=1,exact=0,exact_l=0;
	int left;
	int base;
	
	if(mb->fuzzy)
	{
		ret=y_mb_max_match_fuzzy(mb,s,len,dlen,filter,good,less);
		return ret;
	}
	
	if(mb->pinyin && mb->split=='\'' && mb->trie &&
			(ret=y_mb_max_match_qp(mb,s,len,dlen,filter,good,less))>0)
	{
		return ret;
	}
	index_val=mb_ci_index(mb,s,len,0);
	base=(len<=1 || mb->nsort)?1:2;
	s+=base;left=len-base;
	
	for(index=mb->index;index;index=L_CPTR(index->next))
	{
		base=index->index&0xff?2:1;
		ret=mb_index_cmp_direct(index_val,index->index,base);
		if(ret<0)
			break;
		if(ret>0)
			continue;
		if(filter && index->ci_count-index->ext_count==0)
			break;
		for(item=L_CPTR(index->item);item;item=L_CPTR(item->next))
		{
			int i;
			char *key=mb_key_conv2_r(mb,0,item->code);
			for(i=0;i<left && key[i];i++)
			{
				if(s[i]!=key[i]) break;
			}
			if((key[i]==0 && i+base>exact) || i+base>match)
			{
				for(struct y_mb_ci *c=L_CPTR(item->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del) continue;
					if(dlen>0 && l_gb_strlen(c->data,c->len)!=dlen) continue;
					if(filter && c->zi && c->ext) continue;
					if(mb->ctx.sp && c->zi && key[i]!=0) continue;
					if(key[i]==0 && i+base>exact)
					{
						exact_l=exact;
						exact=i+base;
					}
					if(i+base>match)
					{
						match=i+base;
					}
					break;
				}
			}
			/* if entire code match, no need to test next */
			if(match>=len) break;
			/* if the first test is enough */
			if(left<=0) break;
		}
	}
	if(good) *good=exact;
	if(less) *less=exact_l;
	return match;
}

/* detect if the simple key exist, in case of the simple key len >= clen*/
int mb_simple_exist(struct y_mb *mb,const char *s,int clen,struct y_mb_ci *c)
{
	struct y_mb_zi *z;

	uint32_t key=l_gb_to_char(c->data);;
	z=l_hash_table_lookup(mb->zi,&key);
	if(z)
	{
		struct y_mb_code *p;
		for(p=L_CPTR(z->code);p;p=L_CPTR(p->next))
		{
			if(p->virt) continue;
			if(p->len>=clen)
			{
				int i;
				for(i=0;i<clen;i++)
				{
					if(s[i]!=y_mb_code_n_key(mb,p,i))
						break;
				}
				if(i==clen) return 1;
			}
		}
	}
	return 0;
}

#if 1
static int mb_simple_code_match(const char *code,const char *s,int len,uint8_t split)
{
	int i;
	int clen;
	clen=strlen(code);
	if(clen<=len)
		return 0;
	if(code[0]!=s[0])
		return 0;
	if(split && split>='2' && split<='7')
	{
		split-='0';
		if(clen<split*len)
			return 0;
		for(i=0;i<len;i++)
		{
			if(code[split*i]!=s[i])
				return 0;
		}
		return 1;
	}
	else
	{
		char *p;
		for(i=0;i<len;i++)
		{
			p=strchr(code,s[i]);
			if(!p) return 0;
			code=p+1;			
		}
		return 1;
	}
	return 0;
}

static int mb_simple_phrase_match(struct y_mb *mb,const char *c,const char *s,int len)
{
	struct y_mb_zi *z;
	struct y_mb_code *p;
	int i;
	for(i=1;i<len;i++)
	{
		if(!s[i]) break;
		if(!gb_is_gbk((uint8_t*)c+i*2))
		{
			return 0;
		}
		z=mb_find_zi(mb,c+2*i);
		if(!z)
		{
			return 0;
		}
		for(p=L_CPTR(z->code);p;p=L_CPTR(p->next))
		{
			if(y_mb_code_n_key(mb,p,0)==s[i])
				break;
		}
		if(!p)
		{
			return 0;
		}
	}
	return 1;
}
#endif

struct _s_item{
	struct y_mb_ci *c;
	int f;
	int m;
};
static int _s_item_cmpar(struct _s_item *it1,struct _s_item *it2)
{
	int m=it2->m-it1->m;
	if(m) return m;
	return it2->f-it1->f;
}
int y_mb_predict_simple(struct y_mb *mb,char *s,char *out,int *out_len,int (*freq)(const char *))
{
	int len=strlen(s);
	struct y_mb_index *index;
	struct y_mb_item *item;
	struct y_mb_ci *ci;
	uint16_t index_val;
	int ret;
	LArray *array;

	if(len<=1) return 0;
	
	array=l_array_new(26,sizeof(struct _s_item));	
	index_val=mb_ci_index(mb,s,1,0);
	for(index=mb->index;index;index=L_CPTR(index->next))
	{
		ret=mb_index_cmp_direct(index_val,index->index,1);
		if(ret>0) continue;
		if(ret<0) break;
		for(item=L_CPTR(index->item);item;item=L_CPTR(item->next))
		{
			char *code=mb_key_conv2_r(mb,index->index,item->code);
			ret=mb_simple_code_match(code,s,len,mb->split);
			if(!ret) continue;
			for(ci=L_CPTR(item->phrase);ci;ci=L_CPTR(ci->next))
			{
				struct _s_item item;
				char *c;
				if(ci->del || ci->len<2*len)
					continue;
				c=y_mb_ci_string(ci);
				ret=mb_simple_phrase_match(mb,c,s,len);
				if(!ret) continue;
				item.c=ci;
				item.f=(freq && ci->len<15)?freq(c):0;
				if(ci->dic==Y_MB_DIC_USER)
					ci->dic+=10000;
				item.m=(ci->len==2*len);
				l_array_insert_sorted(array,&item,(LCmpFunc)_s_item_cmpar);
				if(!freq && array->len>100)
					array->len=100;
				if(freq && array->len>40)
					array->len=40;
			}
		}
	}
	struct y_mb_ci *prev=NULL;
	for(ret=len=0;ret<array->len;ret++)
	{
		struct _s_item *it=l_array_nth(array,ret);
		struct y_mb_ci *ci=it->c;
		if(len+ci->len+1+1>MAX_CAND_LEN)
			break;
		if(prev!=NULL && ci->len==prev->len && !memcmp(ci->data,prev->data,ci->len))
			continue;
		len+=y_mb_ci_string2(ci,out+len)+1;
		prev=ci;
	}
	out[len]=0;
	l_array_free(array,NULL);
	if(out_len)
		*out_len=len+1;
	return ret;
}

bool y_mb_match_jp(struct y_mb *mb,py_item_t *item,int count,const char *s)
{
	if(s[0]=='$')
		return true;
	for(int i=0;i<count;i++)
	{
		uint32_t hz=l_gb_to_char(s);
		s=l_gb_next_char(s);
		if(!s)
			return false;
		struct y_mb_zi *zi=L_HASH_TABLE_LOOKUP_INT(mb->zi,hz);
		if(!zi)
			return false;
		char temp[16];
		int len=py2_build_string(temp,item+i,1,0);
		struct y_mb_code *found=NULL;
		for(struct y_mb_code *c=L_CPTR(zi->code);c!=NULL;c=L_CPTR(c->next))
		{
			if(c->len<len)
				continue;
			bool not_match=false;
			for(int j=0;temp[j]!=0;j++)
			{
				if(y_mb_code_n_key(mb,c,j)!=temp[j])
				{
					not_match=true;
					break;
				}
			}
			if(!not_match)
			{
				found=c;
				break;
			}
		}
		if(!found)
			return false;
	}
	return true;
}

bool y_mb_ci_py_match(struct y_mb *mb,struct y_mb_ci *c,py_item_t *input,int count)
{
	uint8_t *data=c->data;
	data=l_gb_next_char(data);
	for(int i=1;i<count;i++)
	{
		struct y_mb_zi *z=mb_find_zi(mb,(const char*)data);
		if(!z)
			return false;
		int first=py2_get_jp_code(input[i]);
		struct y_mb_code *code=L_CPTR(z->code);
		for(;code!=NULL;code=L_CPTR(code->next))
		{
			if(first==y_mb_code_n_key(mb,code,0))
				break;
		}
		if(!code)
			return false;
		data=l_gb_next_char(data);
	}
	return true;
}

// 在分词可能有问题的情况下，判断某个词是否和全拼是否匹配，存在问题，需要改进
static int mb_match_quanpin(struct y_mb *mb,struct y_mb_ci *c,int clen,const char *sep)
{
	if(sep==NULL)
	{
#if 0
		if(c->zi || !mb->ctx.sp || c->len!=4)
			return 1;
		struct y_mb_zi *z=mb_find_zi(mb,(char*)&c->data+2);
		if(z)
		{
			struct y_mb_code *code=z->code;
			for(;code!=NULL;code=code->next)
			{
				if(code->len==clen)
					return 1;
			}
		}
		return 0;
#else
		return 1;
#endif
	}
	if(c->zi)
	{
		uint32_t hz=l_gb_to_char(c->data);
		if(hz==0x83bf || hz==0xad99) // 兛瓩
			return 1;
		return 0;
	}
	if(c->len>=4)
	{
		struct y_mb_zi *z=mb_find_zi(mb,(char*)&c->data+2);
		if(z)
		{
			for(struct y_mb_code *code=L_CPTR(z->code);code!=NULL;code=L_CPTR(code->next))
			{
				if(*sep==y_mb_code_n_key(mb,code,0))
					return 1;
			}
			return 0;
		}
	}
	return 1;
}

static LArray *add_fuzzy_phrase(LArray *head,struct y_mb *mb,struct y_mb_context *ctx)
{
	struct y_mb_index *index;
	struct y_mb_item *item,*p;
	char *s;
	uint8_t *key;
	uint16_t index_val;
	int len,left;
	int ret;
	int filter,filter_zi,filter_ext;
	int got=0;
	int i;
	int (*extern_match)(struct y_mb *,struct y_mb_ci *,int clen,const char*)=NULL;
	int last[4]={0};
	int count=0;
	const char *sep=NULL;
	
	index=ctx->result_index;
	item=ctx->result_first;
	
	s=ctx->input;
	len=strlen(s);
	index_val=mb_ci_index_wildcard(mb,s,len,0,&key);
	left=mb_key_len2(key);
	filter=ctx->result_filter;
	filter_zi=ctx->result_filter_zi || ctx->result_filter_ext;
	filter_ext=ctx->result_filter_ext;
	if(mb->pinyin==1 && mb->split=='\'')
	{
		char *temp=alloca(len+1);
		const char*p;
		int i;
		sep=strchr(s,'\'');
		if(sep)
		{
			sep++;
			for(p=s,i=0;p<s+len;p++)
			{
				if(*p=='\'') continue;
				temp[i++]=*p;
			}
			temp[i]=0;len=i;
			s=temp;
		}
		extern_match=mb_match_quanpin;
	}
	
	for(;index;index=L_CPTR(index->next))
	{
		ret=mb_index_cmp_direct(index_val,index->index,len);
		if(ret<0)
			break;
		if(ret!=0)
			continue;
		if(index->ci_count==0)
			continue;
		if(filter && index->ci_count-index->ext_count==0)
			continue;
		for(p=(index==ctx->result_index)?item:L_CPTR(index->item);p;p=L_CPTR(p->next))
		{
			int clen=mb_key_len2(p->code)+(index->index&0xff?2:1);
			struct y_mb_ci *c;
			ret=mb_key_cmp_direct2(key,p->code,left);
			if(ret>0) continue;
			if(ret<0) break;
			for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
			{
				if(c->del) continue;
				if(filter && c->zi && c->ext) continue;
				if(filter_zi && !c->zi) continue;
				if(c->zi && mb->simple && c->simp) continue;
				if(filter_ext && c->zi && !c->ext) continue;
				if(ctx->result_compact==0)
				{
					if(c->zi && mb->compact && c->simp && clen>len && mb_simple_exist(mb,s,len,c)) continue;
					if(!c->zi && mb->compact && clen>len+mb->compact-1) continue;
				}
				if(extern_match && !extern_match(mb,c,len,sep)) continue;
				
				/* 限制每个模糊音的候选，避免太多候选导致输入法长时间失去响应 */
				count++;
				if(count>1536) break;

				if(!head)
					head=l_ptr_array_new(1024);

				if(c->len>2)
					i=c->len<=8?last[(c->len+1)/2-2]:last[3];
				else
					i=0;
				for(;i<head->len;i++)
				{
					struct y_mb_ci *p=l_ptr_array_nth(head,i);
					if(p->len<c->len) continue;
					if(p->len>c->len)
					{
						l_ptr_array_insert(head,i,c);
						if(c->len+1<=8)
							last[(c->len+1)/2-1]=i+1;
						break;
					}
					if(mb_ci_equal(c,(char*)p->data,p->len))
						break;
				}
				if(i==head->len)
				{
					l_ptr_array_append(head,c);
					if(c->len+1<=8)
						last[(c->len+1)/2-1]=i+1;
				}
				got++;
				if(got==ctx->result_count)
					break;
			}
			if(got==ctx->result_count)
				break;
		}
		if(got==ctx->result_count)
			break;
	}
	return head;
}

static int mb_pin_phrase_fuzzy(struct y_mb *mb,LArray *list,const char *code)
{
	struct y_mb_pin_item *item;
	struct y_mb_pin_ci *c;
	if(!mb->pin)
		return 0;
	int len=(uint8_t)strlen(code);
	if(len>8)
		return 0;
	item=l_hash_table_lookup(mb->pin,code);
	if(!item)
		return 0;
	for(c=item->list;c!=NULL;c=c->next)
	{
		for(int i=0;i<list->len;i++)
		{
			struct y_mb_ci *p=l_ptr_array_nth(list,i);
			if(mb_ci_equal(p,(char*)c->data,c->len))
			{
				l_ptr_array_remove(list,i);
				l_ptr_array_insert(list,c->pos,p);
				break;
			}
		}
	}
	return 0;
}

//#include <time.h>
int y_mb_set_fuzzy(struct y_mb *mb,const char *s,int len,int filter)
{
	LArray *list;
	FUZZY_TABLE *ft=mb->fuzzy;
	int ret;
	assert(ft!=NULL);
	assert(mb->ctx.result_ci==NULL);
	mb->fuzzy=NULL;

	list=fuzzy_key_list(ft,s,len,mb->split);
	if(list->len==1 || mb->nsort || y_mb_has_wildcard(mb,s))
	{
		ret=y_mb_set(mb,s,len,filter);
	}
	else
	{
		struct y_mb_context first;
		int i;
		int found=0;
		int count=0;
		LArray *head=NULL;
		for(i=0;i<list->len;i++)
		{
			char *code=l_ptr_array_nth(list,i);
			int clen=strlen(code);
			ret=y_mb_set(mb,code,clen,filter);
			if(ret<=0) continue;
			found++;
			count+=ret;
			if(found==1)
			{
				y_mb_push_context(mb,&first);
				continue;
			}
			else if(found==2)
			{
				//clock_t start=clock();
				head=add_fuzzy_phrase(NULL,mb,&first);
				//printf("%.3f\n",(clock()*1.0-start)/CLOCKS_PER_SEC);
			}
			//clock_t start=clock();
			head=add_fuzzy_phrase(head,mb,&mb->ctx);
			//printf("%.3f\n",(clock()*1.0-start)/CLOCKS_PER_SEC);
		}
		if(found==1)
		{
			y_mb_pop_context(mb,&first);
		}
		else if(found>1)
		{
			mb_pin_phrase_fuzzy(mb,head,l_ptr_array_nth(list,0));
			count=head->len;
			mb->ctx.result_dummy=2;
			mb->ctx.result_ci=head;
			mb->ctx.result_count=count;
		}
		ret=count;
	}
	l_ptr_array_free(list,l_free);
	mb->fuzzy=ft;
	return ret;
}

int y_mb_set(struct y_mb *mb,const char *s,int len,int filter)
{
	int wildcard;
	uint16_t index_val;
	struct y_mb_index *index;
	int ret;
	int left;
	struct y_mb_index *index_first=0;
	struct y_mb_item *item=0;
	int count=0,count_zi=0,count_ci_ext=0;
	uint8_t *key;
	struct y_mb_context *ctx=&mb->ctx;
	int (*extern_match)(struct y_mb *,struct y_mb_ci *,int,const char *)=NULL;
	const char *orig=s;
	int orig_len=len;
	const char *sep=NULL;

	ctx->result_dummy=0;
	ctx->result_has_next=0;
	if(mb->ctx.result_ci!=NULL)
	{
		l_ptr_array_free(mb->ctx.result_ci,NULL);
		mb->ctx.result_ci=NULL;
	}
	if(!len)
	{
		ctx->result_count=0;
		ctx->result_count_zi=0;
		ctx->result_count_ci_ext=0;
		return 0;
	}
	if(mb->fuzzy)
	{
		return y_mb_set_fuzzy(mb,s,len,filter);
	}
	
	if(mb->pinyin==1 && mb->split=='\'')
	{
		sep=strchr(s,'\'');
		if(sep)
		{
			const char*p;
			int i;
			sep++;
			char *temp=l_alloca(len);
			for(p=s,i=0;p<s+len;p++)
			{
				if(*p=='\'') continue;
				temp[i++]=*p;
			}
			temp[i]=0;len=i;
			s=temp;
		}
		extern_match=mb_match_quanpin;
	}

	wildcard=y_mb_has_wildcard(mb,s);
	index_val=mb_ci_index_wildcard(mb,s,len,mb->wildcard,&key);
	left=mb_key_len2(key);
	if((mb->match || mb->ctx.result_match) && mb_ci_index_code_len(index_val)+left!=len)
	{
		return 0;
	}

	for(index=mb->index;index;index=L_CPTR(index->next))
	{
		int base=index->index&0xff?2:1;
		if(wildcard)
		{
			ret=mb_index_cmp_wildcard(mb,index_val,index->index,len,mb->wildcard);
		}
		else
		{
			ret=mb_index_cmp_direct(index_val,index->index,len);
			if(ret<0) break;
		}
		if(ret!=0)
			continue;
		if(index->ci_count==0)
			continue;
		if(filter && index->ci_count-index->ext_count==0)
			continue;
		if(wildcard)
		{
			struct y_mb_item *p;			
			for(p=L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				ret=mb_key_cmp_wildcard2(key,p->code,Y_MB_KEY_SIZE);
				if(ret!=0) continue;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del) continue;
					if(filter && c->zi && c->ext) continue;
					if(!item)
					{
						item=p;
						index_first=index;
					}
					count++;
					if(c->zi)
						count_zi++;
					else if(c->ext)
						count_ci_ext++;
				}
			}
		}
		else if(mb->match || ctx->result_match || len>base || mb->compact || ctx->result_filter_ci_ext)
		{
			struct y_mb_item *p;
			if((p=L_CPTR(index->half))!=NULL)
			{
				ret=mb_key_cmp_direct2(key,p->code,(ctx->result_match?Y_MB_KEY_SIZE:left));
				if(ret<=0) p=L_CPTR(index->item);
			}
			else
			{
				p=L_CPTR(index->item);
			}
			for(/*p=index->item*/;p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				ret=mb_key_cmp_direct2(key,p->code,(ctx->result_match?Y_MB_KEY_SIZE:left));
				if(ret>0) continue;
				if(mb->nsort && ret<0) continue;
				if(ret<0) break;
	
				int clen=mb_key_len2(p->code)+base;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del) continue;
					if(filter && c->zi && c->ext) continue;
					if(c->zi && (mb->simple==1 || mb->simple==2) && c->simp) continue;
					if(extern_match && !extern_match(mb,c,len,sep)) continue;
					if(mb->ctx.sp && c->zi && len>2 && clen!=len) continue;
					
					if(!item)
					{
						item=p;
						index_first=index;
					}
					if(c->zi && mb->compact && c->simp && clen>len && mb_simple_exist(mb,s,len,c))
					{
						ctx->result_has_next=1;
						continue;
					}
					if(mb->compact==1 || mb->compact==2)
					{
						if(!c->zi && clen>len+mb->compact-1)
						{
							ctx->result_has_next=1;
							continue;
						}
					}
					else if(mb->compact==3)
					{
						if(clen>len+1)
						{
							ctx->result_has_next=1;
							continue;
						}
					}
					count++;
					if(c->zi)
						count_zi++;
					else if(c->ext)
						count_ci_ext++;
					if(count==1)
					{
						item=p;
						index_first=index;
					}
					if((mb->match || ctx->result_match) && index->index!=index_val)
						break;
					if(mb->compact && len==1 && clen!=1)
						break;
				}
				if(ctx->result_filter_zi)
				{
					if((count_zi) && (mb->match || ctx->result_match))
						break;
					if(ctx->result_filter_ci_ext && count_ci_ext && (mb->match || ctx->result_match))
						break;
				}
				else if(count && (mb->match || ctx->result_match))
				{
					break;
				}
				if(mb->compact && len==1 && clen!=1)
				{
					/* not detect if found at this index, so problem
					 * will happen, when first item have no valid 
					 * phrase and with second item
					 */
					break;
				}
			}
		}
		else
		{
			if(!item)
			{
				item=L_CPTR(index->item);
				index_first=index;
			}
			count+=index->ci_count;
			count_zi+=index->zi_count;
			if(filter)
			{
				count-=index->ext_count;
				count_zi-=index->ext_count;
			}
		}
		/* only test one index is enough, so break */
		if(!wildcard && len!=1)
			break;
		if(!wildcard && len==1 && (mb->match || ctx->result_match) && count)
			break;
	}
	if(!count && item && mb->compact && !wildcard)
	{
		/* in compact mode, if nothing found, but have normal match,
		 * just use the first normal match one */
		count=1;
		ctx->result_compact=1;
	}
	else
	{
		ctx->result_compact=0;
	}

	if(count)
	{
		ctx->result_count=count;
		ctx->result_first=item;
		ctx->result_filter=filter;
		ctx->result_index=index_first;
		ctx->result_wildcard=wildcard;
		ctx->result_count_zi=count_zi;
		ctx->result_count_ci_ext=count_ci_ext;
		memcpy(ctx->input,orig,orig_len);
		ctx->input[orig_len]=0;
	}
	else
	{
		// 仅仅在count>0的时候修改ctx，使空码保留上一个状态状态
		//ctx->result_count=0;
		//ctx->result_count_zi=0;
	}
	
	if(ctx->result_filter_zi)
	{
		if(ctx->result_filter_ci_ext)
			return count_zi+count_ci_ext;
		else
			return count_zi;
	}
	else
	{
		return count;
	}
}

void y_mb_set_zi(struct y_mb *mb,int zi)
{
	if(zi/* && mb->result_count_zi*/)
		mb->ctx.result_filter_zi=1;
	else
		mb->ctx.result_filter_zi=0;
}

void y_mb_set_ci_ext(struct y_mb *mb,int ci_ext)
{
	if(mb->ctx.result_filter_zi && ci_ext)
		mb->ctx.result_filter_ci_ext=1;
	else
		mb->ctx.result_filter_ci_ext=0;
}

static int y_mb_get_fuzzy(struct y_mb *mb,int at,int num,
	char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	struct y_mb_ci *c;
	int i;
	for(i=0;i<num;i++)
	{
		c=l_ptr_array_nth(((LArray*)mb->ctx.result_ci),at+i);
		y_mb_ci_string2(c,cand[i]);
		if(tip) tip[i][0]=0;
	}
	return 0;
}

int y_mb_get(struct y_mb *mb,int at,int num,
	char cand[][MAX_CAND_LEN+1],char tip[][MAX_TIPS_LEN+1])
{
	char *s;
	int filter;
	int filter_zi;
	int filter_ext;
	uint8_t *key;
	int wildcard;
	int len,left;
	int ret;
	uint16_t index_val;
	struct y_mb_index *index;
	struct y_mb_item *item;
	int skip=0,got=0;
	struct y_mb_context *ctx=&mb->ctx;
	int (*extern_match)(struct y_mb *,struct y_mb_ci *,int,const char *)=NULL;
	const char *sep=NULL;
	
	if(num==0) return 0;

	s=ctx->input;
	len=strlen(s);
	if(ctx->result_dummy==2)
	{
		return y_mb_get_fuzzy(mb,at,num,cand,tip);
	}
	assert(at+num<=ctx->result_count);
	
	if(mb->pinyin==1 && mb->split=='\'')
	{
		sep=strchr(s,'\'');
		if(sep)
		{
			char *temp=alloca(len+1);
			const char*p;
			int i;
			sep++;
			for(p=s,i=0;p<s+len;p++)
			{
				if(*p=='\'') continue;
				temp[i++]=*p;
			}
			temp[i]=0;len=i;
			s=temp;
		}
		extern_match=mb_match_quanpin;
	}
	
	wildcard=ctx->result_wildcard;
	index_val=mb_ci_index_wildcard(mb,s,len,mb->wildcard,&key);
	left=mb_key_len2(key);
	filter=ctx->result_filter;
	filter_zi=ctx->result_filter_zi || ctx->result_filter_ext;
	filter_ext=ctx->result_filter_ext;
	index=ctx->result_index;
	item=ctx->result_first;
	
	if(mb->nsort && !wildcard)
	{
		for(;index;index=L_CPTR(index->next))
		{
			struct y_mb_item *p;
			ret=mb_index_cmp_direct(index_val,index->index,len);
			if(ret<0)
				break;
			if(ret!=0)
				continue;
			if(index->ci_count==0)
				continue;
			if(filter && index->ci_count-index->ext_count==0)
				continue;			
			for(p=(index==ctx->result_index)?item:L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				ret=mb_key_cmp_direct2(key,p->code,Y_MB_KEY_SIZE);
				if(ret!=0) continue;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del) continue;
					if(filter && c->zi && c->ext) continue;
					if(filter_zi && !c->zi && !(c->ext && ctx->result_filter_ci_ext)) continue;
					if(filter_ext && c->zi && !c->ext) continue;
					if(skip<at)
					{
						skip++;
						continue;
					}
					y_mb_ci_string2(c,cand[got]);
					if(!mb->english && tip!=NULL)
					{
						if(c->dic==Y_MB_DIC_ASSIST)
						{
							if(c->zi)
								y_mb_get_full_code(mb,cand[got],tip[got]);
							else
								y_mb_get_exist_code(mb,cand[got],tip[got]);
						}
						else
						{
							strcpy(tip[got],mb_key_conv2_r(mb,index->index,p->code)+len);
						}
					}
					got++;
					if(got==num) break;
				}
				if(got==num) break;
			}
			if(got==num) return 0;
		}
		/* restore index */
		index=ctx->result_index;
	}

	for(;index;index=L_CPTR(index->next))
	{
		if(wildcard)
		{
			ret=mb_index_cmp_wildcard(mb,index_val,index->index,len,mb->wildcard);
		}
		else
		{
			ret=mb_index_cmp_direct(index_val,index->index,len);
			if(ret<0)
				break;
		}
		if(ret!=0)
			continue;
		if(index->ci_count==0)
			continue;
		if(filter && index->ci_count-index->ext_count==0)
			continue;
		if(wildcard)
		{
			struct y_mb_item *p;
			for(p=(index==ctx->result_index)?item:L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				ret=mb_key_cmp_wildcard2(key,p->code,Y_MB_KEY_SIZE);
				if(ret!=0) continue;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del) continue;
					if(filter && c->zi && c->ext) continue;
					if(filter_zi && !c->zi && !(c->ext && ctx->result_filter_ci_ext)) continue;
					if(filter_ext && c->zi && !c->ext) continue;
					if(skip<at)
					{
						skip++;
						continue;
					}
					if(!mb->english && tip!=NULL)
						strcpy(tip[got],mb_key_conv2_r(mb,index->index,p->code));
					y_mb_ci_string2(c,cand[got]);
					got++;
					if(got==num) break;
				}
				if(got==num) break;
			}
			if(got==num) break;
		}
		else
		{
			struct y_mb_item *p;
			for(p=(index==ctx->result_index)?item:L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				int clen=mb_key_len2(p->code)+(index->index&0xff?2:1);
				struct y_mb_ci *c;
				ret=mb_key_cmp_direct2(key,p->code,left);
				if(ret>0) continue;
				if(mb->nsort)
				{
					if(ret<0)
						continue;
					if(0==mb_key_cmp_direct2(key,p->code,Y_MB_KEY_SIZE))
						continue; /* this have been got */
				}
				if(ret<0) break;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					/* 标记为删除的 */
					if(c->del) continue;
					/* 扩展字符集的字被过滤 */
					if(filter && c->zi && c->ext) continue;
					/* 只要单字但却不是单字 */
					if(filter_zi && !c->zi && !(ctx->result_filter_ci_ext && c->ext)) continue;
					/* 出简不出全碰到了有简码的单字 */
					if(c->zi && mb->simple && c->simp) continue;
					/* 只要非常用汉字，而当前非字或者是常用汉字 */
					if(filter_ext && c->zi && !c->ext) continue;
					if(extern_match && !extern_match(mb,c,len,sep)) continue;
					if(mb->ctx.sp && c->zi && len>2 && clen!=len) continue;
					if(ctx->result_compact==0)
					{
						if(c->zi && mb->compact && c->simp && clen>len && mb_simple_exist(mb,s,len,c)) continue;
						if(!c->zi && (mb->compact==1 || mb->compact==2) && clen>len+mb->compact-1) continue;
						if(mb->compact==3 && clen>len+1) continue;
					}
					/* 跳过不需要取的部分 */
					if(skip<at)
					{
						skip++;
						/* 在一简的时候，每一个非完全匹配的编码只需要一个 */
						if(mb->compact && len==1 && clen!=1) break;
						continue;
					}
					y_mb_ci_string2(c,cand[got]);
					if(!mb->english && tip!=NULL)
					{
						if(c->dic==Y_MB_DIC_ASSIST)
						{
							if(c->zi)
								y_mb_get_full_code(mb,cand[got],tip[got]);
							else
								y_mb_get_exist_code(mb,cand[got],tip[got]);
						}
						else
						{
							strcpy(tip[got],mb_key_conv2_r(mb,index->index,p->code)+len);
						}
					}
					got++;
					if(got==num) break;
					if(mb->compact && len==1 && clen!=1)
						break;
				}
				if(got==num) break;
				if(mb->compact && len==1 && clen!=1)
					break;
			}
			if(got==num) break;
		}
	}
	return 0;
}

/* 现在只考虑拼音输入法的单字情况 */
int y_mb_in_result(struct y_mb *mb,struct y_mb_ci *c)
{
	struct y_mb_context *ctx=&mb->ctx;
	struct y_mb_ci *p;
	
	if(ctx->result_ci)
	{
		LArray *arr=ctx->result_ci;
		int i;		
		for(i=0;i<arr->len;i++)
		{
			p=l_ptr_array_nth(arr,i);
			if(!p->zi || p->del)
				continue;
			if(mb_ci_equal(p,(const char*)c->data,c->len))
			{
				return 1;
			}
		}
		return 0;
	}
	if(ctx->result_count_zi==0 || !ctx->result_first)
		return 0;
	for(p=L_CPTR(ctx->result_first->phrase);p!=NULL;p=L_CPTR(p->next))
	{
		if(!p->zi || p->del)
			continue;
		if(mb_ci_equal(p,(const char*)c->data,c->len))
		{
			return 1;
		}
	}
	return 0;
}

struct y_mb_item *y_mb_get_zi(struct y_mb *mb,const char *s,int len,int filter)
{
	struct y_mb_index *index;
	struct y_mb_item *item;
	uint8_t *key;
	uint16_t index_val;
	int ret;
	
	index_val=mb_ci_index(mb,s,len,&key);
	
	for(index=mb->index;index;index=L_CPTR(index->next))
	{
		ret=mb_index_cmp_direct(index_val,index->index,2);
		if(ret<0)
			break;
		if(ret!=0)
			continue;
		if(index->zi_count==0)
			continue;
		for(item=L_CPTR(index->item);item;item=L_CPTR(item->next))
		{
			struct y_mb_ci *c;
			ret=mb_key_cmp_direct2(key,item->code,Y_MB_KEY_SIZE);
			if(ret>0) continue;
			if(ret<0) break;
			for(c=L_CPTR(item->phrase);c;c=L_CPTR(c->next))
			{
				if(c->del) continue;
				if(!c->zi) continue;
				if(filter && c->zi && c->ext) continue;
				return item;
			}
			return NULL;
		}
	}

	return 0;
}

struct y_mb_ci *y_mb_get_first(struct y_mb *mb,char *cand,char *tip)
{
	char *s;
	int filter;
	int filter_zi;
	uint8_t *key;
	int len;
	int ret;
	uint16_t index_val;
	struct y_mb_index *index;
	struct y_mb_item *item;
	struct y_mb_context *ctx=&mb->ctx;
	int left;

	s=ctx->input;
	len=strlen(s);

	assert(ctx->result_count>=1);
	index_val=mb_ci_index_wildcard(mb,s,len,mb->wildcard,&key);
	left=mb_key_len2(key);
	filter=ctx->result_filter;
	filter_zi=ctx->result_filter_zi;
	index=ctx->result_index;
	item=ctx->result_first;

	for(;index;index=L_CPTR(index->next))
	{
		ret=mb_index_cmp_direct(index_val,index->index,len);
		if(ret<0)
			break;
		if(ret!=0)
			continue;
		if(index->ci_count==0)
			continue;
		if(filter && index->ci_count-index->ext_count==0)
			continue;
		{
			struct y_mb_item *p;	
			for(p=(index==ctx->result_index)?item:L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				ret=mb_key_cmp_direct2(key,p->code,left);
				if(ret>0) continue;
				if(mb->nsort && ret<0) continue;
				if(ret<0) break;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del) continue;
					if(filter && c->zi && c->ext) continue;
					if(filter_zi && !c->zi) continue;
					if(cand)
					{
						memcpy(cand,c->data,c->len);
						cand[c->len]=0;
					}
					if(tip)
					{
						strcpy(tip,mb_key_conv2_r(mb,index->index,p->code)+len);
					}
					return c;
				}
			}
		}
	}
	return NULL;
}

struct _l_item{struct y_mb_ci *c;int f;};
static int _l_item_cmpar(struct _l_item *it1,struct _l_item *it2)
{return it2->f-it1->f;}
int y_mb_get_assoc(struct y_mb *mb,const char *src,int slen,
		int dlen,char calc[][MAX_CAND_LEN+1],int max)
{
	struct y_mb_zi *z;
	struct y_mb_code *c;
	char start[4]={0,0,0,0};
	int i=0;
	int count;
	LArray *array;
		
	if(slen<2 || !gb_is_gbk((uint8_t*)src) || !mb->zi)
		return 0;
	z=mb_find_zi(mb,src);
	if(!z)
		return 0;
	for(c=L_CPTR(z->code);c;c=L_CPTR(c->next))
	{
		int j;
		char key;
		if(c->virt) continue;
		key=y_mb_code_n_key(mb,c,0);
		for(j=0;j<i;j++)
		{
			if(start[j]==key)
				break;
		}
		if(j==i)
		{
			start[i++]=key;
			if(i==4) break;
		}
	}
	dlen*=2; /* dlen in is in hz count, so double it */
	
	array=l_array_new(max+1,sizeof(struct _l_item));
		
	for(i=0;start[i] && i<4;i++)
	{
		uint16_t index_val;
		struct y_mb_index *index;
		struct y_mb_item *p;
		char temp[2]={start[i],0};
		index_val=mb_ci_index(mb,temp,1,0);
		
		for(index=mb->index;index;index=L_CPTR(index->next))
		{
			int ret=mb_index_cmp_direct(index_val,index->index,1);
			if(ret<0) break;
			if(ret>0) continue;
			if(index->ci_count-index->zi_count==0)
				continue;
			for(p=L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				int n;
				for(c=L_CPTR(p->phrase),n=0;c;c=L_CPTR(c->next))
				{
					char *res;
					if(c->del) continue;
					n++;
					if(c->zi || c->len<dlen || c->len<=slen)
						continue;
					res=y_mb_ci_string(c);
					if(!strncmp(res,src,slen))
					{
						struct _l_item item;
						item.c=c;
						item.f=-c->len*10-n; /* 词长短的优先，词序在前的优先 */
						l_array_insert_sorted(array,&item,(LCmpFunc)_l_item_cmpar);
						if(array->len>max)
							array->len=max;
					}
				}
			}
		}
	}
	count=array->len;
	for(i=0;i<count;i++)
	{
		struct _l_item *it=l_array_nth(array,i);
		char *s=y_mb_ci_string(it->c);
		strcpy(calc[i],s+slen);
	}
	l_array_free(array,NULL);
	return count;
}

static int mb_ci_start_code(struct y_mb *mb,const char *data,char *start,int size)
{
	int i=0;
	struct y_mb_zi *z;
	struct y_mb_code *c;

	if(!mb->zi)
		return 0;
	z=mb_find_zi(mb,data);
	if(!z)
		return 0;
	for(c=L_CPTR(z->code);c;c=L_CPTR(c->next))
	{
		int j;
		char key;
		if(c->virt) continue;
		key=y_mb_code_n_key(mb,c,0);
		for(j=0;j<i;j++)
		{
			if(start[j]==key)
				break;
		}
		if(j==i)
		{
			start[i++]=key;
			if(i==size) break;
		}
	}
	return i;
}

struct y_mb_ci *y_mb_ci_exist(struct y_mb *mb,const char *data,int dic)
{
	char start[4];
	int i,count;
	int dlen=strlen(data);
	
	count=mb_ci_start_code(mb,data,start,sizeof(start));
	for(i=0;i<count;i++)
	{
		uint16_t index_val;
		struct y_mb_index *index;
		struct y_mb_item *p;
		char temp[2]={start[i],0};
		index_val=mb_ci_index(mb,temp,1,NULL);
		
		for(index=L_CPTR(mb->index);index;index=L_CPTR(index->next))
		{
			int ret=mb_index_cmp_direct(index_val,index->index,1);
			if(ret<0) break;
			if(ret>0) continue;
			for(p=L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del || c->len!=dlen || c->dic==Y_MB_DIC_ASSIST || (dic>=0 && c->dic!=dic))
						continue;
					if(!mb_ci_equal(c,data,dlen))
						continue;
					return c;
				}
			}
		}
	}
	return NULL;
}

static int mb_del_temp_phrase(struct y_mb *mb,const char *data)
{
	int dlen=strlen(data);
	char start[4];
	int count=mb_ci_start_code(mb,data,start,sizeof(start));
	for(int i=0;i<count;i++)
	{
		uint16_t index_val;
		struct y_mb_index *index;
		struct y_mb_item *p;
		char temp[2]={start[i],0};
		index_val=mb_ci_index(mb,temp,1,NULL);
		
		for(index=mb->index;index;index=L_CPTR(index->next))
		{
			int ret=mb_index_cmp_direct(index_val,index->index,1);
			if(ret<0) break;
			if(ret>0) continue;
			for(p=L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					if(c->dic==Y_MB_DIC_TEMP && c->len==dlen && !memcmp(c->data,data,dlen))
					{
						p->phrase=L_CPTR_T(l_cslist_remove(L_CPTR(p->phrase),c));
						mb_ci_free(c);
						return 0;
					}
				}
			}
		}
	}
	return 0;
}

int y_mb_get_exist_code(struct y_mb *mb,const char *data,char *code)
{
	struct y_mb_zi *z;
	struct y_mb_code *c;
	char start[4]={0,0,0,0};
	int i=0;
	int count=0;
	int dlen;
	
	dlen=strlen(data);
	if(dlen<2 || !mb->zi)
		return 0;
	z=mb_find_zi(mb,data);
	
	if(!z)
		return 0;
	/* find a list of first code of the phrase */
	for(c=L_CPTR(z->code);c;c=L_CPTR(c->next))
	{
		int j;
		char key;
		if(c->virt) continue;
		key=y_mb_code_n_key(mb,c,0);
		for(j=0;j<i;j++)
		{
			if(start[j]==key)
				break;
		}
		if(j==i)
		{
			start[i++]=key;
			if(i==4) break;
		}
	}
	for(i=0;start[i] && i<4;i++)
	{
		uint16_t index_val;
		struct y_mb_index *index;
		struct y_mb_item *p;
		char temp[2]={start[i],0};
		index_val=mb_ci_index(mb,temp,1,NULL);
		
		for(index=mb->index;index;index=L_CPTR(index->next))
		{
			int ret=mb_index_cmp_direct(index_val,index->index,1);
			if(ret<0) break;
			if(ret>0) continue;
			for(p=L_CPTR(index->item);p;p=L_CPTR(p->next))
			{
				struct y_mb_ci *c;
				for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
				{
					if(c->del || c->dic==Y_MB_DIC_ASSIST)
						continue;
					if(!c->zi &&  c->ext)
					{
						const char *s=y_mb_ci_string(c);
						if(s[0]=='$' && s[1]=='[')
						{
							s=y_mb_skip_display(s,-1);
							if(dlen==strlen(s) && !memcmp(s,data,dlen))
							{
								if(code)
									strcpy(code,mb_key_conv2_r(mb,index->index,p->code));
								count++;
								goto out;
							}
						}
					}
					if(c->len!=dlen )
						continue;
					if(!mb_ci_equal(c,data,dlen))
						continue;
					if(code)
						strcpy(code,mb_key_conv2_r(mb,index->index,p->code));
					count++;
					goto out;
				}
			}
		}
	}
out:
	return count;
}

static int _mb_super_test(struct y_mb *mb,const uint8_t *s,char super)
{
	if(mb->ass_mb && !mb->ass_lead)		// use extern assist code instead
	{
		mb=mb->ass_mb;
		int key=mb->key[(int)super];
		super=mb->map[(int)key];
		if(!super)
			return 0;
		struct y_mb_zi *z=mb_find_zi(mb,(const char*)s);
		if(!z)
			return 0;
		for(struct y_mb_code *p=L_CPTR(z->code);p;p=L_CPTR(p->next))
		{
			if(super==((p->val>>8)&0x3f))
			return 1;
		}
		return 0;
	}
	struct y_mb_zi *z=mb_find_zi(mb,(const char*)s);
	if(!z)
		return 0;
	for(struct y_mb_code *p=L_CPTR(z->code);p;p=L_CPTR(p->next))
	{
		if(p->virt || p->len<3) continue;
		if(super==((p->val>>20)&0x3f))
			return 1;
	}
	return 0;
}

static int mb_super_test(struct y_mb *mb,struct y_mb_ci *c,char super)
{
	if(c->zi || c->len<2 || !mb->zi)
		return 0;
	const uint8_t *s=c->data;
	if((mb->yong&0x02))
	{
		if(_mb_super_test(mb,s,super))
			return 1;
		if(c->zi)
			return 0;
	}
	if((mb->yong&0x01))
	{
		s+=c->len-2;
		if(c->len>=4 && s[0]<=0xFE && s[0]>=0x81 && s[1]<=0x39 && s[1]>=0x30)
		{
			s-=2;
		}
	}
	return _mb_super_test(mb,s,super);
}

int y_mb_super_get(struct y_mb *mb,char calc[][MAX_CAND_LEN+1],int max,char super)
{
	char *s;
	int count=0;
	int first_match=0;
	struct y_mb_item *p;
	struct y_mb_context *ctx=&mb->ctx;

	s=ctx->input;
	if(s[0]==mb->ass_lead)
		return 0;
	if(ctx->result_wildcard)
		return 0;
	super=mb->map[(int)super];
	if(!super)
		return 0;
	p=ctx->result_first;

	if(p)
	{
		struct y_mb_ci *c;
		int pos=0;
		for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
		{
			if(c->del || c->zi) continue;
			pos++;
			if(mb_super_test(mb,c,super))
			{
				strcpy(calc[count],y_mb_ci_string(c));
				count++;
				if(count==max)
					break;
				if(pos==1)
					first_match=1;
			}
		}
	}
	if(count>1 && first_match)
	{
		char temp[MAX_CAND_LEN+1];
		strcpy(temp,calc[0]);
		for(int i=1;i<count;i++)
			strcpy(calc[i-1],calc[i]);
		if(mb->commit_mode!=3)
			strcpy(calc[count-1],temp);
		else
			count--;
	}
	return count;
}

static bool mb_assist_test(struct y_mb *mb,struct y_mb_ci *c,char super,int n,int end)
{
	if(c->len<2 || !mb->zi || !super)
		return false;
	const char *s=y_mb_ci_string(c);
	uint32_t hz;
	if(end!=0)
		hz=l_gb_last_char(s);
	else
		hz=l_gb_to_char(y_mb_skip_display(s,-1));
	struct y_mb_zi *z=L_HASH_TABLE_LOOKUP_INT(mb->zi,hz);
	if(!z)
	{
		return false;
	}
	for(struct y_mb_code *p=L_CPTR(z->code);p;p=L_CPTR(p->next))
	{
		if(p->virt) continue;
		if(super==mb->map[(int)y_mb_code_n_key(mb,p,n)])
		{
			return true;
		}
	}
	return false;
}

int y_mb_is_assist_key(struct y_mb *mb,int key)
{
	int super=0;
	if((key&KEYM_MASK) || key>=128)
		return 0;
	if(mb->yong && !mb->ass_mb)
		super=mb->map[(int)key];
	else if(mb->ass_mb)
		super=mb->ass_mb->map[(int)key];
	return super?1:0;
}

int y_mb_assist_test(struct y_mb *mb,struct y_mb_ci *c,char super,int n,int end)
{
	if(mb->yong && !mb->ass_mb)
	{
		super=mb->map[(int)super];
		return mb_assist_test(mb,c,super,2+n,end);
	}
	else
	{
		if(!mb->ass_mb) return 0;
		super=mb->ass_mb->map[(int)super];
		return mb_assist_test(mb->ass_mb,c,super,n,end);
	}
}

bool y_mb_assist_test_hz(struct y_mb *mb,uint32_t hz,char super[2])
{
	char super_code[2];
	int n;

	if(mb->yong && !mb->ass_mb)
	{
		super_code[0]=mb->map[(int)super[0]];
		super_code[1]=super[1]?mb->map[(int)super[1]]:0;
		n=2;
	}
	else
	{
		if(!mb->ass_mb) return 0;
		mb=mb->ass_mb;
		super_code[0]=mb->map[(int)super[0]];
		super_code[1]=super[1]?mb->map[(int)super[1]]:0;
		n=0;
	}

	struct y_mb_zi *z=L_HASH_TABLE_LOOKUP_INT(mb->zi,hz);
	if(!z)
	{
		return false;
	}
	for(struct y_mb_code *p=L_CPTR(z->code);p;p=L_CPTR(p->next))
	{
		if(p->virt) continue;
		if(super_code[0]==mb->map[(int)y_mb_code_n_key(mb,p,n)])
		{
			if(!super_code[1] || super_code[1]==mb->map[(int)y_mb_code_n_key(mb,p,n+1)])
				return true;
		}
	}
	return false;
}

int y_mb_assist_get(struct y_mb *mb,char calc[][MAX_CAND_LEN+1],int max,char super,int end)
{
	char temp[2]={super,0};
	return y_mb_assist_get2(mb,calc,max,temp,end);
}

static int y_mb_assist_get_fuzzy(struct y_mb *mb,char calc[][MAX_CAND_LEN+1],int max,char super[2],int end)
{
	int first_match=0,second_match=0;
	int i;
	int count=0;
	int pos=0;
	for(i=0;i<mb->ctx.result_count;i++)
	{
		struct y_mb_ci *c;
		pos++;
		c=l_ptr_array_nth(mb->ctx.result_ci,i);
		if(y_mb_assist_test(mb,c,super[0],0,end))
		{
			if(!first_match)
				first_match=pos;
			if(super[1])
			{
				if(!y_mb_assist_test(mb,c,super[1],1,end))
					continue;
				if(!second_match)
					second_match=pos;
			}
			strcpy(calc[count],y_mb_ci_string(c));
			count++;
			if(count==max)
				break;
		}
	}
	if(count<=1)
		return count;
	if(first_match==1)
	{
		char temp[MAX_CAND_LEN+1];
		strcpy(temp,calc[0]);
		memmove(calc,calc+1,(count-1)*(MAX_CAND_LEN+1));
		strcpy(calc[count-1],temp);
	}
	if(super[1] && second_match==first_match)
	{
		char temp[MAX_CAND_LEN+1];
		strcpy(temp,calc[0]);
		memmove(calc,calc+1,(count-1)*(MAX_CAND_LEN+1));
		strcpy(calc[count-1],temp);
	}
	return count;
}

int y_mb_assist_get2(struct y_mb *mb,char calc[][MAX_CAND_LEN+1],int max,char super[2],int end)
{
	char *s;
	int len;
	int count=0;
	int first_match=0,second_match=0;
	struct y_mb_item *p;
	struct y_mb_context *ctx=&mb->ctx;
	uint8_t *key;

	if(!mb || !(mb->ass_mb || mb->yong) || !ctx->result_count)
		return 0;
	if(ctx->result_dummy==2 && ctx->result_ci!=NULL)
		return y_mb_assist_get_fuzzy(mb,calc,max,super,end);
	s=ctx->input;
	len=strlen(s);
	if(s[0]==mb->ass_lead)
		return 0;
	if(ctx->result_wildcard)
		return 0;
	mb_ci_index(mb,s,len,&key);
	for(p=ctx->result_first;p && count<max;p=L_CPTR(p->next))
	{
		struct y_mb_ci *c;
		int pos=0;
		if(mb->nsort && mb_key_cmp_direct2(key,p->code,Y_MB_KEY_SIZE))
		{
			continue;
		}
		for(c=L_CPTR(p->phrase);c;c=L_CPTR(c->next))
		{
			if(c->del) continue;
			if(end && c->zi) continue;
			if(ctx->result_filter_zi && !c->zi) continue;
			pos++;
			if(y_mb_assist_test(mb,c,super[0],0,end))
			{
				if(!first_match)
					first_match=pos;
				if(super[1])
				{
					if(!y_mb_assist_test(mb,c,super[1],1,end))
						continue;
					if(!second_match)
						second_match=pos;
				}
				strcpy(calc[count],y_mb_ci_string(c));
				count++;
				if(count==max)
					break;
			}
		}
		if(!mb->nsort) break;
	}
	if(count<=1)
		return count;
	if(first_match==1)
	{
		char temp[MAX_CAND_LEN+1];
		strcpy(temp,calc[0]);
		memmove(calc,calc+1,(count-1)*(MAX_CAND_LEN+1));
		strcpy(calc[count-1],temp);
	}
	if(super[1] && second_match==first_match)
	{
		char temp[MAX_CAND_LEN+1];
		strcpy(temp,calc[0]);
		memmove(calc,calc+1,(count-1)*(MAX_CAND_LEN+1));
		strcpy(calc[count-1],temp);
	}
	return count;
}

struct y_mb_ci *y_mb_check_assist(struct y_mb *mb,const char *s,int len,char super,int end)
{
	struct y_mb_index *index;
	struct y_mb_item *item;
	uint8_t *key;
	uint16_t index_val;
	int ret;
	
	if(!mb->ass_mb)
		return NULL;
	
	super=mb->ass_mb->map[(int)super];
	
	index_val=mb_ci_index(mb,s,len,&key);
	for(index=mb->index;index;index=L_CPTR(index->next))
	{
		ret=mb_index_cmp_direct(index_val,index->index,2);
		if(ret<0)
			break;
		if(ret!=0)
			continue;

		for(item=L_CPTR(index->item);item;item=L_CPTR(item->next))
		{
			struct y_mb_ci *c;
			ret=mb_key_cmp_direct2(key,item->code,Y_MB_KEY_SIZE);
			if(ret>0) continue;
			if(ret<0) break;
			for(c=L_CPTR(item->phrase);c;c=L_CPTR(c->next))
			{
				if(c->del) continue;
				if(!c->zi) continue;
				if(mb_assist_test(mb->ass_mb,c,super,0,end))
				{
					return c;
				}
			}
			return NULL;
		}
	}

	return NULL;
}

void y_mb_push_context(struct y_mb *mb,struct y_mb_context *ctx)
{
	memcpy(ctx,&mb->ctx,sizeof(*ctx));
	mb->ctx.result_ci=NULL;
}

void y_mb_pop_context(struct y_mb *mb,struct y_mb_context *ctx)
{
	if(mb->ctx.result_ci) {
		l_ptr_array_free(mb->ctx.result_ci,NULL);
	}
	memcpy(&mb->ctx,ctx,sizeof(*ctx));
}

