#include "llib.h"
#include "ui.h"
#include "common.h"

#include <assert.h>
#include <stdarg.h>

typedef struct{
	int event,x,y,which;
}UI_EVENT;

typedef struct{
	uint8_t r,g,b,a;
}UI_COLOR;

static char skin_path[64];
static double ui_scale=1.0;

#ifdef _WIN32

#include <windows.h>

//#include "png/pngusr.h"
//#include "png/png.h"

typedef HBITMAP UI_IMAGE;
typedef HICON UI_ICON;
typedef HFONT UI_FONT;
typedef HWND UI_WINDOW;
typedef HDC UI_DC;
typedef WCHAR UI_CHAR;

UI_WINDOW MainWin;
static bool png_blend;

static UI_IMAGE ui_image_load_bmp(const char *file,...)
{
	char path[256];
	WCHAR tmp[256];
	va_list ap;
	HBITMAP res=NULL;
	va_start(ap,file);
	do{
		const char *cur=va_arg(ap,const char*);
		if(!cur) break;
		snprintf(path,sizeof(path),"%s/%s",cur,file);
		l_utf8_to_utf16(path,tmp,sizeof(tmp));
		res=LoadImage(0,tmp,IMAGE_BITMAP,0,0,LR_LOADFROMFILE);
	}while(res==NULL);
	va_end(ap);
	if(!res)
	{
		l_utf8_to_utf16(file,tmp,sizeof(tmp));
		res=LoadImage(0,tmp,IMAGE_BITMAP,0,0,LR_LOADFROMFILE);
	}
	if(res)
	{
		int w,h;
		BITMAP bmp;
		BITMAPINFO bmi;
		HBITMAP hbmp;
		LPBYTE pBitsSrc;
		LPBYTE pBitsDest=NULL;
		int i,j,stride;
		
		GetObject((HGDIOBJ)res,sizeof(bmp),&bmp);
		if(bmp.bmBitsPixel<24)
		{
			DeleteObject(res);
			return NULL;
		}
		else if(bmp.bmBitsPixel==32)
		{
			return res;
		}
		w=bmp.bmWidth;h=bmp.bmHeight;pBitsSrc=bmp.bmBits;
		stride=(3*bmp.bmWidth+3)&~0x03;
		
		ZeroMemory(&bmi, sizeof(bmi));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		
		hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,(void **)&pBitsDest, NULL, 0);
		for(j=0;j<h;j++)
		{
			BYTE *data=pBitsSrc+h*stride;
			for(i=0;i<w;i++)
			{
				*pBitsDest++=data[0];
				*pBitsDest++=data[1];
				*pBitsDest++=data[2];
				*pBitsDest++=255;
				data+=3;
			}
		}
		DeleteObject(res);
		res=hbmp;
	
	}
	return res;
}
/*
static HBITMAP ui_png_to_bmp(png_structp png_ptr,png_infop info_ptr)
{
	int i,j,w,h;
	BITMAPINFO bmi;
	HBITMAP hbmp;
	LPBYTE pBitsDest=NULL;
	int step;

	w=info_ptr->width;h=info_ptr->height;
	step=info_ptr->pixel_depth/8;

	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,(void **)&pBitsDest, NULL, 0);
	
	if(step==3)
	{
		for(j=h-1;j>=0;j--)
		{
			BYTE *data=info_ptr->row_pointers[j];
			for(i=0;i<w;i++)
			{
				*pBitsDest++=data[2];
				*pBitsDest++=data[1];
				*pBitsDest++=data[0];
				*pBitsDest++=255;
				data+=step;
			}
		}
	}
	else
	{
		for(j=h-1;j>=0;j--)
		{
			BYTE *data=info_ptr->row_pointers[j];
			for(i=0;i<w;i++)
			{
				if(!png_blend)
				{
					*pBitsDest++=data[2];
					*pBitsDest++=data[1];
					*pBitsDest++=data[0];
				}
				else
				{
					*pBitsDest++=data[2]*data[3]/255;
					*pBitsDest++=data[1]*data[3]/255;
					*pBitsDest++=data[0]*data[3]/255;
				}
				*pBitsDest++=data[3];
				data+=step;
			}
		}
	}
	return hbmp;
}

static UI_IMAGE ui_image_load_png(const char *file,...)
{
	va_list ap;
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	HBITMAP res;
	
	va_start(ap,file);
	fp=l_file_vopen(file,"rb",ap,NULL);
	va_end(ap);
	if(!fp)
		return NULL;
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, fp);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);
	fclose(fp);
	if(!(info_ptr->valid & PNG_INFO_IDAT) || info_ptr->pixel_depth<24)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, 0);
		return NULL;
	}
	res=ui_png_to_bmp(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, 0);
	return res;
}
*/

#include "lpng.h"
static HBITMAP ui_png_to_bmp(L_PNG *p)
{
	int i,j,w,h;
	BITMAPINFO bmi;
	HBITMAP hbmp;
	LPBYTE pBitsDest=NULL;

	w=p->img_x;h=p->img_y;

	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,(void **)&pBitsDest, NULL, 0);
	
	for(j=h-1;j>=0;j--)
	{
		BYTE *data=p->out+j*w*4;
		for(i=0;i<w;i++)
		{
			if(!png_blend)
			{
				*pBitsDest++=data[2];
				*pBitsDest++=data[1];
				*pBitsDest++=data[0];
			}
			else
			{
				*pBitsDest++=data[2]*data[3]/255;
				*pBitsDest++=data[1]*data[3]/255;
				*pBitsDest++=data[0]*data[3]/255;
			}
			*pBitsDest++=data[3];
			data+=4;
		}
	}

	return hbmp;
}

static UI_IMAGE ui_image_load_png(const char *file,...)
{
	va_list ap;
	HBITMAP res;
	L_PNG *p;
	
	va_start(ap,file);
	p=l_png_load(file,ap);
	va_end(ap);
	if(!p)
		return NULL;
	res=ui_png_to_bmp(p);
	l_png_free(p);
	return res;
}

static UI_IMAGE ui_image_load(const char *file)
{
	char home_path[256],home_path2[256];
	char data_path[256],data_path2[256];
	
	sprintf(home_path,"%s/%s",y_im_get_path("HOME"),skin_path);
	sprintf(home_path2,"%s/skin",y_im_get_path("HOME"));
	sprintf(data_path,"%s/%s",y_im_get_path("DATA"),skin_path);
	sprintf(data_path2,"%s/skin",y_im_get_path("DATA"));
				
	if(l_str_has_suffix(file,".bmp"))
	{
		return ui_image_load_bmp(file,
				home_path,home_path2,y_im_get_path("HOME"),
				data_path,data_path2,y_im_get_path("DATA"),
				NULL);
	}
	else
	{
		return ui_image_load_png(file,
				home_path,home_path2,y_im_get_path("HOME"),
				data_path,data_path2,y_im_get_path("DATA"),
				NULL);
	}
}

UI_IMAGE ui_image_part(UI_IMAGE img,int x,int y,int w,int h)
{
	int j;
	BITMAP bmp;
	BITMAPINFO bmi;
	HBITMAP hbmp;
	LPBYTE pBitsDest=NULL,pBitsSrc;
	int stride_src,stride_dst;
	
	GetObject(img,sizeof(bmp),&bmp);
	
	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,(void **)&pBitsDest, NULL, 0);
	
	stride_dst=w*4;stride_src=bmp.bmWidth*4;
	pBitsSrc=bmp.bmBits+y*stride_src+x*4;
	for(j=0;j<h;j++)
	{
		memcpy(pBitsDest,pBitsSrc,stride_dst);
		pBitsSrc+=stride_src;
		pBitsDest+=stride_dst;
	}
	
	return hbmp;
}

static void ui_image_free(UI_IMAGE img)
{
	if(!img) return;
	DeleteObject(img);
}

static int ui_image_size(UI_IMAGE img,int *w,int *h)
{
	BITMAP bmp;
	GetObject((HGDIOBJ)img,sizeof(bmp),&bmp);
	*w=bmp.bmWidth;
	*h=bmp.bmHeight;
	return 0;
}

static void ui_image_draw(HDC hdc,HBITMAP bmp,int x,int y)
{
	BITMAP bm;
	if(!bmp)
		return;
	GetObject(bmp,sizeof(BITMAP),(LPVOID)&bm);
	{
		BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
		HDC tmp=CreateCompatibleDC(hdc);
		HBITMAP old;
		old=SelectObject(tmp,(HGDIOBJ)bmp);
		AlphaBlend(hdc,x,y,bm.bmWidth,bm.bmHeight,tmp,0,0,bm.bmWidth,bm.bmHeight,bf);
		SelectObject(tmp,(HGDIOBJ)old);
		DeleteDC(tmp);
	}
}

static void ui_image_draw_full(HDC hdc,HBITMAP bmp,
				int dst_x,int dst_y,int dst_w,int dst_h,
				int x,int y,int w,int h)
{
	if(!bmp)
		return;
	{
		BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
		HDC tmp=CreateCompatibleDC(hdc);
		HBITMAP old;
		old=SelectObject(tmp,(HGDIOBJ)bmp);
		AlphaBlend(hdc,dst_x,dst_y,dst_w,dst_h,tmp,x,y,w,h,bf);
		SelectObject(tmp,(HGDIOBJ)old);
		DeleteDC(tmp);
	}
}

static void ui_image_stretch(HDC hdc,HBITMAP bmp,int x,int y,int w,int h)
{
	BITMAP bm;
	if(!bmp)
		return;
	GetObject(bmp,sizeof(BITMAP),(LPVOID)&bm);
	{
		BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
		HDC tmp=CreateCompatibleDC(hdc);
		HBITMAP old;
		old=SelectObject(tmp,(HGDIOBJ)bmp);
		AlphaBlend(hdc,x,y,w,h,tmp,0,0,bm.bmWidth,bm.bmHeight,bf);
		SelectObject(tmp,(HGDIOBJ)old);
		DeleteDC(tmp);
	}
}

static UI_ICON ui_image_load_icon(const char *file,...)
{
	char path[256];
	WCHAR tmp[256];
	va_list ap;
	HICON res=NULL;
	va_start(ap,file);
	do{
		const char *cur=va_arg(ap,const char*);
		if(!cur) break;
		snprintf(path,sizeof(path),"%s/%s",cur,file);
		l_utf8_to_utf16(path,tmp,sizeof(tmp));
		res=LoadImage(0,tmp,IMAGE_ICON,0,0,LR_LOADFROMFILE);
	}while(res==NULL);
	va_end(ap);
	if(!res)
	{
		l_utf8_to_utf16(file,tmp,sizeof(tmp));
		res=LoadImage(0,tmp,IMAGE_ICON,0,0,LR_LOADFROMFILE);
	}
	return res;
}

static HBITMAP create_color_bitmap (int size,uint8_t **outdata)
{
  struct {
    BITMAPV4HEADER bmiHeader;
    RGBQUAD bmiColors[2];
  } bmi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bmi, sizeof (bmi));
  bmi.bmiHeader.bV4Size = sizeof (BITMAPV4HEADER);
  bmi.bmiHeader.bV4Height = bmi.bmiHeader.bV4Width = size;
  bmi.bmiHeader.bV4Planes = 1;
  bmi.bmiHeader.bV4BitCount = 1;
  bmi.bmiHeader.bV4V4Compression = BI_RGB;

  bmi.bmiColors[1].rgbBlue = 0xFF;
  bmi.bmiColors[1].rgbGreen = 0xFF;
  bmi.bmiColors[1].rgbRed = 0xFF;

  hdc = GetDC (NULL);
  if (!hdc)
    {
      return NULL;
    }
  hBitmap = CreateDIBSection (hdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS,
                              (PVOID *) outdata, NULL, (DWORD)0);
  ReleaseDC (NULL, hdc);
  return hBitmap;
}

static HBITMAP get_alpha_mask(HBITMAP hbmp)
{
	HBITMAP res;
	DIBSECTION ds;
	uint8_t *maskdata;
	uint8_t *indata;
	int size;
	int i,j;
	int rowstride;
	int maskstride;

	GetObject(hbmp, sizeof(ds), &ds );
	if(ds.dsBm.bmWidth!=ds.dsBm.bmHeight)
		return NULL;
	if(ds.dsBm.bmBitsPixel!=32)
		return NULL;
	size=ds.dsBm.bmWidth;
	if(size%8!=0 || size>256)
		return NULL;
	res=create_color_bitmap(size,&maskdata);
	indata=ds.dsBm.bmBits;
	rowstride=size*4;
	maskstride = (((size + 31) & ~31) >> 3);
	for(j=0;j<size;j++)
	{
		uint8_t *inrow;
		uint8_t *maskbyte;
		int mask_bit;
		
		inrow = indata + (size-j-1)*rowstride;
		maskbyte=maskdata + j*maskstride;
		mask_bit=0x80;
		
		for(i=0;i<size;i++)
		{
			if (inrow[4*i+3] == 0)
				maskbyte[0] |= mask_bit;
			else
				maskbyte[0] &= ~mask_bit;
			mask_bit >>= 1;
			if (mask_bit == 0)
			{
				mask_bit = 0x80;
				maskbyte++;
			}
		}
	}
	return res;
}

static UI_ICON ui_icon_load(const char *file)
{
	if(l_str_has_suffix(file,".ico"))
	{
		char home_path[256],home_path2[256];
		char data_path[256],data_path2[256];
		
		sprintf(home_path,"%s/%s",y_im_get_path("HOME"),skin_path);
		sprintf(home_path2,"%s/skin",y_im_get_path("HOME"));
		sprintf(data_path,"%s/%s",y_im_get_path("DATA"),skin_path);
		sprintf(data_path2,"%s/skin",y_im_get_path("DATA"));
		return ui_image_load_icon(file,
				home_path,home_path2,y_im_get_path("HOME"),
				data_path,data_path2,y_im_get_path("DATA"),
				NULL);
	}
	else
	{
		HICON res;
		HBITMAP hbmp=ui_image_load(file);
		if(!hbmp)
			return NULL;
		ICONINFO ii;
		ii.fIcon=TRUE;
		ii.xHotspot=0;
		ii.yHotspot=0;
		ii.hbmColor=hbmp;
		ii.hbmMask=get_alpha_mask(ii.hbmColor);
		res=CreateIconIndirect(&ii);
		DeleteObject(hbmp);
		DeleteObject(ii.hbmMask);
		return res;
	}
}

static void ui_icon_free(UI_ICON icon)
{
	if(!icon)
		return;
	DestroyIcon(icon);
}

static UI_FONT ui_font_parse(UI_WINDOW w,const char *s)
{
	int size;
	char face[96];
	HDC hdc;
	HFONT ret;
	int res;
	int weight;
	LOGFONT lf;
	char **list;
	
	list=l_strsplit(s,' ');
	res=l_strv_length(list);
	if(res>=2 && res<=6)
	{
		int i,len;
		size=atoi(list[res-1]);
		if(!stricmp(list[res-2],"Bold"))
		{
			weight=FW_BOLD;
			len=res-2;
		}
		else
		{
			weight=FW_REGULAR;
			len=res-1;
		}
		for(face[0]=0,i=0;i<len;i++)
		{
			strcat(face,list[i]);
			if(i!=len-1) strcat(face," ");
		}
	}
	else
	{
		size=12;
		weight=FW_REGULAR;
		strcpy(face,"Fixedsys");
	}
	l_strfreev(list);
	
	hdc=GetDC(w);
	size = -(size*GetDeviceCaps(hdc, LOGPIXELSY)/72); 
	ReleaseDC(w,hdc);
	memset(&lf,0,sizeof(lf));
	lf.lfHeight=size;
	lf.lfWeight=weight;
	lf.lfCharSet=DEFAULT_CHARSET;
	lf.lfOutPrecision=CLIP_DEFAULT_PRECIS;
	lf.lfClipPrecision=CLIP_DEFAULT_PRECIS;
	lf.lfQuality=DEFAULT_QUALITY;
	lf.lfPitchAndFamily=DEFAULT_PITCH;
	l_utf8_to_utf16(face,lf.lfFaceName,sizeof(lf.lfFaceName));
	ret=CreateFontIndirect(&lf);
	
	return ret;
}

static void ui_font_free(UI_FONT font)
{
	if(!font)
		return;
	DeleteObject(font);
}

static void ui_draw_text(UI_DC dc,UI_FONT font,int x,int y,const void *text,UI_COLOR color)
{
	HDC hdc;
	HBITMAP bmp,oldbmp;
	HFONT oldfont;
	BITMAPINFO bmi;
	LPBYTE pBitsDest,p;
	int i,w,h;
	SIZE sz;
	int len;
	int size;
	
	len=wcslen(text);
	if(len<=0) return;
	hdc=CreateCompatibleDC(dc);
	oldfont=SelectObject(hdc,font);
	GetTextExtentPoint(hdc,text,len,&sz);

	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w=sz.cx;
	bmi.bmiHeader.biHeight = h=sz.cy;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,(void **)&pBitsDest, NULL, 0);
	
	size=w*h;p=pBitsDest;
	for(i=0;i<size;i++)
	{
		*(DWORD*)p=0x00ffffff;
		p+=4;
	}
	
	oldbmp=SelectObject(hdc,bmp);	
	SetBkMode(hdc,TRANSPARENT);
	SetTextColor(hdc,RGB(0,0,0));
	TextOut(hdc,0,0,text,len);
	p=pBitsDest;
	for(i=0;i<size;i++)
	{
		DWORD t=*(DWORD*)p;
		if(L_UNLIKELY(t!=0x00ffffff))
		{
			// 应该用灰度作为alpha，为了速度选择用最有代表性的g分量
			BYTE alpha=0xff^((t>>8)&0xff);
			p[0]=color.b*alpha/255;
			p[1]=color.g*alpha/255;
			p[2]=color.r*alpha/255;
			p[3]=alpha;
		}
		else
		{
			*(int*)p=0;
		}
		p+=4;
	}
	SelectObject(hdc,oldfont);	
	SelectObject(hdc,oldbmp);
	DeleteObject(hdc);
	ui_image_draw(dc,bmp,x,y);
	DeleteObject(bmp);
}

static void ui_text_size(UI_DC dc,UI_FONT font,const void *text,int *w,int *h)
{
	SIZE sz;

	if(*(wchar_t*)text==0)
	{
		wchar_t temp[2]=L" ";
		GetTextExtentPoint(dc,temp,1,&sz);
		sz.cx=0;
	}
	else
	{
		if(!GetTextExtentPoint(dc,text,wcslen(text),&sz))
			return;
	}
	if(h) *h=sz.cy;
	*w=sz.cx;
}

typedef struct{
	int x,y,width,height;
	LPBYTE bits;
	DWORD pen;
	double line_width;
}DRAW_CONTEXT;

static void _ui_pen(DRAW_CONTEXT *ctx,UI_COLOR color)
{
	ctx->pen=color.b|(color.g<<8)|(color.r<<16)|(color.a<<24);
}

static void _ui_line_width(DRAW_CONTEXT *ctx,double w)
{
	if(w>2) w=2;
	else if(w<1) w=1;
	ctx->line_width=w;
}

static void _ui_moveto(DRAW_CONTEXT *ctx,int x,int y)
{
	ctx->x=x;
	ctx->y=y;
}

static inline void set_pixel(uint8_t *p,DWORD pen,int alpha)
{
	p[0]=(pen&0xff)*alpha/255+p[0]*(255-alpha)/255;
	p[1]=((pen>>8)&0xff)*alpha/255+p[1]*(255-alpha)/255;
	p[2]=((pen>>16)&0xff)*alpha/255+p[2]*(255-alpha)/255;
	//p[3]=255;
}

static void _ui_lineto(DRAW_CONTEXT *ctx,int x,int y)
{
	int i,orig,max,step;
	LPBYTE p;
	
	if(y<0 || x<0 || y>ctx->height || x>ctx->width)
		return;

	if(ctx->x==x)
	{
		step=ctx->width<<2;
		if(y<ctx->y)
		{
			orig=ctx->height-ctx->y-1;
			max=ctx->height-y-1;
		}
		else
		{
			orig=ctx->height-y-1;
			max=ctx->height-ctx->y-1;
		}
		p=ctx->bits+x*4+orig*step;
		for(i=orig;i<=max;i++)
		{
			*(DWORD*)p=ctx->pen;
			p+=step;
		}
		if(ctx->line_width!=1)
		{
			if(ctx->x<ctx->width-1)
				p=ctx->bits+(x+1)*4+orig*step;
			else
				p=ctx->bits+(x-1)*4+orig*step;
			for(i=orig;i<=max;i++)
			{
				set_pixel(p,ctx->pen,(int)((ctx->line_width-1)*255));
				p+=step;
			}
		}
	}
	else if(ctx->y==y)
	{
		if(x>ctx->x)
		{
			orig=ctx->x;
			max=x;
		}
		else
		{
			orig=x;
			max=ctx->x;
		}
		p=ctx->bits+orig*4+(ctx->height-y-1)*ctx->width*4;
		for(i=orig;i<=max;i++)
		{
			*(DWORD*)p=ctx->pen;
			p+=4;
		}
		if(ctx->line_width!=1)
		{
			if(ctx->y<ctx->height-1)
				p=ctx->bits+orig*4+(ctx->height-(y+1)-1)*ctx->width*4;
			else
				p=ctx->bits+orig*4+(ctx->height-(y-1)-1)*ctx->width*4;
			for(i=orig;i<=max;i++)
			{
				set_pixel(p,ctx->pen,(int)((ctx->line_width-1)*255));
				p+=4;
			}
		}
	}
	else
	{
	}
	_ui_moveto(ctx,x,y);
}

static void _ui_context(UI_DC dc,DRAW_CONTEXT *ctx)
{
	HBITMAP hbmp=GetCurrentObject(dc,OBJ_BITMAP);
	BITMAP bmp;
	GetObject(hbmp,sizeof(bmp),&bmp);
	ctx->bits=bmp.bmBits;
	ctx->width=bmp.bmWidth;
	ctx->height=bmp.bmHeight;
	ctx->x=ctx->y=0;
	ctx->pen=0;
}

static void ui_draw_line(UI_DC dc,int x0,int y0,int x1,int y1,UI_COLOR color,double line_width)
{	
	DRAW_CONTEXT ctx;
	_ui_context(dc,&ctx);
	_ui_pen(&ctx,color);
	_ui_line_width(&ctx,line_width);
	_ui_moveto(&ctx,x0,y0);
	_ui_lineto(&ctx,x1,y1);
}

static void ui_draw_rect(UI_DC dc,int x,int y,int w,int h,UI_COLOR color,double line_width)
{
	DRAW_CONTEXT ctx;
	
	if(w<1 || h<1)
		return;
	
	_ui_context(dc,&ctx);
	_ui_pen(&ctx,color);
	_ui_line_width(&ctx,line_width);
	
	_ui_moveto(&ctx,x,y);
	_ui_lineto(&ctx,x+w-1,y);
	_ui_lineto(&ctx,x+w-1,y+h-1);
	_ui_lineto(&ctx,x,y+h-1);
	_ui_lineto(&ctx,x,y);
}

static void ui_fill_rect(UI_DC dc,int x,int y,int w,int h,UI_COLOR color)
{
	int i,j;
	DRAW_CONTEXT ctx;
	LPBYTE p;
	
	if(w<1 || h<1)
		return;
	
	_ui_context(dc,&ctx);
	_ui_pen(&ctx,color);

	p=ctx.bits+(ctx.height-y-h)*ctx.width*4+x*4;
	for(j=0;j<h;j++)
	{
		for(i=0;i<w;i++)
		{
			*(DWORD*)p=ctx.pen;
			p+=4;
		}
		p+=(ctx.width-w)*4;
	}	
}

static void UpdateMainWindow(void);

#else

#include <gtk/gtk.h>
typedef GdkPixbuf *UI_IMAGE;
typedef GdkPixbuf *UI_ICON;
typedef PangoLayout *UI_FONT;
typedef GtkWidget *UI_WINDOW;
typedef cairo_t *UI_DC;
typedef char UI_CHAR;
#if GTK_CHECK_VERSION(3,0,0)
typedef cairo_region_t *UI_REGION;
#else
typedef GdkRegion *UI_REGION;
#endif

UI_WINDOW MainWin;

#define ui_icon_load(x) ui_image_load(x)
#define ui_icon_free(x) ui_icon_free(x)

static void image_load_cb(GdkPixbufLoader *loader,GdkPixbuf **pixbuf)
{
	if(*pixbuf)
		return;
	*pixbuf=gdk_pixbuf_loader_get_pixbuf(loader);
	g_object_ref(*pixbuf);
}

static UI_IMAGE ui_image_load(const char *file)
{
	char path[256];
	GdkPixbuf *pixbuf;
	if(!file)
	{
		return 0;
	}
	if(file[0]=='~' && file[1]=='/')
	{
		sprintf(path,"%s/%s",getenv("HOME"),file+2);
	}
	else
	{
		strcpy(path,file);
	}
	if(file[0]=='/')
	{
        	pixbuf = gdk_pixbuf_new_from_file(path, NULL);
	}
	else
	{
		char home_path[256],home_path2[256];
		char data_path[256],data_path2[256];
		char *contents;
		size_t length;
		GdkPixbufLoader *load;
		sprintf(home_path,"%s/%s",y_im_get_path("HOME"),skin_path);
		sprintf(home_path2,"%s/skin",y_im_get_path("HOME"));
		sprintf(data_path,"%s/%s",y_im_get_path("DATA"),skin_path);
		sprintf(data_path2,"%s/skin",y_im_get_path("DATA"));
		contents=l_file_get_contents(file,&length,
				home_path,home_path2,y_im_get_path("HOME"),
				data_path,data_path2,y_im_get_path("DATA"),
				NULL);
		if(!contents)
		{
			//fprintf(stderr,"load %s contents fail\n",file);
			return NULL;
		}
		load=gdk_pixbuf_loader_new();
		pixbuf=NULL;
		g_signal_connect(load,"area-prepared",G_CALLBACK(image_load_cb),&pixbuf);
		if(!gdk_pixbuf_loader_write(load,(const guchar*)contents,length,NULL))
		{
			l_free(contents);
			//fprintf(stderr,"load image %s fail\n",file);
			return NULL;
		}
		l_free(contents);
		pixbuf=gdk_pixbuf_loader_get_pixbuf(load);
		if(pixbuf) g_object_ref(pixbuf);
		gdk_pixbuf_loader_close(load,NULL);
	}
	return pixbuf;
}

UI_IMAGE ui_image_part(UI_IMAGE img,int x,int y,int w,int h)
{
	return gdk_pixbuf_new_subpixbuf(img,x,y,w,h);
}

static void ui_image_free(UI_IMAGE img)
{
	if(!img) return;
	g_object_unref(img);
}

static int ui_image_size(UI_IMAGE img,int *w,int *h)
{
	*w=gdk_pixbuf_get_width(img);
	*h=gdk_pixbuf_get_height(img);
	return 0;
}

static void ui_image_draw(UI_DC dc,UI_IMAGE img,int x,int y)
{
	int w,h;
	if(!img)
		return;
	ui_image_size(img,&w,&h);
	cairo_rectangle(dc,x,y,w,h);
	cairo_clip(dc);
	gdk_cairo_set_source_pixbuf(dc,img,x,y);
	cairo_paint(dc);
	cairo_reset_clip(dc);
}

static void ui_image_stretch(UI_DC dc,UI_IMAGE img,int x,int y,int w,int h)
{
	int w0,h0;
	double sx=1,sy=1;
	double ox=0,oy=0;

	if(!img)
		return;
	
	ui_image_size(img,&w0,&h0);
	if(w!=w0)
	{
		sx=(double)(w)/(double)(w0-1);
		ox=-0.5;
	}
	if(h!=h0)
	{
		sy=(double)h/(double)(h0-1);
		oy=-0.5;
	}
	
	cairo_save(dc);
	
	cairo_translate(dc,x,y);
	cairo_rectangle(dc,0,0,w,h);
	cairo_clip(dc);
	cairo_scale(dc,sx,sy);
	gdk_cairo_set_source_pixbuf(dc,img,ox,oy);
	cairo_paint(dc);
	
	cairo_restore(dc);
}

static UI_FONT ui_font_parse(UI_WINDOW w,const char *s)
{
	PangoFontDescription *desc;
	GdkWindow *window;
	PangoLayout *res;
	cairo_t *cr;

#if defined(GSEAL_ENABLE) || GTK_CHECK_VERSION(3,0,0)
	window=gtk_widget_get_window(w);
#else
	window=w->window;
#endif

	desc=pango_font_description_from_string(s);
	assert(desc!=NULL);
	cr=gdk_cairo_create(window);
	res=pango_cairo_create_layout(cr);
	cairo_destroy(cr);
	assert(res!=NULL);
	pango_layout_set_font_description(res,desc);
	pango_font_description_free(desc);
		
	return res;
}

static void ui_font_free(UI_FONT font)
{
	if(!font)
		return;
	g_object_unref(font);
}

static void ui_set_source_color(UI_DC dc,UI_COLOR c)
{
	double r,g,b,a;
	r=c.r/255.0;
	g=c.g/255.0;
	b=c.b/255.0;
	a=c.a/255.0;
	cairo_set_source_rgba(dc,r,g,b,a);
}

static void ui_draw_text(UI_DC dc,UI_FONT font,int x,int y,const void *text,UI_COLOR color)
{
	pango_layout_set_text (font, text, -1);
	cairo_move_to(dc,x,y);
	cairo_set_source_rgba(dc,color.r/255.0,color.g/255.0,color.b/255.0,color.a/255.0);
	pango_cairo_show_layout(dc,font);
}

static void ui_text_size(UI_DC dc,UI_FONT font,const char *text,int *w,int *h)
{
	if(!text[0])
	{
		pango_layout_set_text(font," ",-1);
		pango_layout_get_pixel_size(font,w,h);
		*w=0;
	}
	else
	{
		pango_layout_set_text(font,text,-1);
		pango_layout_get_pixel_size(font,w,h);
	}	
}

static void ui_draw_line(UI_DC dc,int x0,int y0,int x1,int y1,UI_COLOR color,double line_width)
{
	if(line_width>2) line_width=2;
	ui_set_source_color(dc,color);
	if(line_width==1 || line_width==2)
	{
		cairo_set_line_width(dc,line_width);
		cairo_move_to(dc,x0,y0);
		cairo_line_to(dc,x1,y1);
	}
	else
	{
		cairo_set_line_width(dc,1);
		cairo_move_to(dc,x0,y0);
		cairo_line_to(dc,x1,y1);
		cairo_stroke(dc);
		
		color.a*=line_width-1;
		ui_set_source_color(dc,color);
		if(y0==y1)
		{
			cairo_move_to(dc,x0,y0+1);
			cairo_line_to(dc,x1,y1+1);
		}
		else
		{
			cairo_move_to(dc,x0+1,y0);
			cairo_line_to(dc,x1+1,y1);
		}
	}
	cairo_stroke(dc);
}

static void ui_draw_rect(UI_DC dc,int x,int y,int w,int h,UI_COLOR color,double line_width)
{
	if(line_width>2) line_width=2;
	cairo_set_antialias(dc,CAIRO_ANTIALIAS_DEFAULT);
	ui_set_source_color(dc,color);
	cairo_set_line_width(dc,line_width);
	cairo_rectangle(dc,x+line_width/2,y+line_width/2,w-line_width,h-line_width);
	cairo_stroke(dc);
	cairo_set_antialias(dc,CAIRO_ANTIALIAS_NONE);
}

static void ui_fill_rect(UI_DC dc,int x,int y,int w,int h,UI_COLOR color)
{
	ui_set_source_color(dc,color);
	cairo_rectangle(dc,x,y,w,h);
	cairo_fill(dc);
}

static void UpdateMainWindow(void)
{
	gtk_widget_queue_draw(MainWin);
}

static void ui_region_destroy(UI_REGION r)
{
#if GTK_CHECK_VERSION(3,0,0)
	cairo_region_destroy(r);
#else
	gdk_region_destroy(r);
#endif
}

#endif

/*
static void ui_image_tile(UI_DC dc,UI_IMAGE img,int x,int y,int w,int h)
{
	int w0,h0,cx,cy,i,j;
	ui_image_size(img,&w0,&h0);
	cx=w/w0,cy=h/h0;
	for(j=0;j<cy;j++)
	for(i=0;i<cx;i++)
		ui_image_draw(dc,img,x+w0*i,y+h0*j);
}
*/

enum{
	UI_STATE_NORMAL=0,
	UI_STATE_DOWN,
	UI_STATE_OVER,
	UI_STATE_DISABLE,
};

enum{
	UI_EVENT_DOWN=0,
	UI_EVENT_UP,
	UI_EVENT_MOVE,
	UI_EVENT_LEAVE,
};

enum{
	UI_BUTTON_LEFT=0,
	UI_BUTTON_MIDDLE,
	UI_BUTTON_RIGHT,
};

typedef struct{
	bool visible;
	int state;
	UI_RECT rc;
	UI_IMAGE bmp[3];
	void (*click)(void *);
	void *arg;

	UI_CHAR text[32];
	UI_FONT font;
	UI_COLOR color;
}UI_BTN_REAL;

static UI_BTN_REAL btns[UI_BTN_COUNT];

static UI_IMAGE MainWin_bg;
static UI_COLOR MainWin_bgc;
static UI_COLOR MainWin_border;
static UI_RECT MainWin_move;
static int MainWin_X,MainWin_Y,MainWin_W,MainWin_H;
static int MainWin_Drag;
static int MainWin_Drag_X,MainWin_Drag_Y;
static int MainWin_tran;
static int MainWin_auto_tran;

UI_WINDOW InputWin;
static int InputWin_X,InputWin_Y;
static int InputWin_Drag;
static int InputWin_Drag_X,InputWin_Drag_Y;

struct{
	int scale;
	int CodeX,CodeY;
	int CandX,CandY;
	int OffX,OffY;
	int Height,Width;
	int mWidth,mHeight;
	int RealHeight,RealWidth;
	int Left,Right;
	int Top,Bottom;
	int WorkLeft,WorkRight;
	int MaxHeight;
	UI_IMAGE bg[3];
#ifndef _WIN32
	UI_REGION rgn[3];
	GdkRectangle clip;
#endif
	UI_COLOR bg_color;
	UI_COLOR border;
	UI_COLOR sep;
	UI_COLOR text[7];
	UI_FONT layout;
	int space;
	int root;
	int noshow;
	int line;
	int page;
	int caret;
	int no;
	int strip;
	int x,y;
	int tran;
	double line_width;
}InputTheme;

struct{
	int scale;
	double line_width;
	int move_style;
}MainTheme;

static bool ui_pt_in_rect(const UI_RECT *rc,int x,int y)
{
	return x>=rc->x && y>=rc->y &&
		x<rc->x+rc->w && y<rc->y+rc->h;
}

static UI_COLOR ui_color_parse(const char *s)
{
	UI_COLOR clr;
	int len=strlen(s);
	int a=255,r,g,b;
	if(len==9)
		l_sscanf(s,"#%02x%02x%02x%02x",&a,&r,&g,&b);
	else
		l_sscanf(s,"#%02x%02x%02x",&r,&g,&b);
	clr.a=a;clr.g=g;clr.b=b;clr.r=r;
	return clr;
}

static void ui_popup_menu(void);
static int ui_button_event(UI_WINDOW win,UI_EVENT *event,UI_BTN_REAL **under)
{
	double scale=MainTheme.scale!=1?ui_scale:1;
	UI_BTN_REAL *cur=NULL;
	UI_BTN_REAL *last=NULL;
	int dirty=0;
	int i;

	if(!MainWin_bg)
	{
		event->x=(int)(event->x/scale);
		event->y=(int)(event->y/scale);
	}
	if(event->event!=UI_EVENT_LEAVE)
	for(i=0;i<UI_BTN_COUNT;i++)
	{
		if(btns[i].visible==false)
		{
			continue;
		}
		if(ui_pt_in_rect(&btns[i].rc,event->x,event->y))
			cur=btns+i;
		if(btns[i].state!=UI_STATE_NORMAL)
			last=btns+i;
	}
	if(last!=cur)
		dirty++;
	switch(event->event){
	case UI_EVENT_DOWN:
		if(last && last!=cur && last->state!=UI_STATE_NORMAL)
		{			
			last->state=UI_STATE_NORMAL;
			dirty++;
		}
		if(cur && cur->state!=UI_STATE_DOWN)
		{
			cur->state=UI_STATE_DOWN;
			dirty++;
		}
		break;
	case UI_EVENT_UP:
		if(last && last!=cur && last->state!=UI_STATE_NORMAL)
		{
			last->state=UI_STATE_NORMAL;
			dirty++;
		}
		if(cur && cur->state!=UI_STATE_OVER && event->which==UI_BUTTON_LEFT)
		{
			cur->state=UI_STATE_OVER;
			dirty++;
			if(last==cur)
			{
				if(cur->click)
				{
					cur->click(cur->arg);
				}
				else if(cur==btns+UI_BTN_MENU)
				{
					y_im_setup_config();
				}
			}
		}
		if(cur && event->which==UI_BUTTON_RIGHT)
		{
			if(cur==btns+UI_BTN_MENU)
				ui_popup_menu();
			else if(cur==btns+UI_BTN_KEYBOARD)
				y_kbd_popup_menu();
		}
		break;
	case UI_EVENT_MOVE:
		if(last) last->state=UI_STATE_NORMAL;
		if(cur) cur->state=UI_STATE_OVER;
		break;
	case UI_EVENT_LEAVE:
		if(last)
		{
			last->state=UI_STATE_NORMAL;
			dirty++;
		}
		for(i=0;i<UI_BTN_COUNT;i++)
		{
			if(btns[i].visible==false)
			{
				continue;
			}
			if(btns[i].state!=UI_STATE_NORMAL)
			{
				btns[i].state=UI_STATE_NORMAL;
				dirty++;
			}
		}
		break;
	}
	if(dirty)
	{
		UpdateMainWindow();
	}
	if(under)
		*under=cur;
	return dirty;
}

int ui_button_update(int id,UI_BUTTON *param)
{
	UI_BTN_REAL *btn=btns+id;
	int i;

	for(i=0;i<3;i++)
	{
		if(btn->bmp[i])
		{
			ui_image_free(btn->bmp[i]);
			btn->bmp[i]=0;
		}
	}
	if(btn->font)
	{
		ui_font_free(btn->font);
		btn->font=0;
	}
	if(!param)
	{
		btn->visible=false;
		btn->rc.w=0;
		return 0;
	}
#ifdef _WIN32
	png_blend=true;
#endif
	btn->bmp[0]=ui_image_load(param->normal);
	if(param->down)
		btn->bmp[1]=ui_image_load(param->down);
	if(param->over)
		btn->bmp[2]=ui_image_load(param->over);
#ifdef _WIN32
	png_blend=false;
#endif
	if(btn->bmp[0])
	{
		int w,h;
		ui_image_size(btn->bmp[0],&w,&h);
		btn->rc.w=w;btn->rc.h=h;
	}
	else
	{
		btn->rc.w=0;
		btn->rc.h=0;
	}
	btn->visible=false;
	btn->rc.x=param->x;
	btn->rc.y=param->y;
	btn->font=param->font?ui_font_parse(MainWin,param->font):0;
	btn->color=param->color?ui_color_parse(param->color):(UI_COLOR){0,0,9,0};
	btn->click=param->click;
	btn->arg=param->arg;
	if(id==UI_BTN_MENU || id==UI_BTN_NAME || id==UI_BTN_KEYBOARD)
		ui_button_show(id,1);
	return 0;
}

int ui_button_show(int id,int show)
{
	btns[id].visible=show;
	if(!show)
		btns[id].state=UI_STATE_NORMAL;
	UpdateMainWindow();
	return 0;
}

int ui_button_label(int id,const char *text)
{
	if(!btns[id].rc.w)
		return 0;
	y_im_str_encode(text,btns[id].text,0);
	UpdateMainWindow();
	return 0;
}

void ui_skin_path(const char *p)
{
	strncpy(skin_path,p,63);
	skin_path[63]=0;
}

static void ui_draw_main_win(UI_DC cr)
{
	double scale=MainTheme.scale!=1?ui_scale:1;
	int i;

#ifndef _WIN32
	//if(MainWin_bg || MainTheme.line_width==1 || MainTheme.line_width==2)
		cairo_set_antialias(cr,CAIRO_ANTIALIAS_NONE);
#endif
	
	if(MainWin_bg)
	{
#ifndef _WIN32
		cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
#endif
		ui_image_draw(cr,MainWin_bg,0,0);
#ifndef _WIN32
		cairo_set_operator(cr,CAIRO_OPERATOR_OVER);
#endif
	}
	else
	{
		double w=MainWin_W;
		double h=MainWin_H;
		
		ui_fill_rect(cr,0,0,w,h,MainWin_bgc);
		ui_draw_rect(cr,0,0,w,h,MainWin_border,MainTheme.line_width*scale);
		if(MainWin_move.w>=3 && MainWin_move.h>=3)
		{
			if(MainTheme.move_style==0)
			{
				double x=MainWin_move.x*scale+MainWin_move.w*scale/2+0.5;
				double y=MainWin_move.y*scale+0.5;
			
				ui_draw_line(cr,x,y,x,MainWin_move.h*scale+y,MainWin_border,MainTheme.line_width*scale);
				ui_draw_line(cr,x+2,y,x+2,MainWin_move.h*scale+y,MainWin_border,MainTheme.line_width*scale);
			}
			else
			{
				double x=MainWin_move.x*scale+MainWin_move.w*scale/3+0.5;
				double y=MainWin_move.y*scale+0.5;
				double d=MainWin_move.w*scale/3;

				ui_draw_line(cr,x,y,x,MainWin_move.h*scale+y,MainWin_border,MainTheme.line_width*scale);
				ui_draw_line(cr,x+d,y,x+d,MainWin_move.h*scale+y,MainWin_border,MainTheme.line_width*scale);
			}
				
		}
	}
	for(i=0;i<UI_BTN_COUNT;i++)
	{
		UI_BTN_REAL *btn=btns+i;
		UI_RECT *r=&btn->rc;
		if(!btn->visible || !r->w) continue;
		if(MainWin_bg)
		{
			if((btn->state==UI_STATE_DOWN) && btn->bmp[1])
				ui_image_draw(cr,btn->bmp[1],r->x,r->y);
			else if(btn->state==UI_STATE_OVER && btn->bmp[2])
				ui_image_draw(cr,btn->bmp[2],r->x,r->y);
			else
				ui_image_draw(cr,btn->bmp[0],r->x,r->y);
		}
		else
		{
			int x=(int)(r->x*scale);
			int y=(int)(r->y*scale);
			int w=(int)(r->w*scale);
			int h=(int)(r->h*scale);

			if((btn->state==UI_STATE_DOWN) && btn->bmp[1])
				ui_image_stretch(cr,btn->bmp[1],x,y,w,h);
			else if(btn->state==UI_STATE_OVER && btn->bmp[2])
				ui_image_stretch(cr,btn->bmp[2],x,y,w,h);
			else
				ui_image_stretch(cr,btn->bmp[0],x,y,w,h);
		}
		if(btn->text[0])
		{
			int x,y,w,h;
			UI_RECT *rc=&btn->rc;
#ifdef _WIN32
			UI_FONT old;
			old=SelectObject(cr,btn->font);
#endif
			ui_text_size(cr,btn->font,btn->text,&w,&h);
#ifdef _WIN32
			SelectObject(cr,old);
#endif
			x=rc->x+(rc->w-w)/2;y=rc->y+(rc->h-h)/2;			
			if(!MainWin_bg)
			{
				x=(int)(x*scale);y=(int)(y*scale);
			}
			ui_draw_text(cr,btn->font,x,y,btn->text,btn->color);
		}
	}
}

static void ui_draw_input_win(UI_DC cr)
{
	UI_COLOR color;
	int count=0,i;
	EXTRA_IM *eim=CURRENT_EIM();
		
#ifndef _WIN32
	//if(InputTheme.bg[1] || InputTheme.line_width==1 || InputTheme.line_width==2)
		cairo_set_antialias(cr,CAIRO_ANTIALIAS_NONE);
#endif

	if(eim)
		count=eim->CandWordCount;

	if(!InputTheme.bg[1])
	{
		double w=InputTheme.RealWidth;
		double h=InputTheme.RealHeight;
		//int line=InputTheme.line;
		ui_fill_rect(cr,0,0,w,h,InputTheme.bg_color);
		ui_draw_rect(cr,0,0,w,h,InputTheme.border,InputTheme.line_width*ui_scale);
	}
	else/* if(InputTheme.line!=2)*/
	{
#ifndef _WIN32
		cairo_set_operator(cr,CAIRO_OPERATOR_SOURCE);
#endif
		if(InputTheme.bg[0])
		{
			ui_image_draw(cr,InputTheme.bg[0],0,0);
		}
		if(InputTheme.bg[2])
		{
			ui_image_draw(cr,InputTheme.bg[2],InputTheme.RealWidth-InputTheme.Right,0);
		}
		ui_image_stretch(cr,InputTheme.bg[1],
				InputTheme.Left,0,
				InputTheme.RealWidth-InputTheme.Left-InputTheme.Right,
				InputTheme.RealHeight);
#ifndef _WIN32
		cairo_set_operator(cr,CAIRO_OPERATOR_OVER);
#endif
	}
	
	if(InputTheme.line!=1 && InputTheme.sep.a!=0)
	{
		double w=InputTheme.RealWidth;
		double h=InputTheme.Height;
		ui_draw_line(cr,4,h/2-1,w-7,h/2-1,InputTheme.sep,InputTheme.line_width*ui_scale);
	}

	if(eim && eim->StringGet[0])
	{
		ui_draw_text(cr,InputTheme.layout,im.CodePos[0],InputTheme.CodeY,
			im.StringGet,InputTheme.text[6]);

	}
	if(im.CodeInput[0])
	{
		ui_draw_text(cr,InputTheme.layout,im.CodePos[1],InputTheme.CodeY,
			im.CodeInput,InputTheme.text[5]);
	}
	if(/*!im.EnglishMode && */eim && eim->CandPageCount>1 && InputTheme.page)
	{
		double pos_x,pos_y;
		pos_x=im.PagePosX;
		pos_y=im.PagePosY;
		ui_draw_text(cr,InputTheme.layout,pos_x,pos_y,im.Page,InputTheme.text[4]);
	}
	if(InputTheme.caret)
	{
		ui_draw_line(cr,im.CodePos[2],InputTheme.CodeY+2,
					im.CodePos[2],InputTheme.CodeY+im.cursor_h,
					InputTheme.text[3],InputTheme.line_width);
	}

	for(i=0;i<count;i++)
	{
		double *posx=im.CandPosX+3*i;
		double *posy=im.CandPosY+3*i;

		if(InputTheme.no==0)
		{
			color=InputTheme.text[0];
			ui_draw_text(cr,InputTheme.layout,posx[0],posy[0],YongGetSelectNumber(i),color);
		}
		else
		{
			color=InputTheme.text[0];
		}

		if(i==eim->SelectIndex) color=InputTheme.text[1];
		ui_draw_text(cr,InputTheme.layout,posx[1],posy[1],im.CandTable[i],color);

		if(im.Hint && eim->CodeTips[i] && *eim->CodeTips[i])
		{
			color=InputTheme.text[2];
			ui_draw_text(cr,InputTheme.layout,posx[2],posy[2],im.CodeTips[i],color);
		}
	}
}
