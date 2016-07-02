#include "yong.h"
#include "llib.h"
#include "gbk.h"

#include <sys/stat.h>

typedef struct{
	char *file;
	uint8_t *data;
	uint8_t src[32];
	uint32_t index[0x8000];
	size_t size;
	int dirty;
}LEGEND;

extern EXTRA_IM EIM;

void *y_legend_new(const char *file,int save)
{
	LEGEND *p;
	FILE *fp;
	struct stat st;
	const uint8_t *s;
	int prev=0;
	
	if(!EIM.OpenFile)
		return 0;
	fp=EIM.OpenFile(file,"rb");
	if(!fp) return NULL;
	fstat(fileno(fp),&st);
	if(st.st_size > 0x800000 || st.st_size<2)
	{
		fclose(fp);
		return NULL;
	}
	p=l_new0(LEGEND);
	if(save)
		p->file=l_strdup(file);
	p->size=st.st_size;
	p->data=l_alloc(8+st.st_size+1);
	memset(p->data,0,8);
	fread(p->data+8,st.st_size,1,fp);
	p->data[8+st.st_size]=0;
	fclose(fp);

	s=p->data+8;
	while(*s!=0)
	{
		if((s[0] & 0x80) && (s[1] >=0x40))
		{
			int pos=(((s[0]<<8)|s[1])&0x7fff);
			if(prev==pos)
			{
				goto next_line;
			}
			p->index[pos]=(uint32_t)(s-p->data);
			prev=pos;
		}
next_line:
		while(*s && *s!='\n') s++;
		if(*s=='\n') s++;
	}
	return p;
}

static int y_legend_save(LEGEND *p)
{
	FILE *fp;
	if(!p || !p->dirty || !p->file)
		return 0;
	fp=EIM.OpenFile(p->file,"wb");
	if(!fp) return -1;
	fwrite(p->data+8,p->size,1,fp);
	fclose(fp);
	p->dirty=0;
	return 0;
}

void y_legend_free(void *handle)
{
	LEGEND *p=handle;
	if(!p) return;
	y_legend_save(p);
	l_free(p->data);
	l_free(p->file);
	l_free(p);
}

void y_legend_reset(void *handle)
{
	LEGEND *p=handle;
	if(!p) return;
	p->src[0]=0;
}

int y_legend_get(void *handle,const char *src,int slen,
                int dlen,char calc[][MAX_CAND_LEN+1],int max)
{
	LEGEND *p=handle;
	int count=0;
	uint32_t pos;
	const uint8_t *s=(const uint8_t *)src;
	const uint8_t *end;
	
	p->src[0]=0;
	if(slen<2 || !gb_is_gbk((uint8_t*)src) || max<1)
		return 0;
	dlen=dlen*2-slen;
	pos=p->index[(((s[0]<<8)|s[1])&0x7fff)];
	if(pos<8) return 0;
	s=p->data+pos+2;
	end=(uint8_t*)strpbrk((const char*)s,"\r\n");
	src+=2;slen-=2;

	while(s && (!end || s<end))
	{
		if(slen==0 || !memcmp(s,src,slen))
		{
			char *to=calc[count];
			const char *from=(const char*)s+slen;
			int i,c;
			if(from[0]==',') from++;
			for(i=0;(c=from[i])!=0 && i<MAX_CAND_LEN+1;i++)
			{
				if(c==' ' || c=='\r' ||c=='\n')
					break;
				if(c==',') goto skip;
				to[i]=c;
			}
			to[i]=0;
			if(i>0 && i>dlen)
			{
				count++;
				if(count>=max) break;
			}
skip:;
			s=(const uint8_t*)from+i;
		}
		while(*s=='\r' || *s==',' || *s>=0x80) s++;
		if(*s=='\n')
		{
			s++;
			if(!memcmp(s,src-2,2))
			{
				s+=2;
				end=(uint8_t*)strpbrk((const char*)s,"\r\n");
				continue;
			}
			else
				break;
		}
			
		s=(const uint8_t*)strchr((const char*)s,' ');
		if(!s)
			break;
		s++;
	}
	if(count>0 && slen+2<sizeof(p->src))
	{
		src-=2;slen+=2;
		memcpy(p->src,src,slen);
		p->src[slen]=0;
	}
	return count;
}

void y_legend_move(void *handle,const char *phrase)
{
	LEGEND *p=handle;
	uint32_t pos;
	char temp[512];
	const uint8_t *s=p->src;
	const uint8_t *end;
	size_t tlen;
	
	if(!s[0] || !s[1])
		return;
	pos=p->index[(((s[0]<<8)|s[1])&0x7fff)];
	if(pos<8) return;
	s=p->data+pos+2;
	end=(uint8_t*)strpbrk((const char*)s,"\r\n");
	tlen=sprintf(temp,"%s%s",p->src+2,phrase);
	temp[tlen]=' ';

	while(s && (!end || s<end))
	{
		if(!memcmp(s,temp,tlen) && s[tlen]<=0x20)
		{
			if(p->data+pos+2==s)
				break;
			memmove(p->data+pos+2+tlen+1,p->data+pos+2,s-p->data-pos-2-1);
			memcpy(p->data+pos+2,temp,tlen+1);
			p->dirty++;
			break;
		}
		s=(const uint8_t*)strchr((const char*)s,' ');
		if(!s)
			break;
		s++;
	}
}
