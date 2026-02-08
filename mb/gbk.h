/*!
 * \author dgod
 */
#ifndef _GBK_H_
#define _GBK_H_

#include <stdint.h>
#include "lbits.h"
#include "lmem.h"
#include "lgb.h"

#if 0
#define GBK_BEGIN	0x8140
#define GBK_END		0xFEFE
#define GBK_WIDTH	(254-64+1)
#define GBK_HEIGHT	(254-129+1)
#define GBK_SIZE	(GBK_WIDTH*GBK_HEIGHT)
#define GBK_OFFSET(a)	((((a)>>8)-0x81)*GBK_WIDTH+(((a)&0xff)-0x40))
#define GBK_CODE(a)	((((a)/GBK_WIDTH+0x81)<<8)+((a)%GBK_WIDTH)+0x40)

#define GB2312_BEGIN	0xA1A1
#define GB2312_END	0xF7FE
#define GB2312_SIZE	(GB2312_END-GB2312_BEGIN+1)
#define GB2312_SECTION_SIZE (94)

#define GB_HZ_SIZE ((0xF7-0xB0+1)*GB2312_SECTION_SIZE)
#define GB_HZ_OFFSET(a) ((((uint8_t*)(a))[0]-0xb0)*GB2312_SECTION_SIZE+((uint8_t*)(a))[1]-0xa1)

#define GB2312_IS_SYMBOL(a) ((a)>=GB2312_BEGIN && (a)<=0xA9FE && ((a)&0xff)>=0xA1)
#define GB2312_IS_HZ1(a) ((a)>=0xB0A1 && (a)<=0xD7FE && ((a)&0xff)>=0xA1)
#define GB2312_IS_HZ2(a) ((a)>=0xD8A1 && (a)<=0xF7FE && ((a)&0xff)>=0xA1)
#define GB2312_IS_HZ(a) ((a)>=0xB0A1 && (a)<=0xF7FE && ((a)&0xff)>=0xA1)
#define GB2312_IS_BIAODIAN(a) ((a)>=0xA1A1 && (a)<=0xA1FE)

#define GB2312_IS_VALID(a) ((a)>=GB2312_BEGIN && (a)<=GB2312_END && ((a)&0xff)>=0xA1)
#define GBK_IS_VALID(a)	((a)>=GBK_BEGIN && (a)<=GBK_END && ((a)&0xff)>=0x40)
#define GBK_MAKE_CODE(a,b) (((uint8_t)a)<<8|(((uint8_t)b)))
#define GBK_MAKE_STRING(a,b) (((b)[0]=(char)((a)>>8)),\
				((b)[1]=(char)((a)&0xff)),\
				((b)[2]=0))

#define GBK_IS_ASCII(a) ((a) && !((a)&0x80))
#endif

static inline int gb_strbrk(const uint8_t *s)
{
	int len=0;
	
	while(s[0])
	{
		if(!(s[0]&0x80))
		{
			break;
		}
		else if(gb_is_gbk(s))
		{
			len+=2;
			s+=2;
		}
		else if(gb_is_gb18030_ext(s))
		{
			len+=4;
			s+=4;
		}
		else
		{
			break;
		}
	}
	return len;
}

#define GB_NORMAL_EXT_COUNT		64
extern uint32_t gb_normal_map[0xFE - 0x81+1][(0xFE - 0x40+1+31)>>5];
extern uint32_t gb_normal_ext[GB_NORMAL_EXT_COUNT];
extern uint32_t gb_normal_ext_count;

int l_int_equal(const void *v1,const void *v2);

#ifdef GB_LOAD_NORMAL

uint32_t gb_normal_map[0xFE - 0x81+1][(0xFE - 0x40+1+31)>>5];
uint32_t gb_normal_ext[GB_NORMAL_EXT_COUNT];
uint32_t gb_normal_ext_count;

static int gb_load_normal(FILE *fp)
{
	int i,j,t;

	/* clear it first */
	memset(gb_normal_map,0,sizeof(gb_normal_map));
	
	/* mark gb2312 sysmbol as normal */
	for(i=0xa1;i<=0xa9;i++)
	for(j=0xa1;j<=0xfe;j++)
	{
		t=j-0x40;
		gb_normal_map[i-0x81][t>>5]|=1<<(t&0x1f);
	}
	/* section 0xAA-0xAF 0xF8-0XFE is user defined, don't mark it for less code */
	if(!fp) /* mark gb2312 hz as normal default */
	{
		for(i=0xb0;i<=0xf7;i++)
		for(j=0xa1;j<=0xfe;j++)
		{
			t=j-0x40;
			gb_normal_map[i-0x81][t>>5]|=1<<(t&0x1f);
		}
	}
	else /* load normal from file */
	{
		L_ALIGN(uint8_t s[4],8);
		while(1)
		{
			t=fread(s,1,1,fp);
			if(t==0) break;
			if(s[0]<0x81 || s[0]==0xff)
				continue;
			t=fread(s+1,1,1,fp);
			if(t==0) break;
			if(s[1]>=0x40 && s[1]!=0x7f && s[1]!=0xff)
			{
				t=s[1]-0x40;
				gb_normal_map[s[0]-0x81][t>>5]|=1<<(t&0x1f);
				continue;
			}
			t=fread(s+2,1,1,fp);
			if(t==0) break;
			if(s[2]<0x81 || s[2]==0xff)
				continue;
			t=fread(s+3,1,1,fp);
			if(t==0) break;
			if(s[3]>=0x30 && s[3]<=0x39)
			{
				if(gb_normal_ext_count<GB_NORMAL_EXT_COUNT)
				{
					gb_normal_ext[gb_normal_ext_count++]=*(uint32_t*)s;
				}
			}
		}
		qsort(gb_normal_ext,gb_normal_ext_count,sizeof(uint32_t),l_int_equal);
	}
	return 0;
}

#endif

static inline int gb_is_normal(const void *p)
{
	const uint8_t *s=p;
	if(!gb_is_gbk(s))
	{
		if(gb_is_gb18030_ext(s))
		{
			uint32_t t=*(uint32_t*)s;
			return bsearch(&t,gb_normal_ext,gb_normal_ext_count,sizeof(uint32_t),l_int_equal)?1:0;
		}
		return 0;
	}
	return (gb_normal_map[s[0]-0x81][(s[1]-0x40)>>5]>>(s[1]&0x1f)) & 0x01;
}

#endif/*_GBK_H_*/
