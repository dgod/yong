#pragma once

#include <gtk/gtk.h>

#define YONG_TYPE_IM_CONTEXT (yong_im_context_get_type())
#define YONG_IM_CONTEXT(obj)                                                  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), YONG_TYPE_IM_CONTEXT, YongIMContext))
#define YONG_IM_CONTEXT_CLASS(klass)                                          \
    (G_TYPE_CHECK_CLASS_CAST((klass), YONG_TYPE_IM_CONTEXT,                   \
                             YongIMContextClass))
#define YONG_IS_IM_CONTEXT(obj)                                               \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), YONG_TYPE_IM_CONTEXT))
#define YONG_IS_IM_CONTEXT_CLASS(klass)                                       \
    (G_TYPE_CHECK_CLASS_TYPE((klass), YONG_TYPE_IM_CONTEXT))
#define YONG_IM_CONTEXT_GET_CLASS(obj)                                        \
    (G_TYPE_CHECK_GET_CLASS((obj), YONG_TYPE_IM_CONTEXT, YongIMContextClass))

typedef struct _YongIMContext YongIMContext;
typedef struct _YongIMContextClass YongIMContextClass;

GType yong_im_context_get_type(void);
YongIMContext *yong_im_context_new(void);
void yong_im_context_register_type(GTypeModule *type_module);
