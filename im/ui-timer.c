#ifdef _WIN32
#include <windows.h>
#
#else
#include <glib.h>
#endif

#define MAX_TIMER_CB		8
#define MAX_IDLE_CB			4
typedef struct{
	void (*cb)(void *);
	void *arg;
#ifdef _WIN32
	UINT_PTR id;
#else
	guint id;
#endif
}UI_CALLBACK;

static UI_CALLBACK l_timers[MAX_TIMER_CB];
static UI_CALLBACK l_idles[MAX_IDLE_CB];

#ifdef _WIN32
static VOID CALLBACK _timer(HWND hwnd,UINT uMsg,UINT_PTR idEvent,DWORD dwTime)
{
	int i;
	KillTimer(NULL,idEvent);
	for(i=0;i<MAX_TIMER_CB;i++)
	{
		if(idEvent==l_timers[i].id)
			break;
	}
	if(i==MAX_TIMER_CB)
	{
		printf("not found\n");
		return;
	}
	void (*cb)(void*)=l_timers[i].cb;
	void *arg=l_timers[i].arg;
	l_timers[i].cb=NULL;
	l_timers[i].id=0;
	if(cb)
		cb(arg);
}

static void _idle(void)
{
	int i;
	for(i=0;i<MAX_IDLE_CB;i++)
	{
		if(l_idles[i].cb)
		{
			void (*cb)(void*)=l_idles[i].cb;
			void *arg=l_idles[i].arg;
			l_idles[i].cb=NULL;
			l_idles[i].arg=NULL;
			cb(arg);
		}
	}
}

#else
static gboolean _timer(gpointer arg)
{
	int i=GPOINTER_TO_INT(arg);
	void (*cb)(void*)=l_timers[i].cb;
	arg=l_timers[i].arg;
	l_timers[i].cb=NULL;
	l_timers[i].id=0;
	if(cb)
		cb(arg);
	return FALSE;
}
static gboolean _idle(gpointer arg)
{
	int i=GPOINTER_TO_INT(arg);
	void (*cb)(void*)=l_idles[i].cb;
	arg=l_idles[i].arg;
	l_idles[i].cb=NULL;
	l_idles[i].id=0;
	if(cb)
		cb(arg);
	return FALSE;
}
#endif

static void ui_timer_del(void (*cb)(void *),void *arg)
{
	int i;
	for(i=0;i<MAX_TIMER_CB;i++)
	{
		if(l_timers[i].cb==cb && l_timers[i].arg==arg)
		{
			l_timers[i].cb=NULL;
			l_timers[i].arg=NULL;
#ifdef _WIN32
			KillTimer(NULL,l_timers[i].id);
#else
			g_source_remove(l_timers[i].id);
#endif
			l_timers[i].id=0;
			break;
		}
	}
}

static void ui_idle_del(void (*cb)(void *),void *arg)
{
	int i;
	for(i=0;i<MAX_IDLE_CB;i++)
	{
		if(l_idles[i].cb==cb && l_idles[i].arg==arg)
		{
			l_idles[i].cb=NULL;
			l_idles[i].arg=NULL;
#ifndef _WIN32
			g_source_remove(l_idles[i].id);
#endif
			l_idles[i].id=0;
			break;
		}
	}
}

static int ui_timer_add(unsigned interval,void (*cb)(void *),void *arg)
{
	int i;
	ui_timer_del(cb,arg);
	for(i=0;i<MAX_TIMER_CB;i++)
	{
		if(l_timers[i].cb==NULL)
			break;
	}
	if(i==MAX_TIMER_CB)
		return -1;
	l_timers[i].cb=cb;
	l_timers[i].arg=arg;
#ifdef _WIN32
	l_timers[i].id=SetTimer(NULL,0,interval,_timer);
#else
	l_timers[i].id=g_timeout_add(interval,(GSourceFunc)_timer,GINT_TO_POINTER(i));
#endif
	return 0;
}

static int ui_idle_add(void (*cb)(void *),void *arg)
{
	int i;
	ui_idle_del(cb,arg);
	for(i=0;i<MAX_IDLE_CB;i++)
	{
		if(l_idles[i].cb==NULL)
			break;
	}
	if(i==MAX_IDLE_CB)
		return -1;
	l_idles[i].cb=cb;
	l_idles[i].arg=arg;
#ifdef _WIN32
	PostMessage(InputWin,WM_USER_IDLE,0,0);
#else
	l_idles[i].id=g_idle_add((GSourceFunc)_idle,GINT_TO_POINTER(i));
#endif
	return 0;
}
