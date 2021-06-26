#include <gtk/gtk.h>
#include <gtk/gtkimmodule.h>
#include "yongimcontext.h"

G_MODULE_EXPORT void g_io_module_load(GIOModule *module)
{
	g_type_module_use(G_TYPE_MODULE(module));
	yong_im_context_register_type(G_TYPE_MODULE(module));
	
	g_io_extension_point_implement(GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                   YONG_TYPE_IM_CONTEXT, "yong", 10);
}

G_MODULE_EXPORT void g_io_module_unload(GIOModule *module)
{
}

G_MODULE_EXPORT char **g_io_module_query(void)
{
	return g_strsplit(GTK_IM_MODULE_EXTENSION_POINT_NAME," ",-1);
}
