#pragma once

#define L_SDF_ALPHA(c)		((c)>>24)
#define L_SDF_RGB(r,g,b)	(0xFF000000|((r)<<16)|((g)<<8)|(b))
#define L_SDF_RGBA(r,g,b,a)	(((a)<24)|((r)<<16)|((g)<<8)|(b))

typedef struct{
	uint32_t *pixels;
	int width;
	int height;
	int stride;
}L_SDF_SURFACE;

typedef struct{
	float x1,y1,x2,y2;
}L_SDF_SHADOW;

enum{
	L_SDF_NEAREST=0,
	L_SDF_BILINEAR=1,
};

typedef struct{
	uint32_t *pixels;
	int width;
	int height;
	int stride;

	float line_width;
	uint32_t bg;
	uint32_t fg;
	int x;
	int y;

	L_SDF_SHADOW shadow;
}L_SDF_CONTEXT;

static inline uint32_t l_sdf_byte_mul(uint32_t c,uint32_t a)
{
	uint32_t rb = (c & 0x00FF00FF) * a;
	uint32_t ag = ((c>>8) & 0x00FF00FF) *a;
	rb=(rb+((rb>>8)&0x00FF00FF)+0x800080)&0xFF00FF00;
	ag=(ag+((ag>>8)&0x00FF00FF)+0x800080)&0xFF00FF00;
	return ag | rb>>8;
}

static inline uint32_t l_sdf_premultiply(uint32_t c)
{
	uint32_t a = L_SDF_ALPHA(c);
	uint32_t rb = (c & 0x00FF00FF) * a;
	uint32_t g  = (c & 0x0000FF00) * a;
	rb=(rb+((rb>>8)&0xFF00FF)+0x800080)&0xFF00FF00;
	g =(g +((g >>8)&0x00FF00)+0x008000)&0x00FF0000;
	return (rb >> 8) | (g>>8) | (a<<24);
}

static inline uint32_t l_sdf_premultiply_with(uint32_t c,uint32_t a)
{
	uint32_t rb = (c & 0x00FF00FF) * a;
	uint32_t g  = (c & 0x0000FF00) * a;
	rb = (rb + 0x00800080 + ((rb + 0x00800080) >> 8)) & 0xFF00FF00;
	g  = (g  + 0x00008000 + ((g  + 0x00008000) >> 8)) & 0x00FF0000;
	return (rb >> 8) | g | (a<<24);
}

int l_sdf_moveto(L_SDF_CONTEXT *ctx,int x,int y);
int l_sdf_lineto(L_SDF_CONTEXT *ctx,int x,int y);
int l_sdf_rect(L_SDF_CONTEXT *ctx,int w,int h,int r);
int l_sdf_rect_shadow(L_SDF_CONTEXT *ctx,int w,int h,int len,int r);
void l_sdf_context_init(L_SDF_CONTEXT *ctx,void *pixels,int w,int h);

static inline void l_sdf_set_fg(L_SDF_CONTEXT *ctx,uint32_t c)
{
	ctx->fg=c;
}

static inline void l_sdf_set_bg(L_SDF_CONTEXT *ctx,uint32_t c)
{
	ctx->bg=c;
}

static inline void l_sdf_set_line(L_SDF_CONTEXT *ctx,float line_width)
{
	ctx->line_width=line_width;
}

static inline void l_sdf_set_shadow(L_SDF_CONTEXT *ctx,float x1,float y1,float x2,float y2)
{
	ctx->shadow.x1=x1;
	ctx->shadow.y1=y1;
	ctx->shadow.x2=x2;
	ctx->shadow.y2=y2;
}

#ifdef __WIN32
#define L_USE_SDF_DRAW		0
#else
#define L_USE_SDF_DRAW		1
#endif

L_SDF_SURFACE *l_sdf_surface_new(int w, int h);
void l_sdf_surface_free(L_SDF_SURFACE *s);
L_SDF_SURFACE l_sdf_surface_slice(L_SDF_SURFACE *surface,int x,int y,int w,int h);
L_SDF_SURFACE *l_sdf_surface_copy(const L_SDF_SURFACE *surface);
L_SDF_SURFACE *l_sdf_surface_scale(const L_SDF_SURFACE *src, int w, int h, int mode);
void l_sdf_draw(L_SDF_CONTEXT *ctx, const L_SDF_SURFACE *s, int w, int h);
