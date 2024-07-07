#pragma once

#include "ltypes.h"

#define l_read_u8(p) *(uint8_t*)(p)

#if L_ALIGNED_ACCESS
static inline uint16_t l_read_u16(const void *p)
{
	return ((uint8_t*)p)[0]|(((uint8_t*)p)[1]<<8);
}
static inline uint32_t l_read_u32(const void *p)
{
	return ((uint8_t*)p)[0]|(((uint8_t*)p)[1]<<8)|
			(((uint8_t*)p)[2]<<16)|(((uint8_t*)p)[3]<<24);
}
#else
#define l_read_u16(p) *(uint16_t*)(p)
#define l_read_u32(p) *(uint32_t*)(p)
#endif

#if L_BYTE_ORDER==L_LITTLE_ENDIAN

#define l_read_u16le(p) l_read_u16(p)
#define l_read_u32le(p) l_read_u32(p)

static inline uint16_t l_read_u16be(const void *p)
{
	return (((uint8_t*)p)[0]<<8)|((uint8_t*)p)[1];
}
static inline uint32_t l_read_u32be(const void *p)
{
	return (((uint8_t*)p)[0]<<24)|(((uint8_t*)p)[1]<<16)|
			(((uint8_t*)p)[2]<<8)|((uint8_t*)p)[3];
}

#else

#define l_read_u16be(p) l_read_u16(p)
#define l_read_u32be(p) l_read_u32(p)

static inline uint16_t l_read_u16le(const void *p)
{
	return (((uint8_t*)p)[0]<<8)|((uint8_t*)p)[1];
}
static inline uint32_t l_read_u32le(const void *p)
{
	return ((uint8_t*)p)[0]|(((uint8_t*)p)[1]<<8)|
			(((uint8_t*)p)[2]<<16)|(((uint8_t*)p)[3]<<24);
}

#endif

