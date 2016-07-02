#ifndef __GTK_YBUTTON_H__
#define __GTK_YBUTTON_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_YBUTTON                 (gtk_ybutton_get_type ())
#define GTK_YBUTTON(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_YBUTTON, GtkYButton))
#define GTK_YBUTTON_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_YBUTTON, GtkYButtonClass))
#define GTK_IS_YBUTTON(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_YBUTTON))
#define GTK_IS_YBUTTON_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_YBUTTON))
#define GTK_YBUTTON_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_YBUTTON, GtkYButtonClass))

typedef struct _GtkYButton        GtkYButton;
typedef struct _GtkYButtonClass   GtkYButtonClass;

struct _GtkYButton
{
  GtkWidget widget;

  GdkWindow *event_window;
  
  PangoLayout *layout;
  GdkPixbuf *bg[3];
 
  guint activate_timeout;

  guint in_button : 1;
  guint button_down : 1;
  guint depressed : 1;
  guint depress_on_activate : 1;
};

struct _GtkYButtonClass
{
  GtkWidgetClass parent_class;
  void (* pressed)  (GtkYButton *button);
  void (* released) (GtkYButton *button);
  void (* clicked)  (GtkYButton *button);
  void (* enter)    (GtkYButton *button);
  void (* leave)    (GtkYButton *button);
  void (* activate) (GtkYButton *button);
};

enum{
	YBUTTON_NORMAL=0,
	YBUTTON_OVER,
	YBUTTON_DOWN,
};

GType          gtk_ybutton_get_type(void) G_GNUC_CONST;

GtkWidget *gtk_ybutton_new(void);
void gtk_ybutton_pressed(GtkYButton *button);
void gtk_ybutton_released(GtkYButton *button);
void gtk_ybutton_clicked(GtkYButton *button);
void gtk_ybutton_enter(GtkYButton *button);
void gtk_ybutton_leave(GtkYButton *button);
void gtk_ybutton_text(GtkYButton *button,char *text);
void gtk_ybutton_bg(GtkYButton *button,int state,GdkPixbuf *pixbuf);

G_END_DECLS

#endif /* __GTK_YBUTTON_H__ */

