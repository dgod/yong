#include <glib-unix.h>

typedef struct{
	void *next;
	int fd;
	void (*cb)(int,int,void*);
	void *user_data;
	GSource *source;
}PollFD;

static PollFD *fds = NULL;

static gboolean source_func(gint fd, GIOCondition condition, PollFD *pfd)
{
	pfd->cb(fd, condition, pfd->user_data);
	return G_SOURCE_CONTINUE;
}

int ui_poll(int fd, int events, void (*cb)(int,int,void*),void *user_data)
{
	for(PollFD *p=fds; p; p=p->next)
	{
		if(p->fd == fd)
		{
			if(events==0)
			{
				g_source_destroy(p->source);
				g_source_unref(p->source);
				fds = l_slist_remove(fds, p);
				l_free(p);
				return 0;
			}
			return -1;
		}
	}
	PollFD *p = l_new(PollFD);
	p->fd = fd;
	p->cb = cb;
	p->user_data = user_data;
	p->source = g_unix_fd_source_new(fd,(GIOCondition)events);
	fds = l_slist_append(fds, p);
	g_source_set_callback(p->source, (GSourceFunc)source_func, p, NULL);
	g_source_attach(p->source, NULL);
	return 0;
}

