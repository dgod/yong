#include "gtk/gtkimmodule.h"
#include "gtkimcontextyong.h"

#undef G_MODULE_EXPORT
#define G_MODULE_EXPORT __attribute__((visibility("default")))

#include <stdlib.h>
#include <string.h>

static GtkIMContextInfo yong_info = {
  "yong",
  "Yong Input Method",
  "yong",
  "/usr/share/locale",
  "*"
};

static const GtkIMContextInfo *info_list[] = {
  &yong_info
};

G_MODULE_EXPORT void im_module_init (GTypeModule *type_module)
{
  gtk_im_context_yong_register_type(type_module);
}

G_MODULE_EXPORT void im_module_exit (void)
{
  gtk_im_context_yong_shutdown();
}

G_MODULE_EXPORT void im_module_list (const GtkIMContextInfo ***contexts,int *n_contexts)
{
#if 0
  char *lang=getenv("LANG");
  if(lang && !strncmp(lang,"zh_CN.",6))
  	  yong_info.context_name="Yong输入法";
  else if(lang && !strncmp(lang,"zh_",3))
  	  yong_info.context_name="Yong輸入法";
#endif
  *contexts = info_list;
  *n_contexts = G_N_ELEMENTS (info_list);
}

G_MODULE_EXPORT GtkIMContext * im_module_create (const gchar *context_id)
{
  if (strcmp (context_id, "yong") == 0)
    return gtk_im_context_yong_new ();
  else
    return NULL;
}
