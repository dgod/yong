typedef struct _GWaylandSource GWaylandSource;
struct _GWaylandSource {
    GSource source;
    gboolean display_owned;
    struct wl_display *display;
    GPollFD fd;
};

static gboolean _g_wayland_source_prepare(GSource *source, gint *timeout)
{
    GWaylandSource *self = (GWaylandSource *)source;
    p_wl_display_flush(self->display);
    *timeout = -1;
    return FALSE;
}

static gboolean _g_wayland_source_check(GSource *source)
{
    GWaylandSource *self = (GWaylandSource *)source;

    return ( self->fd.revents & G_IO_IN );
}

static gboolean _g_wayland_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    GWaylandSource *self = (GWaylandSource *)source;
    gint r;

    r = p_wl_display_dispatch(self->display);
    //printf("dispatch %p %d\n",self->display,r);
    if ( r < 0 )
    {
		printf("wayland error %d\n",p_wl_display_get_error(self->display));
		exit(-1);
        /*if ( callback != NULL )
            return callback(user_data);
        return FALSE;*/
    }

    return TRUE;
}

static void _g_wayland_source_finalize(GSource *source)
{
    GWaylandSource *self = (GWaylandSource *)source;

    if ( self->display_owned )
        p_wl_display_disconnect(self->display);
}

static GSourceFuncs _g_wayland_source_funcs = {
    _g_wayland_source_prepare,
    _g_wayland_source_check,
    _g_wayland_source_dispatch,
    _g_wayland_source_finalize
};

static GWaylandSource *g_wayland_source_new_for_display(GMainContext *context, struct wl_display *display)
{
    GWaylandSource *source;

    source = (GWaylandSource *)g_source_new(&_g_wayland_source_funcs, sizeof(GWaylandSource));

    source->display = display;

    source->fd.fd = p_wl_display_get_fd(display);
    source->fd.events = G_IO_IN;

    g_source_add_poll((GSource *)source, &source->fd);
    g_source_attach((GSource *)source, context);

    return source;
}

static GWaylandSource *g_wayland_source_new(GMainContext *context, const gchar *name)
{
    struct wl_display *display;
    GWaylandSource *source;
   
    GdkDisplay *gd=gdk_display_get_default();
    if(gd!=NULL && !l_str_has_prefix(gdk_display_get_name(gd),":"))
    {
		display=p_gdk_wayland_display_get_wl_display(gd);
		source = (GWaylandSource *)g_source_new(&_g_wayland_source_funcs, sizeof(GWaylandSource));
		source->fd.fd = p_wl_display_get_fd(display);
		source->fd.events=0;
		source->display = display;
		return source;
	}

    display = p_wl_display_connect(name);
    if ( display == NULL )
    {
        return NULL;
	}

    source = g_wayland_source_new_for_display(context, display);
    source->display_owned = TRUE;
    return source;
}

#ifdef WAYLAND_STANDALONE
static void g_wayland_source_set_error_callback(GWaylandSource *self, GSourceFunc callback, gpointer user_data, GDestroyNotify destroy_notify)
{
    g_return_if_fail(self != NULL);

    g_source_set_callback((GSource *)self, callback, user_data, destroy_notify);
}
#endif

static struct wl_display *g_wayland_source_get_display(GWaylandSource *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return self->display;
}
