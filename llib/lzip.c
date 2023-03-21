#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ltypes.h"
#include "lmem.h"
#include "lzlib.h"
#include "ltricky.h"

#define LOCAL_FILE_SIGNATURE	0x04034b50
#define CENTRAL_FILE_SIGNATURE	0x02014b50
#define DIGITAL_SIGNATURE		0x05054b50
#define DATA_DESC_SIGNATURE		0x08074b50
#define EXTRA_DATA_SIGNATURE	0x08064b50
#define END_OF_FILE_SIGNATURE	0x06054b50

#define FLAG_USE_DATA_DESC			(1<<3)
#define FLAG_UTF8_FILE_NAME			(1<<11)

enum{
	COMP_Store=0,
	COMP_Deflated=8,
	COMP_Deflate64=9,
	COMP_BZIP2=12,
	COMP_LZMA=14,
	COMP_PPMd=98,
};

typedef struct{
	uint32_t signature;
	uint16_t version;
	uint16_t flag;
	uint16_t comp;
	uint16_t mtime;
	uint16_t mdate;
	uint32_t crc;
	uint32_t size;
	uint32_t usize;
	uint16_t name;
	uint16_t extra;
}LZipLocalFile;

typedef struct{
	uint32_t signature;			/* this is optional */
	uint32_t crc;
	uint32_t size;
	uint32_t usize;
}LZipDataDesc;

typedef struct{
	uint32_t signature;
	uint32_t size;
}LZipExtraData;

typedef struct{
	uint32_t signature;
	uint16_t v_made;
	uint16_t v_extract;
	uint16_t flag;
	uint16_t comp;
	uint16_t mtime;
	uint16_t mdate;
	uint16_t name;
	uint16_t extra;
	uint16_t comment;
	uint16_t disk;
	uint16_t i_attr;
	uint32_t e_attr;
	uint32_t offset;
}LZipCentralFile;

typedef struct{
	uint32_t signature;
	uint16_t size;
}LZipDigitalSignature;

typedef struct{
	uint32_t signature;
	uint16_t disk_num;		// Number of this disk
	uint16_t disk_first;	// Disk where central directory starts
	uint16_t central_this;	// Number of central directory records on this disk
	uint16_t central_total;	// Total number of central directory records
	uint32_t size;			// Size of central directory (bytes)
	uint32_t offset;		// Offset of start of central directory, relative to start of archive
	uint16_t comment;		// Comment length (n)
}LZipEndOfFile;

#define r16(x) if(2!=fread(&x,1,2,fp)) goto out;
#define r32(x) if(4!=fread(&x,1,4,fp)) goto out;

#if defined(_WIN32)
#define skip(l) do{ \
	if(!(l)) break; \
	if((l)>256) { \
		if(fseek(fp,l,SEEK_CUR)!=0) goto out;\
	} else { \
		char temp[256];\
		if((l)!=fread(temp,1,(l),fp)) goto out; \
	} \
}while(0)
#else
#define skip(l) if(l && fseek(fp,l,SEEK_CUR)!=0) goto out;
#endif

static int l_zip_get_end_of_file(FILE *fp,LZipEndOfFile *e)
{
	// if there are any file, zip is at least 100 bytes
	// here we not support comment len>100-22
	uint8_t temp[100];
	int i,ret;
	if(0!=fseek(fp,(int)-sizeof(temp),SEEK_END))
		return -1;
	ret=fread(temp,1,sizeof(temp),fp);
	if(ret<22)
		return -1;
	for(i=0;i<=ret-22;i++)
	{
		uint32_t sig=temp[i]|(temp[i+1]<<8)|(temp[i+2]<<16)|(temp[i+3]<<24);
		if(sig==END_OF_FILE_SIGNATURE)
		{
			memcpy(e,temp+i,22);
			return 0;
		}
	}
	return -1;
}

static int l_zip_get_central_file(FILE *fp,const LZipEndOfFile *e,const char *name,LZipCentralFile *c)
{
	int i;
	size_t name_len=strlen(name);

	if(e->central_total==0)
		return -1;
	if(0!=fseek(fp,e->offset,SEEK_SET))
		return -1;
	for(i=0;/*i<e->central_total*/;i++)
	{
		r32(c->signature);
		if(c->signature!=CENTRAL_FILE_SIGNATURE)
		{
			goto out;
		}
		r16(c->v_made);
		r16(c->v_extract);
		r16(c->flag);
		r16(c->comp);
		r16(c->mtime);
		r16(c->mdate);
		skip(12);		// crc,size,usize
		r16(c->name);
		r16(c->extra);
		r16(c->comment);
		r16(c->disk);
		r16(c->i_attr);
		r32(c->e_attr);
		r32(c->offset);
		if(c->name==name_len)
		{
			char entry[name_len+1];
			if(name_len!=fread(entry,1,name_len,fp))
				goto out;
			entry[name_len]=0;
			if(!strcmp(entry,name))
				return 0;
		}
		else
		{
			skip(c->name);
		}
		skip(c->extra);
		skip(c->comment);
	}
out:
	return -1;
}

static int l_zip_get_local_file(FILE *fp,const char *name,LZipLocalFile *h)
{
	size_t name_len=strlen(name);
	while(1)
	{
		r32(h->signature);
		if(h->signature!=LOCAL_FILE_SIGNATURE)
		{
			goto out;
		}
		r16(h->version);
		r16(h->flag);
		if((h->flag&FLAG_USE_DATA_DESC)!=0)
		{
			goto out;
		}
		r16(h->comp);
		r16(h->mtime);
		r16(h->mdate);
		r32(h->crc);
		r32(h->size);
		r32(h->usize);
		r16(h->name);
		r16(h->extra);
		if(h->name==name_len)
		{
			char entry[name_len+1];
			
			if(name_len!=fread(entry,1,name_len,fp))
				goto out;
			entry[name_len]=0;
			skip(h->extra);
			if(!strcmp(entry,name))
				return 0;
		}
		else
		{
			skip(h->name);
			skip(h->extra);
		}
		skip(h->size);
	}
out:
	return -1;
}

int l_zip_goto_file(FILE *fp,const char *name)
{
	LZipEndOfFile e;
	LZipCentralFile c;
	LZipLocalFile h;
	int ret;
	ret=l_zip_get_end_of_file(fp,&e);
	if(ret!=0) return -1;
	ret=l_zip_get_central_file(fp,&e,name,&c);
	if(ret!=0) return -1;
	ret=fseek(fp,c.offset,SEEK_SET);
	if(ret!=0) return -1;
	ret=l_zip_get_local_file(fp,name,&h);
	if(ret!=0)
		return -1;
	if(h.comp!=COMP_Store || h.size!=h.usize)
		return -1;
	return h.size;
}

bool l_zip_file_exists(FILE *fp,const char *name)
{
	LZipEndOfFile e;
	LZipCentralFile c;
	LZipLocalFile h;
	int ret;
	ret=l_zip_get_end_of_file(fp,&e);
	if(ret!=0) return false;
	ret=l_zip_get_central_file(fp,&e,name,&c);
	if(ret!=0) return false;
	ret=fseek(fp,c.offset,SEEK_SET);
	if(ret!=0) return false;
	ret=l_zip_get_local_file(fp,name,&h);
	if(ret!=0)
		return false;
	if(h.comp!=COMP_Store && h.comp!=COMP_Deflated)
		return false;
	return true;
}

char *l_zip_file_get_contents(FILE *fp,const char *name,size_t *length)
{
	LZipEndOfFile e;
	LZipCentralFile c;
	LZipLocalFile h;
	int ret;
	char *res=NULL;
	ret=l_zip_get_end_of_file(fp,&e);
	if(ret!=0)
	{
		return NULL;
	}
	ret=l_zip_get_central_file(fp,&e,name,&c);
	if(ret!=0) return NULL;
	ret=fseek(fp,c.offset,SEEK_SET);
	if(ret!=0) return NULL;
	ret=l_zip_get_local_file(fp,name,&h);
	if(ret==-1)
	{
		return NULL;
	}
	res=l_alloc((h.usize+1+15)&~0x0f);
	if(h.comp==COMP_Store)
	{		
		fread(res,1,h.size,fp);
		res[h.size]=0;
		if(length) *length=h.size;
	}
	else if(h.comp==COMP_Deflated)
	{
		uint8_t *temp=l_alloc(h.size);
		fread(temp,1,h.size,fp);
		ret=l_zlib_decode(res,h.usize,temp,h.size,0);
		l_free(temp);
		if(ret!=h.usize)
		{
			l_free(res);
			res=NULL;
		}
		else
		{
			res[h.usize]=0;
			if(length) *length=h.usize;
		}
	}
	return res;
}

#if 0
int main(int arc,char *arg[])
{
	FILE *fp;
	size_t len;
	
	if(arc!=3)
		return -1;
	
	fp=fopen(arg[1],"rb");
	if(!fp) return -1;
	//len=l_zip_goto_file(fp,arg[2]);
	//printf("offset %d size %d\n",(int)ftell(fp),len);
	int len;
	char *data;
	data=l_zip_file_get_contents(fp,arg[2],&len);
	fclose(fp);
	if(data!=NULL)
	{
		printf("%s\n",data);
		free(data);
	}
	return 0;
}
#endif
