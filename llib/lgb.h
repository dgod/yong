#ifndef _LGB_H_
#define _LGB_H_

int l_unichar_to_gb(uint32_t c,uint8_t *outbuf);
int l_unichar_to_gb0(uint32_t c,void *outbuf);
uint32_t l_gb_to_unichar(const uint8_t *s);
void *l_gb_next_char(const void *p);
int l_gb_strlen(const void *p,int size);
void *l_gb_offset(const void *p,int offset);
uint32_t l_gb_to_char(const void *p);
uint32_t l_gb_last_char(const void *p);
int l_char_to_gb(uint32_t c,void *outbuf);
int l_char_to_gb0(uint32_t c,void *outbuf);
const void *l_gb_strchr(const void *p,uint32_t c);

static inline bool gb_is_ascii(const void *p)
{
	const uint8_t *s=p;
	return s[0] && !(s[0]&0x80);
}

static inline bool gb_is_gb2312(const void *p)
{
	const uint8_t *s=p;
	return s[0]<=0xF7 && s[0]>=0xA1 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline bool gb_is_gbk(const void *p)
{
	const uint8_t *s=p;
	return (s[0]<=0xFE && s[0]>=0x81 && s[1]<=0xFE && s[1]>=0x40 && s[1]!=0x7F);
}

static inline bool gb_is_gb18030_ext(const void *p)
{
	const uint8_t *s=p;
	return s[0]<=0xFE && s[0]>=0x81 && s[1]<=0x39 && s[1]>=0x30 &&
		s[2]<=0xFE && s[2]>=0x81 && s[3]<=0x39 && s[3]>=0x30;
}

static inline bool gb_is_hz(const void *p)
{
	const uint8_t *s=p;
	return s[0]<=0xF7 && s[0]>=0xB0 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline bool gb_is_hz1(const void *p)
{
	const uint8_t *s=p;
	return s[0]<=0xD7 && s[0]>=0xB0 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline bool gb_is_hz2(const void *p)
{
	const uint8_t *s=p;
	return s[0]<=0xF7 && s[0]>=0xD8 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline bool gb_is_biaodian(const void *p)
{
	const uint8_t *s=p;
	return s[0]>=0xA1 && s[0]<=0xA3 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline bool gb_is_symbol(const void *p)
{
	const uint8_t *s=p;
	return s[0]>=0xA1 && s[0]<=0xA9 && s[1]<=0xFE && s[1]>=0xA1;
}

static inline bool gb_is_gb18030(const void *p)
{
	const uint8_t *s=p;
	return gb_is_gbk(s) || gb_is_gb18030_ext(s);
}

#define GBK_BEGIN		0x8140
#define GBK_END			0xFEFE
#define GBK_WIDTH		(254-64+1)
#define GBK_HEIGHT		(254-129+1)
#define GBK_SIZE		(GBK_WIDTH*GBK_HEIGHT)
#define GBK_OFFSET(a)	((((a)>>8)-0x81)*GBK_WIDTH+(((a)&0xff)-0x40))
#define GBK_CODE(a)		((((a)/GBK_WIDTH+0x81)<<8)+((a)%GBK_WIDTH)+0x40)

#define GB2312_BEGIN	0xA1A1
#define GB2312_END		0xF7FE

#define GB2312_SECTION_SIZE (94)
#define GB2312_HZ_SIZE 		((0xF7-0xB0+1)*GB2312_SECTION_SIZE)
#define GB2312_HZ_OFFSET(a) ((((uint8_t*)(a))[0]-0xb0)*GB2312_SECTION_SIZE+((uint8_t*)(a))[1]-0xa1)

#endif/*_LGB_H_*/

