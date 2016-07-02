//http://www.gzip.org/zlib/rfc-gzip.html

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ltricky.h"
#include "lzlib.h"

#define GZ_ID1	0x1F
#define GZ_ID2	0x8B
	
#define GZ_CM_DEFLATE	0x08
	
#define FTEXT		0x01
#define FHCRC		0x02
#define FEXTRA		0x04
#define FNAME		0x08
#define FCOMMENT	0x10

typedef struct{
	uint8_t ID1;
	uint8_t ID2;
	uint8_t CM;
	uint8_t FLG;
	uint32_t MTIME;
	uint8_t XFL;
	uint8_t OS;
}GZ_HDR;
#define GZ_HDR_LEN		10

void *gz_extract(const void *input,int len,int *olen)
{
	const GZ_HDR *h;
	const uint8_t *p;
	if(len<GZ_HDR_LEN+8)
	{
		return NULL;
	}
	h=(GZ_HDR*)input;
	if(h->ID1!=GZ_ID1 || h->ID2!=GZ_ID2 || h->CM!=GZ_CM_DEFLATE)
	{
		return NULL;
	}
	p=(uint8_t*)h+GZ_HDR_LEN;len-=GZ_HDR_LEN;
	if((h->FLG&FEXTRA)!=0)
	{
		if(len<2)
		{
			return NULL;
		}
		int xlen=p[0]|(p[1]<<8);
		p+=2;len-=2;
		if(len<xlen)
		{
			return NULL;
		}
		p+=xlen;len-=xlen;
	}
	if((h->FLG&FNAME)!=0)
	{
		for(;len>0&&p[0];len--,p++);
		if(len==0)
		{
			return NULL;
		}
		len--;p++;
	}
	if((h->FLG&FCOMMENT)!=0)
	{
		for(;len>0&&p[0];len--,p++);
		if(len==0)
		{
			return NULL;
		}
		len--;p++;
	}
	if((h->FLG&FHCRC)!=0)
	{
		len-=2;p+=2;
	}
	if(len<8)
	{
		return NULL;
	}
	const void *zdata=p;
	int zlen=len-8;
	p+=zlen;len=zlen;
	// CRC32
	p+=4;len-=4;
	// ISIZE
	int isize=(int)(p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24));
	void *res=malloc(isize);
	if(olen) *olen=isize;
	if(isize!=l_zlib_decode(res,isize,zdata,zlen,0))
	{
		free(res);
		return NULL;
	}
	return res;
}

#if 0

#include <stdio.h>
#include <sys/stat.h>
void *read_file(const char *path,int *len)
{
	FILE *fp;
	struct stat st;
	uint8_t *res;
	fp=fopen(path,"rb");
	if(!fp) return NULL;
	fstat(fileno(fp),&st);
	res=malloc(st.st_size);
	if(len) *len=(int)st.st_size;
	fread(res,1,st.st_size,fp);
	fclose(fp);
	return res;
}

void write_file(const char *path,const void *data,int len)
{
	FILE *fp;
	fp=fopen(path,"wb");
	if(!fp) return;
	fwrite(data,1,len,fp);
	fclose(fp);
}

int main(void)
{
	void *data,*odata;
	int len,olen;
	data=read_file("test.gz",&len);
	if(!data)
		return -1;
	odata=gz_extract(data,len,&olen);
	free(data);
	if(!odata)
	{
		printf("gz_extract fail\n");
		return -1;
	}
	write_file("test.zlib",odata,olen);
	free(odata);
	return 0;
}
#endif
