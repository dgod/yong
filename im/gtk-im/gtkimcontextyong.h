#ifndef _GTKIMCONTEXTYONG_H_
#define _GTKIMCONTEXTYONG_H_

#include <gtk/gtk.h>

extern GType gtk_type_im_context_yong;

#define GTK_TYPE_IM_CONTEXT_YONG              gtk_type_im_context_yong
#define GTK_IM_CONTEXT_YONG(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_IM_CONTEXT_YONG, GtkIMContextYong))
#define GTK_IM_CONTEXT_YONG_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT_YONG, GtkIMContextYongClass))
#define GTK_IS_IM_CONTEXT_YONG(obj)           (GTK_CHECK_TYPE ((obj), GTK_TYPE_IM_CONTEXT_YONG))
#define GTK_IS_IM_CONTEXT_YONG_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT_YONG))
#define GTK_IM_CONTEXT_YONG_GET_CLASS(obj)    (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT_YONG, GtkIMContextYongClass))


typedef struct _GtkIMContextYong       GtkIMContextYong;
typedef struct _GtkIMContextYongClass  GtkIMContextYongClass;

struct _GtkIMContextYongClass
{
  GtkIMContextClass parent_class;
};

void gtk_im_context_yong_register_type (GTypeModule *type_module);
GtkIMContext *gtk_im_context_yong_new (void);

void gtk_im_context_yong_shutdown (void);

#endif/*_GTKIMCONTEXTYONG_H_*/
