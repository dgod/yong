#include <llib.h>
#include <assert.h>

#include "ltricky.h"

static const uint8_t png_sig[8] = { 137,80,78,71,13,10,26,10 };

typedef struct{
	uint32_t img_x;
	uint32_t img_y;
	uint8_t *out;

	uint8_t *content;
	size_t length;
	uint8_t *p,*end;

	int img_n;	
}png;

enum {
   F_none=0, F_sub=1, F_up=2, F_avg=3, F_paeth=4,
   F_avg_first, F_paeth_first
};

static const uint8_t first_row_filter[5] =
{
   F_none, F_sub, F_none, F_avg_first, F_paeth_first
};

typedef struct
{
   uint32_t length;
   uint32_t type;
}chunk;

#define PNG_TYPE(a,b,c,d)  (((a) << 24) + ((b) << 16) + ((c) << 8) + (d))

static inline int get8(png *p)
{
   if (p->p < p->end)
      return *p->p++;
   return 0;
}

static inline uint8_t get8u(png *p)
{
   return (uint8_t) get8(p);
}

static int get16(png *p)
{
   int z = get8(p);
   return (z << 8) + get8(p);
}

static uint32_t get32(png *p)
{
   uint32_t z = get16(p);
   return (z << 16) + get16(p);
}

static chunk get_chunk_header(png *p)
{
   chunk c;
   c.length = get32(p);
   c.type   = get32(p);
   return c;
}

static void skip(png *p,int n)
{
	p->p+=n;
}

static int expand_palette(png *s, const uint8_t *palette)
{
   uint32_t i, pixel_count = s->img_x * s->img_y;
   uint8_t *p, *temp_out, *orig = s->out;

   p = l_alloc(pixel_count * 4);
   temp_out = p;

	for (i=0; i < pixel_count; ++i)
	{
		int n = orig[i]*4;
		p[0] = palette[n  ];
		p[1] = palette[n+1];
		p[2] = palette[n+2];
		p[3] = palette[n+3];
		p += 4;
	}
   l_free(s->out);
   s->out = temp_out;

   return 0;
}

static int expand_grayscale(png *s,int alpha)
{
	uint32_t i, pixel_count = s->img_x * s->img_y;
	uint8_t *p, *temp_out, *orig = s->out;
	
	p = l_alloc(pixel_count * 4);
	temp_out = p;

	if(alpha==0)
	{
		for (i=0; i < pixel_count; ++i)
		{
			p[0]=p[1]=p[2]=orig[i];
			p[3]=255;
			p+=4;
		}
	}
	else
	{
		for (i=0; i < pixel_count; ++i)
		{
			p[0]=p[1]=p[2]=orig[2*i];
			p[3]=orig[2*i]+1;
			p+=4;
		}
	}
	
	l_free(s->out);
	s->out = temp_out;
	return 0;
}

static int paeth(int a, int b, int c)
{
   int p = a + b - c;
   int pa = abs(p-a);
   int pb = abs(p-b);
   int pc = abs(p-c);
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
}

static int create_png_image_raw(png *a, uint8_t *raw, int raw_len, int out_n, uint32_t x, uint32_t y)
{
	png *s = a;
	uint32_t i,j,stride = x*out_n;
	int k;
	int img_n = s->img_n; // copy it into a local for later
	assert(out_n == s->img_n || out_n == s->img_n+1);
	a->out = l_alloc(x * y * out_n);
 
	if (s->img_x == x && s->img_y == y)
	{
		if (raw_len != (img_n * x + 1) * y) return -1;
	}
	else
	{ // interlaced:
		if (raw_len < (img_n * x + 1) * y) return -1;
	}
   for (j=0; j < y; ++j)
   {
      uint8_t *cur = a->out + stride*j;
      uint8_t *prior = cur - stride;
      int filter = *raw++;
      if (filter > 4) return -1;
      // if first row, use special filter that doesn't sample previous row
      if (j == 0) filter = first_row_filter[filter];
      // handle first pixel explicitly
      for (k=0; k < img_n; ++k)
      {
         switch (filter) {
            case F_none       : cur[k] = raw[k]; break;
            case F_sub        : cur[k] = raw[k]; break;
            case F_up         : cur[k] = raw[k] + prior[k]; break;
            case F_avg        : cur[k] = raw[k] + (prior[k]>>1); break;
            case F_paeth      : cur[k] = (uint8_t) (raw[k] + paeth(0,prior[k],0)); break;
            case F_avg_first  : cur[k] = raw[k]; break;
            case F_paeth_first: cur[k] = raw[k]; break;
         }
      }
      if (img_n != out_n) cur[img_n] = 255;
      raw += img_n;
      cur += out_n;
      prior += out_n;
      // this is a little gross, so that we don't switch per-pixel or per-component
      if (img_n == out_n) {
         #define CASE(f) \
             case f:     \
                for (i=x-1; i >= 1; --i, raw+=img_n,cur+=img_n,prior+=img_n) \
                   for (k=0; k < img_n; ++k)
         switch (filter) {
            CASE(F_none)  cur[k] = raw[k];
				break;
            CASE(F_sub)   cur[k] = raw[k] + cur[k-img_n];
				break;
            CASE(F_up)    cur[k] = raw[k] + prior[k];
				break;
            CASE(F_avg)   cur[k] = raw[k] + ((prior[k] + cur[k-img_n])>>1);
				break;
            CASE(F_paeth)  cur[k] = (uint8_t) (raw[k] + paeth(cur[k-img_n],prior[k],prior[k-img_n]));
				break;
            CASE(F_avg_first)    cur[k] = raw[k] + (cur[k-img_n] >> 1);
				break;
            CASE(F_paeth_first)  cur[k] = (uint8_t) (raw[k] + paeth(cur[k-img_n],0,0));
				break;
         }
         #undef CASE
      } else {
         assert(img_n+1 == out_n);
         #define CASE(f) \
             case f:     \
                for (i=x-1; i >= 1; --i, cur[img_n]=255,raw+=img_n,cur+=out_n,prior+=out_n) \
                   for (k=0; k < img_n; ++k)
         switch (filter) {
            CASE(F_none)  cur[k] = raw[k];
				break;
            CASE(F_sub)   cur[k] = raw[k] + cur[k-out_n];
				break;
            CASE(F_up)    cur[k] = raw[k] + prior[k];
				break;
            CASE(F_avg)   cur[k] = raw[k] + ((prior[k] + cur[k-out_n])>>1);
				break;
            CASE(F_paeth)  cur[k] = (uint8_t) (raw[k] + paeth(cur[k-out_n],prior[k],prior[k-out_n]));
				break;
            CASE(F_avg_first)    cur[k] = raw[k] + (cur[k-out_n] >> 1);
				break;
            CASE(F_paeth_first)  cur[k] = (uint8_t) (raw[k] + paeth(cur[k-out_n],0,0));
				break;
         }
         #undef CASE
      }
   }
   return 0;
}

static int create_png_image(png *a, uint8_t *raw, uint32_t raw_len, int out_n, int interlaced)
{
   uint8_t *final;
   int p;

   if (!interlaced)
      return create_png_image_raw(a, raw, raw_len, out_n, a->img_x, a->img_y);

   // de-interlacing
   final = l_alloc(a->img_x * a->img_y * out_n);
   for (p=0; p < 7; ++p) {
      const int xorig[] = { 0,4,0,2,0,1,0 };
      const int yorig[] = { 0,0,4,0,2,0,1 };
      const int xspc[]  = { 8,8,4,4,2,2,1 };
      const int yspc[]  = { 8,8,8,4,4,2,2 };
      int i,j,x,y;

      x = (a->img_x - xorig[p] + xspc[p]-1) / xspc[p];
      y = (a->img_y - yorig[p] + yspc[p]-1) / yspc[p];
      if (x && y) {
         if (0!=create_png_image_raw(a, raw, raw_len, out_n, x, y)) {
            l_free(final);
            return -1;
         }
         for (j=0; j < y; ++j)
            for (i=0; i < x; ++i)
               memcpy(final + (j*yspc[p]+yorig[p])*a->img_x*out_n + (i*xspc[p]+xorig[p])*out_n,
                      a->out + (j*x+i)*out_n, out_n);
         l_free(a->out);
         raw += (x*a->img_n+1)*y;
         raw_len -= (x*a->img_n+1)*y;
      }
   }
   a->out = final;
   return 0;
}

static int compute_transparency(png *z, uint8_t tc[3], int out_n)
{
   png *s = z;
   uint32_t i, pixel_count = s->img_x * s->img_y;
   uint8_t *p = z->out;

   // compute color-based transparency, assuming we've
   // already got 255 as the alpha value in the output
   assert(out_n == 2 || out_n == 4);

   if (out_n == 2)
   {
      for (i=0; i < pixel_count; ++i)
      {
         p[1] = (p[0] == tc[0] ? 0 : 255);
         p += 2;
      }
   }
   else
   {
      for (i=0; i < pixel_count; ++i)
      {
         if (p[0] == tc[0] && p[1] == tc[1] && p[2] == tc[2])
            p[3] = 0;
         p += 4;
      }
   }
   return 1;
}

static int parse_png_file(png *p)
{
	uint8_t palette[1024], pal_img_n=0;
	int pal_len=0;
	uint8_t has_trans=0, tc[3];
	int first=1,interlace=0;
	uint32_t ioff=0, idata_limit=0;
	uint8_t *idata=NULL;
	
	for (;;)
	{
		chunk c = get_chunk_header(p);
		switch(c.type){
		case PNG_TYPE('I','H','D','R'): {
			 int depth,color,comp,filter;
			if(!first) goto out;
			first=0;
			if(c.length!=13) return -1;
			p->img_x = get32(p); if (p->img_x > (1 << 24)) goto out;
            p->img_y = get32(p); if (p->img_y > (1 << 24)) goto out;
            depth = get8(p);  if (depth != 8) goto out;
            color = get8(p);  if (color > 6) goto out;
            if (color == 3) pal_img_n = 3; else if (color & 1) goto out;
            comp  = get8(p);if (comp) goto out;
            filter= get8(p);if (filter) goto out;
            interlace = get8(p); if (interlace>1) goto out;
            if (!p->img_x || !p->img_y) goto out;
            if (!pal_img_n)
            {
				p->img_n = (color & 2 ? 3 : 1) + (color & 4 ? 1 : 0);
				if ((1 << 30) / p->img_x / p->img_n < p->img_y) return -1;
			}
			else
			{
				p->img_n=1;
			}
			break;
		}
		case PNG_TYPE('P','L','T','E'): {
			int i;
			if(first) goto out;
			if (c.length > 256*3) goto out;
			pal_len = c.length / 3;
            if (pal_len * 3 != c.length) return -1;
            for (i=0; i < pal_len; ++i)
            {
				palette[i*4+0] = get8u(p);
				palette[i*4+1] = get8u(p);
				palette[i*4+2] = get8u(p);
				palette[i*4+3] = 255;
			}
			break;
		}
		case PNG_TYPE('t','R','N','S'): {
			if(first) goto out;
			if(idata) goto out;
			if (pal_img_n)
			{
				int i;
				pal_img_n = 4;
				if (pal_len == 0) goto out;
				if(c.length>pal_len) goto out;
				for (i=0; i < c.length; ++i)
					palette[i*4+3] = get8u(p);
			}
			else
			{
				int i;
				if (!(p->img_n & 1)) goto out;
				if (c.length != (uint32_t) p->img_n*2) goto out;
				has_trans = 1;
				for (i=0; i < p->img_n; ++i)
					tc[i] = (uint8_t) get16(p);
			}
			break;
		}
		case PNG_TYPE('I','D','A','T'): {
			if(first) goto out;
			if (pal_img_n && !pal_len) goto out;
			if (ioff + c.length > idata_limit)
			{
				if (idata_limit == 0) idata_limit = c.length > 4096 ? c.length : 4096;
				while (ioff + c.length > idata_limit)
					idata_limit *= 2;
				idata = l_realloc(idata, idata_limit);
			}
			if(p->p+c.length>=p->end) goto out;
			memcpy(idata+ioff,p->p,c.length);
			p->p+=c.length;
			ioff += c.length;
			break;
		}
		case PNG_TYPE('I','E','N','D'): {
			int img_out_n;
			int raw_len;
			uint8_t *expanded;
			int ret;
			if(first) goto out;
			if (idata == NULL) goto out;
			expanded=l_zlib_decode_alloc(idata,ioff,&raw_len,1);
			l_free(idata);idata=NULL;
			if(!expanded) goto out;
			if((4==p->img_n+1 && !pal_img_n) || has_trans)
				img_out_n=p->img_n+1;
			else
				img_out_n=p->img_n;
			ret=create_png_image(p, expanded, raw_len, img_out_n, interlace);
			l_free(expanded);
			if(ret!=0) goto out;
			if (has_trans)
               if (0!=compute_transparency(p, tc, img_out_n)) goto out;
			if (pal_img_n)
			{
				p->img_n = pal_img_n;
				if (0!=expand_palette(p, palette))
                  goto out;
			}
			else if(img_out_n==1)
			{
				expand_grayscale(p,0);
			}
			else if(img_out_n==2)
			{
				expand_grayscale(p,1);
			}
			p->img_n=4;
			return 0;
		}
		default:
			if(first) goto out;
			if ((c.type & (1 << 29)) == 0)
			{
				goto out;
			}
			skip(p, c.length);
			break;
		}
		get32(p);
	}
out:
	l_free(idata);
	if(p->out)
	{
		l_free(p->out);
		p->out=NULL;
	}
	return -1;
}

void *l_png_load(const char *file,va_list ap)
{
	png *p;
	
	p=l_new0(png);
	p->content=(uint8_t*)l_file_vget_contents(file,&p->length,ap);
	if(!p->content)
	{
		l_free(p);
		return NULL;
	}
	if(memcmp(png_sig,p->content,sizeof(png_sig)))
	{
		l_free(p->content);
		l_free(p);
		return NULL;
	}
	p->p=p->content+sizeof(png_sig);
	p->end=p->content+p->length;
	
	if(parse_png_file(p)!=0)
	{
		l_free(p->content);
		l_free(p);
		return NULL;
	}
	l_free(p->content);
	p->content=NULL;
	return p;
}

void l_png_free(png *p)
{
	if(!p) return;
	l_free(p->out);
	l_free(p);
}

#ifdef TEST_LPNG

png *l_png_load_test(const char *file,...)
{
	va_list ap;
	png *p;
	va_start(ap,file);
	p=l_png_load(file,ap);
	va_end(ap);
	return p;
}

void detect_white(png *s)
{
	int pixelsize=s->img_x*s->img_y;
	int i;
	unsigned char *p=s->out;
	for(i=0;i<pixelsize;i++)
	{
		if(p[0]!=255 || p[1]!=255 || p[2]!=255 ||p[3]!=255)
		{
			printf("color detect fail\n");
			return;
		}
		p+=4;
	}
	//printf("ok\n");
}

int main(int arc,char *arg[])
{
	if(arc!=2)
		return -1;
	png *p;
	p=l_png_load_test(arg[1],NULL);
	if(!p)
	{
		printf("load %s fail\n",arg[1]);
		return -1;
	}
	printf("%dx%d %d\n",p->img_x,p->img_y,p->img_n);
	//detect_white(p);
	l_png_free(p);
	return 0;
}

#endif
