#pragma once

#include "ltypes.h"

#define l_read_u8(p) *(uint8_t*)(p)
#define l_write_u8(p,v) (*(uint8_t*)(p)=(v))

#if L_ALIGNED_ACCESS
static inline uint16_t l_read_u16(const void *p)
{
#if L_BYTE_ORDER==L_LITTLE_ENDIAN
	return ((uint8_t*)p)[0]|(((uint8_t*)p)[1]<<8);
#else
	return ((uint8_t*)p)[1]|(((uint8_t*)p)[0]<<8);
#endif
}
static inline uint32_t l_read_u32(const void *p)
{
#if L_BYTE_ORDER==L_LITTLE_ENDIAN
	return ((uint8_t*)p)[0]|(((uint8_t*)p)[1]<<8)|
			(((uint8_t*)p)[2]<<16)|(((uint8_t*)p)[3]<<24);
#else
	return ((uint8_t*)p)[4]|(((uint8_t*)p)[2]<<8)|
			(((uint8_t*)p)[1]<<16)|(((uint8_t*)p)[0]<<24);
#endif
}
static inline void l_write_u16(void *p,uint16_t v)
{
#if L_BYTE_ORDER==L_LITTLE_ENDIAN
	((uint8_t*)p)[0]=v;
	((uint8_t*)p)[1]=v>>8;
#else
	((uint8_t*)p)[0]=v>>8;
	((uint8_t*)p)[1]=v;
#endif
}

static inline void l_write_u32(void *p,uint32_t v)
{
#if L_BYTE_ORDER==L_LITTLE_ENDIAN
	((uint8_t*)p)[0]=v;
	((uint8_t*)p)[1]=v>>8;
	((uint8_t*)p)[2]=v>>8;
	((uint8_t*)p)[3]=v>>8;
#else
	((uint8_t*)p)[0]=v>>24;
	((uint8_t*)p)[1]=v>>16;
	((uint8_t*)p)[1]=v>>8;
	((uint8_t*)p)[1]=v;
#endif
}

#else
#define l_read_u16(p) *(uint16_t*)(p)
#define l_read_u32(p) *(uint32_t*)(p)
#define l_write_u16(p,v) (*(uint16_t*)(p)=(v))
#define l_write_u32(p,v) (*(uint32_t*)(p)=(v))
#endif

#if L_BYTE_ORDER==L_LITTLE_ENDIAN

#define l_read_u16le(p) l_read_u16(p)
#define l_read_u32le(p) l_read_u32(p)
#define l_write_u16le(p,v) l_write_u16(p,v)
#define l_write_u32le(p,v) l_write_u32(p,v)

static inline uint16_t l_read_u16be(const void *p)
{
	return (((uint8_t*)p)[0]<<8)|((uint8_t*)p)[1];
}
static inline uint32_t l_read_u32be(const void *p)
{
	return (((uint8_t*)p)[0]<<24)|(((uint8_t*)p)[1]<<16)|
			(((uint8_t*)p)[2]<<8)|((uint8_t*)p)[3];
}

static inline void l_write_u16be(void *p,uint16_t v)
{
	((uint8_t*)p)[0]=v>>8;
	((uint8_t*)p)[1]=v;
}

static inline void l_write_u32be(void *p,uint32_t v)
{
	((uint8_t*)p)[0]=v>>24;
	((uint8_t*)p)[1]=v>>16;
	((uint8_t*)p)[2]=v>>8;
	((uint8_t*)p)[3]=v;
}

#else

#define l_read_u16be(p) l_read_u16(p)
#define l_read_u32be(p) l_read_u32(p)
#define l_write_u16be(p) l_write_u16(p)
#define l_write_u32be(p) l_write_u32(p)

static inline uint16_t l_read_u16le(const void *p)
{
	return (((uint8_t*)p)[0]<<8)|((uint8_t*)p)[1];
}
static inline uint32_t l_read_u32le(const void *p)
{
	return ((uint8_t*)p)[0]|(((uint8_t*)p)[1]<<8)|
			(((uint8_t*)p)[2]<<16)|(((uint8_t*)p)[3]<<24);
}

static inline void l_write_u16le(void *p,uint16_t v)
{
	((uint8_t*)p)[0]=v;
	((uint8_t*)p)[1]=v>>8;
}

static inline void l_write_u32le(void *p,uint16_t v)
{
	((uint8_t*)p)[0]=v;
	((uint8_t*)p)[1]=v>>8;
	((uint8_t*)p)[2]=v>>16;
	((uint8_t*)p)[3]=v>>24;
}

#endif

#define l_flip_bits(v,mask)	((v)^(mask))
#define l_flip_bit(v,n) ((v)^(1<<(n)))

#define l_bswap16(v) __builtin_bswap16(v)
#define l_bswap32(v) __builtin_bswap32(v)
#define l_bswap64(v) __builtin_bswap64(v)

