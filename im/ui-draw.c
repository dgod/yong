#include <llib.h>
#include <math.h>
#include <assert.h>
#include <cairo-xlib.h>
#include "ui-draw.h"

static double get_scale(void *win,void *dc)
{
	return 1.0;
}

void ui_draw_begin(DRAW_CONTEXT1 *ctx,void *win,void *dc)
{
	ctx->x=ctx->y=0;
	ctx->dc=dc;
	ctx->win=win;
	ctx->scale=get_scale(win,dc);
	cairo_set_antialias(dc,CAIRO_ANTIALIAS_NONE);
}

void ui_draw_end(DRAW_CONTEXT1 *ctx)
{
}

static void ui_set_source_color(DRAW_CONTEXT1 *ctx,UI_COLOR c)
{
	double r,g,b,a;
	r=c.r/255.0;
	g=c.g/255.0;
	b=c.b/255.0;
	a=c.a/255.0;
	cairo_set_source_rgba(ctx->dc,r,g,b,a);
}

void ui_draw_line(DRAW_CONTEXT1 *ctx,int x0_i,int y0_i,int x1_i,int y1_i,UI_COLOR color,double line_width)
{
	cairo_t *dc=ctx->dc;
	double x0=ctx->scale*x0_i,y0=ctx->scale*y0_i,x1=ctx->scale*x1_i,y1=ctx->scale*y1_i;
	line_width*=ctx->scale;
	if(line_width>2) line_width=2;
	ui_set_source_color(ctx,color);
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
		ui_set_source_color(ctx,color);
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

void ui_draw_rect(DRAW_CONTEXT1 *ctx,int x_i,int y_i,int w_i,int h_i,UI_COLOR color,double line_width)
{
	cairo_t *dc=ctx->dc;
	double x=ctx->scale*x_i,y=ctx->scale*y_i,w=ctx->scale*w_i,h=ctx->scale*h_i;
	line_width*=ctx->scale;
	if(line_width>2) line_width=2;
	cairo_set_antialias(dc,CAIRO_ANTIALIAS_DEFAULT);
	ui_set_source_color(ctx,color);
	cairo_set_line_width(dc,line_width);
	cairo_rectangle(dc,x+line_width/2,y+line_width/2,w-line_width,h-line_width);
	cairo_stroke(dc);
	cairo_set_antialias(dc,CAIRO_ANTIALIAS_NONE);
}

void ui_fill_rect(DRAW_CONTEXT1 *ctx,int x,int y,int w,int h,UI_COLOR color)
{
	ui_set_source_color(ctx,color);
	cairo_rectangle(ctx->dc,x,y,w,h);
	cairo_fill(ctx->dc);
}

void ui_draw_round_rect(DRAW_CONTEXT1 *ctx,int x_i,int y_i,int w_i,int h_i,int r_i,UI_COLOR stroke,UI_COLOR fill,double line_width)
{
	cairo_t *dc=ctx->dc;
	double x=ctx->scale*x_i,y=ctx->scale*y_i,w=ctx->scale*w_i,h=ctx->scale*h_i,r=ctx->scale*r_i;
	line_width*=ctx->scale;

	if(line_width>2) line_width=2;
	cairo_set_antialias(dc,CAIRO_ANTIALIAS_DEFAULT);

	const double degrees = M_PI / 180.0;
	double d=line_width/2;
	double fx=x+d,fy=y+d,fw=w-2*d,fh=h-2*d;

	cairo_new_sub_path(dc);
	cairo_arc (dc, fx + fw - r, fy + r, r, -90 * degrees, 0 * degrees);
	cairo_arc (dc, fx + fw - r, fy + fh - r, r, 0 * degrees, 90 * degrees);
	cairo_arc (dc, fx + r, fy + fh - r, r, 90 * degrees, 180 * degrees);
	cairo_arc (dc, fx + r, fy + r, r, 180 * degrees, 270 * degrees);
	cairo_close_path(dc);

	if(fill.a)
	{
		ui_set_source_color(ctx,fill);
		cairo_fill_preserve(dc);
	}
	if(stroke.a && line_width)
	{
		cairo_set_line_width(dc,line_width);
		ui_set_source_color(ctx,stroke);
		cairo_stroke_preserve(dc);
	}

	cairo_set_antialias(dc,CAIRO_ANTIALIAS_NONE);
	cairo_new_path(dc);
}

void ui_draw_text_begin(DRAW_CONTEXT1 *ctx)
{
}

int ui_draw_text_end(DRAW_CONTEXT1 *ctx)
{
	return 0;
}

void ui_draw_text(DRAW_CONTEXT1 *ctx,UI_FONT font,int x_i,int y_i,const void *text,UI_COLOR color)
{
	cairo_t *dc=ctx->dc;
	double x=ctx->scale*x_i,y=ctx->scale*y_i;
	ui_set_source_color(ctx,color);
	pango_layout_set_text (font->pango, text, -1);
	cairo_move_to(dc,x,y-font->extraSpaceAbove);
	pango_cairo_show_layout(dc,font->pango);
}

void ui_draw_image(DRAW_CONTEXT1 *ctx,UI_IMAGE image,int x_i,int y_i)
{
	cairo_t *dc=ctx->dc;
	double x=ctx->scale*x_i,y=ctx->scale*y_i;
	int w,h;
	if(!image)
		return;
	ui_image_size(image,&w,&h);
	cairo_rectangle(dc,x,y,w,h);
	cairo_clip(dc);
	cairo_set_source_surface(dc,image,x,y);
	cairo_paint(dc);
	cairo_reset_clip(dc);
}

void ui_stretch_image(DRAW_CONTEXT1 *ctx,UI_IMAGE image,int x_i,int y_i,int w_i,int h_i)
{
	double x=ctx->scale*x_i,y=ctx->scale*y_i,w=ctx->scale*w_i,h=ctx->scale*h_i;
	cairo_t *dc=ctx->dc;
	int w0,h0;
	double sx=1,sy=1;
	double ox=0,oy=0;
	
	if(!image)
		return;

	ui_image_size(image,&w0,&h0);
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
#if 1
	cairo_save(dc);
	
	cairo_translate(dc,x,y);
	cairo_rectangle(dc,0,0,w,h);
	cairo_clip(dc);
	cairo_scale(dc,sx,sy);
	cairo_set_source_surface(dc,image,ox,oy);
	cairo_paint(dc);
	
	cairo_restore(dc);
#else
	ui_image_draw_full(dc,image,x,y,w,h,0,0,w0,h0);
#endif
}

UI_FONT ui_font_parse(void * w,const char *s,double scale)
{
	PangoFontDescription *desc;
	GdkWindow *window;
	PangoLayout *res;
	cairo_t *cr;

	window=gtk_widget_get_window(w);
	desc=pango_font_description_from_string(s);
	assert(desc!=NULL);
	// NOTE: GDK_DPI_SCALE not affect this, so scale the size by ourself
	gint size=pango_font_description_get_size(desc);
	//pango_font_description_set_absolute_size(desc,size/1024*ui_scale*96/72*PANGO_SCALE);
	pango_font_description_set_size(desc,(int)size*scale);

	cr=gdk_cairo_create(window);
	res=pango_cairo_create_layout(cr);
	cairo_destroy(cr);
	assert(res!=NULL);
	pango_layout_set_font_description(res,desc);

	pango_layout_set_spacing(res,0);
	pango_layout_set_single_paragraph_mode(res,TRUE);
	PangoLanguage *lang=pango_language_from_string("zh-CN");
	pango_context_set_language(pango_layout_get_context(res),lang);
	pango_layout_set_text(res,"测",-1);
	PangoContext *context=pango_layout_get_context(res);

	PangoFontMetrics *metrics=pango_context_get_metrics(context,desc,lang);
	int baseline=metrics->descent/1024;
	pango_font_metrics_unref(metrics);
	PangoRectangle ink,logical;
	pango_layout_get_pixel_extents(res,&ink,&logical);

	pango_font_description_free(desc);
	
	UI_FONT font=l_alloc0(sizeof(*font));
	font->pango=res;
	font->extraSpaceAbove=ink.y+ink.height-(logical.height-baseline);
	font->extraSpaceBelow=font->extraSpaceBelow;
	return font;
}

void ui_font_free(UI_FONT font)
{
	if(!font)
		return;
	g_object_unref(font->pango);
	l_free(font);
}

int ui_text_size(void *dc,UI_FONT font,const void *text,int *w,int *h)
{
	int rw=-1,rh=-1;
	if(!((char*)text)[0])
	{
		pango_layout_set_text(font->pango," ",-1);
		pango_layout_get_pixel_size(font->pango,&rw,&rh);
		rw=0;
	}
	else
	{
		pango_layout_set_text(font->pango,text,-1);
		pango_layout_get_pixel_size(font->pango,&rw,&rh);
	}
	// printf("%s %d %d\n",text,rw,rh);
	if(w)
		*w=rw;
	if(h)
		*h=rh-font->extraSpaceAbove-font->extraSpaceBelow;
	return rw;
}

typedef struct{
	int width;
	int height;
}AtScaleData;

static void image_load_cb(GdkPixbufLoader *loader,GdkPixbuf **pixbuf)
{
	if(*pixbuf)
		return;
	*pixbuf=gdk_pixbuf_loader_get_pixbuf(loader);
	g_object_ref(*pixbuf);
}

static void image_size_cb(GdkPixbufLoader *loader,gint width,gint height,UI_SIZE *sz)
{
	if(sz->w>0 && sz->h>0)
	{
		gdk_pixbuf_loader_set_size(loader,sz->w,sz->h);
	}
	else if(sz->w<-1 && sz->w!=-10000)
	{
		width=(gint)round(width*(-sz->w)/10000.0);
		height=(gint)round(height*(-sz->w)/10000.0);
		gdk_pixbuf_loader_set_size(loader,width,height);
	}
}

GdkPixbuf *ui_image_load_pixbuf_at_size(const char *file,int width,int height,int where)
{
	char path[256];
	GdkPixbuf *pixbuf;
	if(!file)
	{
		return 0;
	}
	{
		if(!ui_image_path(file,path,where))
		{
			// printf("get image path fail %s\n",file);
			return NULL;
		}
		char *contents;
		size_t length;
		GdkPixbufLoader *load;
		contents=l_file_get_contents(path,&length,NULL);
		if(!contents)
		{
			// fprintf(stderr,"load %s contents fail\n",file);
			return NULL;
		}
		load=gdk_pixbuf_loader_new();
		UI_SIZE size={.w=width,.h=height};
		g_signal_connect(load,"size-prepared",G_CALLBACK(image_size_cb),&size);
		if(width>0 && height>0)
		{
			gdk_pixbuf_loader_set_size(load,width,height);
		}
		pixbuf=NULL;
		g_signal_connect(load,"area-prepared",G_CALLBACK(image_load_cb),&pixbuf);
		if(!gdk_pixbuf_loader_write(load,(const guchar*)contents,length,NULL))
		{
			l_free(contents);
		 	// fprintf(stderr,"load image %s fail\n",file);
			return NULL;
		}
		l_free(contents);
		gdk_pixbuf_loader_close(load,NULL);
	}
	return pixbuf;
}

static inline uint8_t MULT(uint8_t c,uint8_t a)
{
	uint32_t t = c * a + 0x80;
	return ((t >> 8) + t) >> 8;
}

UI_IMAGE ui_image_load_at_size(const char *file,int width,int height,int where)
{
	GdkPixbuf *pixbuf=ui_image_load_pixbuf_at_size(file,width,height,where);
	if(!pixbuf)
		return NULL;
	int n_channels=gdk_pixbuf_get_n_channels(pixbuf);
	if(n_channels!=3 && n_channels!=4)
	{
		fprintf(stderr,"pixbuf channels %d is not supported\n",n_channels);
		g_object_unref(pixbuf);
		return NULL;
	}
	int format=n_channels==4?CAIRO_FORMAT_ARGB32:CAIRO_FORMAT_RGB24;
	width=gdk_pixbuf_get_width(pixbuf);
	height=gdk_pixbuf_get_height(pixbuf);
	UI_IMAGE r=cairo_image_surface_create(format,width,height);
	cairo_surface_flush(r);
	const uint8_t *src=gdk_pixbuf_get_pixels(pixbuf);
	uint8_t *dst=cairo_image_surface_get_data(r);
	int stride=gdk_pixbuf_get_rowstride(pixbuf);
	int rstride=cairo_image_surface_get_stride(r);

	for (int y = 0; y < height; y++)
	{
		const uint8_t *p = src + y * stride;
		uint8_t *q = dst + y * rstride;
		for(int x=0;x<width;x++)
		{
			if(n_channels==3)
			{
#if L_BYTE_ORDER==L_LITTLE_ENDIAN
				q[0]=p[2];
				q[1]=p[1];
				q[2]=p[0];
#else
				q[1]=p[0];
				q[2]=p[1];
				q[3]=p[2];
#endif
				p+=3;
				q+=4;
			}
			else
			{
#if L_BYTE_ORDER==L_LITTLE_ENDIAN
				q[0]=MULT(p[2], p[3]);
				q[1]=MULT(p[1], p[3]);
				q[2]=MULT(p[0], p[3]);
				q[3]=p[3];
#else
				q[0]=p[3];
				q[1]=MULT(p[0], p[3]);
				q[2]=MULT(p[1], p[3]);
				q[3]=MULT(p[2], p[3]);
#endif
				p+=4;
				q+=4;
			}
		}
	}
	cairo_surface_mark_dirty(r); 
	g_object_unref(pixbuf);
	return r;
}

UI_IMAGE ui_image_load(const char *file,int where)
{
	return ui_image_load_at_size(file,-1,-1,where);
}

UI_IMAGE ui_image_part(UI_IMAGE image,int x,int y,int w,int h)
{
	cairo_format_t format=cairo_image_surface_get_format(image);
	if(format!=CAIRO_FORMAT_ARGB32 && format!=CAIRO_FORMAT_RGB24)
		return NULL;
	UI_IMAGE r=cairo_image_surface_create(format,w,h);
	if(!r)
		return NULL;
	cairo_surface_flush(r);
	int stride=cairo_image_surface_get_stride(image);
	int rstride=cairo_image_surface_get_stride(r);
	uint8_t *src=cairo_image_surface_get_data(image)+y*stride+x*sizeof(uint32_t);
	uint8_t *dst=cairo_image_surface_get_data(r);
	for(int i=0;i<h;i++)
	{
		memcpy(dst,src,sizeof(uint32_t)*w);
		src+=stride;
		dst+=rstride;
	}
	cairo_surface_mark_dirty(r);
	return r;
}

void ui_image_free(UI_IMAGE img)
{
	if(!img) return;
	cairo_surface_destroy(img);
}

int ui_image_size(UI_IMAGE img,int *w,int *h)
{
	*w=cairo_image_surface_get_width(img);
	*h=cairo_image_surface_get_height(img);
	return 0;
}

void ui_image_draw(UI_DC dc,UI_IMAGE img,int x,int y)
{
	int w,h;
	if(!img)
		return;
	ui_image_size(img,&w,&h);
	cairo_rectangle(dc,x,y,w,h);
	cairo_clip(dc);
	cairo_set_source_surface(dc,img,x,y);
	cairo_paint(dc);
	cairo_reset_clip(dc);
}

void ui_image_draw_full(UI_DC hdc,UI_IMAGE img,
				int dst_x,int dst_y,int dst_w,int dst_h,
				int x,int y,int w,int h)
{
	cairo_save(hdc);
	cairo_rectangle(hdc, dst_x, dst_y, dst_w, dst_h);
	cairo_clip(hdc);
	double sx = (double)dst_w / w;
	double sy = (double)dst_h / h;
	cairo_translate(hdc, dst_x - x * sx, dst_y - y * sy);
	cairo_scale(hdc, sx, sy);
	cairo_set_source_surface(hdc, img, 0, 0);
	cairo_paint(hdc);
	cairo_restore(hdc);
}

static uint8_t get_pixel_alpha(cairo_surface_t *surface,int x,int y)
{
	const uint8_t *data=cairo_image_surface_get_data(surface);
	int stride=cairo_image_surface_get_stride(surface);
#if L_BYTE_ORDER==L_LITTLE_ENDIAN
	return data[y*stride+4*x+3];
#else
	return data[y*stride+4*x];
#endif
}

UI_REGION ui_image_region(UI_IMAGE p,double scale)
{
	if(cairo_image_surface_get_format(p)!=CAIRO_FORMAT_ARGB32)
		return NULL;
	int w=cairo_image_surface_get_width(p)*scale;
	int h=cairo_image_surface_get_height(p)*scale;
	UI_REGION rgn=cairo_region_create();
	for(int j=0;j<h;j++)
	{
		GdkRectangle rc={.x=0,.y=j,.width=0,.height=1};
		for(int i=0;i<w;i++)
		{
			uint8_t a=get_pixel_alpha(p,(int)round(i/scale),(int)round(j/scale));
			if(a<80)
			{
				if(rc.width)
				{
					cairo_region_union_rectangle(rgn,&rc);
					rc.width=0;
				}
				continue;
			}
			if(!rc.width)
			{
				rc.x=i;
				rc.width=1;
			}
			else
			{
				rc.width++;
			}
		}
		if(rc.width)
		{
			cairo_region_union_rectangle(rgn,&rc);
		}
	}
	return rgn;
}

