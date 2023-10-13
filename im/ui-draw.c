#include <llib.h>
#include <math.h>
#include <assert.h>
#include "ui-draw.h"

void ui_draw_begin(DRAW_CONTEXT1 *ctx,void *win,void *dc)
{
	ctx->x=ctx->y=0;
	ctx->dc=dc;
	ctx->win=win;
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

void ui_draw_line(DRAW_CONTEXT1 *ctx,int x0,int y0,int x1,int y1,UI_COLOR color,double line_width)
{
	cairo_t *dc=ctx->dc;
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

void ui_draw_rect(DRAW_CONTEXT1 *ctx,int x,int y,int w,int h,UI_COLOR color,double line_width)
{
	cairo_t *dc=ctx->dc;
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

void ui_draw_round_rect(DRAW_CONTEXT1 *ctx,int x,int y,int w,int h,int r,UI_COLOR stroke,UI_COLOR fill,double line_width)
{
	cairo_t *dc=ctx->dc;
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

void ui_draw_text(DRAW_CONTEXT1 *ctx,UI_FONT font,int x,int y,const void *text,UI_COLOR color)
{
	cairo_t *dc=ctx->dc;
	ui_set_source_color(ctx,color);
	pango_layout_set_text (font->pango, text, -1);
	cairo_move_to(dc,x,y-font->extraSpaceAbove);
	pango_cairo_show_layout(dc,font->pango);
}

void ui_draw_image(DRAW_CONTEXT1 *ctx,UI_IMAGE image,int x,int y)
{
	cairo_t *dc=ctx->dc;
	int w,h;
	if(!image)
		return;
	w=gdk_pixbuf_get_width(image);
	h=gdk_pixbuf_get_height(image);
	cairo_rectangle(dc,x,y,w,h);
	cairo_clip(dc);
	gdk_cairo_set_source_pixbuf(dc,image,x,y);
	cairo_paint(dc);
	cairo_reset_clip(dc);
}

void ui_stretch_image(DRAW_CONTEXT1 *ctx,UI_IMAGE image,int x,int y,int w,int h)
{
	cairo_t *dc=ctx->dc;
	int w0,h0;
	double sx=1,sy=1;
	double ox=0,oy=0;

	if(!image)
		return;
	
	w0=gdk_pixbuf_get_width(image);
	h0=gdk_pixbuf_get_height(image);
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
	gdk_cairo_set_source_pixbuf(dc,image,ox,oy);
	cairo_paint(dc);
	
	cairo_restore(dc);
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

UI_IMAGE ui_image_load_at_size(const char *file,int width,int height,int where)
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

UI_IMAGE ui_image_load(const char *file,int where)
{
	return ui_image_load_at_size(file,-1,-1,where);
}

void ui_image_free(UI_IMAGE img)
{
	if(!img) return;
	g_object_unref(img);
}

