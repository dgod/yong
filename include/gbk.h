/*!
 * \author dgod
 */
#ifndef _GBK_H_
#define _GBK_H_

#include <stdint.h>

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

static inline int gb_is_ascii(const uint8_t *s)
{
	return s[0] && !(s[0]&0x80);
}

static inline int gb_is_gb2312(const uint8_t *s)
{
	return s[0]<=0xFE && s[0]>=0xA1 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline int gb_is_hz(const uint8_t *s)
{
	return s[0]<=0xF7 && s[0]>=0xB0 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline int gb_is_hz1(const uint8_t *s)
{
	return s[0]<=0xD7 && s[0]>=0xB0 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline int gb_is_hz2(const uint8_t *s)
{
	return s[0]<=0xF7 && s[0]>=0xD8 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline int gb_is_biaodian(const uint8_t *s)
{
	return s[0]>=0xA1 && s[0]<=0xA3 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline int gb_is_gbk_ext(const uint8_t *s)
{
	return (s[0]<=0xFE && s[0]>=0x81 && s[1]<=0xFE && s[1]>=0x40 && s[1]!=0x7F);
}

static inline int gb_is_gbk(const uint8_t *s)
{
	return gb_is_gb2312(s) || gb_is_gbk_ext(s);
}

static inline int gb_is_gb18030_ext(const uint8_t *s)
{
	return s[0]<=0xFE && s[0]>=0x81 && s[1]<=0x39 && s[1]>=0x30 &&
		s[2]<=0xFE && s[2]>=0x81 && s[3]<=0x39 && s[3]>=0x30;
}

static inline int gb_is_gb18030(const uint8_t *s)
{
	return gb_is_gbk(s) || gb_is_gb18030_ext(s);
}

static inline int gb_strlen(const uint8_t *s)
{
	int len=0;
	
	while(s[0])
	{
		if(!(s[0]&0x80))
		{
			len++;
			s++;
			continue;
		}
		else if(gb_is_gbk(s))
		{
			len++;
			s+=2;
		}
		else if(gb_is_gb18030_ext(s))
		{
			len++;
			s+=4;
		}
		else
		{
			len++;
			s++;
		}
	}
	return len;
}

static inline int gb_strlen2(const uint8_t *s,int size)
{
	int len=0;
	const uint8_t *end=s+size;
	while(s<end)
	{
		if(!(s[0]&0x80))
		{
			len++;
			s++;
			continue;
		}
		else if(gb_is_gbk(s))
		{
			len++;
			s+=2;
		}
		else if(gb_is_gb18030_ext(s))
		{
			len++;
			s+=4;
		}
		else
		{
			len++;
			s++;
		}
	}
	return len;
}

static inline void *gb_offset(const uint8_t *s,int offset)
{
	while(s[0] && offset>0)
	{
		if(!(s[0]&0x80))
			s++;
		else if(gb_is_gbk(s))
			s+=2;
		else if(gb_is_gb18030_ext(s))
			s+=4;
		else
			return NULL;
		offset--;
	}
	if(offset!=0)
		return NULL;
	return (void*)s;
}

static inline uint32_t gb_first(const uint8_t *s)
{
	uint32_t r;
	
	if(!(s[0]&0x80))
	{
		r=s[0];
	}
	else if(gb_is_gbk(s))
	{
#if defined(__arm__) || defined(EMSCRIPTEN)
		r=*(uint16_t*)(s);
#else
		r=s[0]|(s[1]<<8);
#endif
	}
	else if(gb_is_gb18030_ext(s))
	{
#if defined(__arm__) || defined(EMSCRIPTEN)
		r=*(uint32_t*)(s);
#else
		r=s[0]|(s[1]<<8)|(s[2]<<16)|(s[3]<<24);
#endif
	}
	else
	{
		r=0;
	}
	return r;
}

static inline uint32_t gb_first_be(const uint8_t *s)
{
	uint32_t r;
	
	if(!(s[0]&0x80))
	{
		r=s[0];
	}
	else if(gb_is_gbk(s))
	{
		r=s[1]|(s[0]<<8);
	}
	else if(gb_is_gb18030_ext(s))
	{
		r=s[3]|(s[2]<<8)|(s[1]<<16)|(s[0]<<24);
	}
	else
	{
		r=0;
	}
	return r;
}

static inline uint32_t gb_last(const uint8_t *s)
{
	uint32_t r=0;
	
	while(s[0])
	{
		if(!(s[0]&0x80))
		{
			r=s[0];
			s++;
		}
		else if(gb_is_gbk(s))
		{
	#if defined(__arm__) || defined(EMSCRIPTEN)
			r=*(uint16_t*)(s);
	#else
			r=s[0]|(s[1]<<8);
	#endif
			s+=2;
		}
		else if(gb_is_gb18030_ext(s))
		{
	#if defined(__arm__) || defined(EMSCRIPTEN)
			r=*(uint32_t*)(s);
	#else
			r=s[0]|(s[1]<<8)|(s[2]<<16)|(s[3]<<24);
	#endif
			s+=4;
		}
		else
		{
			r=0;
			break;
		}
	}
	return r;
}

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

static inline char *gb_strchr(const uint8_t *s,int c)
{
	while(s[0])
	{
		if(!(s[0]&0x80))
		{
			if(s[0]==c)
				return (char*)s;
			s++;
		}
		else if(gb_is_gbk(s))
		{
			s+=2;
		}
		else if(gb_is_gb18030_ext(s))
		{
			s+=4;
		}
		else
		{
			break;
		}
	}
	return 0;
}

#define GB_NORMAL_EXT_COUNT		8
extern uint32_t gb_normal_map[0xFE - 0x81+1][(0xFE - 0x40+1+31)>>5];
extern uint32_t gb_normal_ext[GB_NORMAL_EXT_COUNT];

#ifdef GB_LOAD_NORMAL

uint32_t gb_normal_map[0xFE - 0x81+1][(0xFE - 0x40+1+31)>>5];
uint32_t gb_normal_ext[GB_NORMAL_EXT_COUNT];

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
		uint8_t s[4];
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
				for(i=0;i<GB_NORMAL_EXT_COUNT;i++)
				{
					if(!gb_normal_ext[i])
					{
						memcpy(gb_normal_ext+i,s,4);
						break;
					}
				}
			}
		}
	}
	return 0;
}

#endif

static inline int gb_is_normal(const uint8_t *s)
{
	if(!gb_is_gbk(s))
	{
		if(gb_is_gb18030_ext(s))
		{
			int i;
			for(i=0;i<GB_NORMAL_EXT_COUNT && gb_normal_ext[i];i++)
				if(!memcmp(s,&gb_normal_ext[i],4))
					return 1;
		}
		return 0;
	}
	return (gb_normal_map[s[0]-0x81][(s[1]-0x40)>>5]>>(s[1]&0x1f)) & 0x01;
}

#endif/*_GBK_H_*/
