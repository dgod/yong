#include <QGuiApplication>
#include <QKeyEvent>
#include <QTimer>
#include <QInputMethod>
#include <QTextCharFormat>
#include <qwidget.h>
#include <QtGui/qpa/qplatformscreen.h>
#include <QtGui/qpa/qplatformcursor.h>
#include <QtGui/qpa/qwindowsysteminterface.h>

#include <xkbcommon/xkbcommon.h>
#include <dlfcn.h>

#include <stdio.h>
#include <glib.h>

#include "qyongplatforminputcontext.h"
#include "lcall.h"
#include "yong.h"

enum{
	APP_NORMAL=0,
	APP_HUNGRY,
};

static int check_app_type(void)
{
	int pid = getpid();
	char tstr0[64];
	char exec[256];
	int i;
	int type=APP_NORMAL;
	sprintf(tstr0, "/proc/%d/exe", pid);
	if ((i=readlink(tstr0, exec, sizeof(exec))) > 0)
	{
		exec[i]=0;
		char *prog=strrchr(exec,'/');
		if(prog!=NULL)
		{
			prog++;
		}
		if(!strcmp(prog,"qterminal") || strstr(exec,"wps-office"))
		{
			type=APP_HUNGRY;
		}
	}
	return type;
}

static gboolean _enable;
static GSList *_ctx_list;
static guint _ctx_id;
static QYongPlatformInputContext *_focus_ctx;
static int _trigger;
static gboolean _debug;

static void client_focus_in(guint id)
{
	l_call_client_call("focus_in",NULL,"i",id);
}

static void client_focus_out(guint id)
{
	l_call_client_call("focus_out",NULL,"i",id);
}

static void client_enable(guint id)
{
	l_call_client_call("enable",NULL,"i",id);
}

static void client_add_ic(guint id)
{
	l_call_client_call("add_ic",NULL,"i",id);
}

static void client_del_ic(guint id)
{
	l_call_client_call("del_ic",NULL,"i",id);
}

static void client_set_cursor_location(guint id,const QRect *area)
{
	l_call_client_call("cursor",NULL,"iiiii",id,area->x(),area->y(),area->width(),area->height());
}

static void client_set_cursor_location_relative(guint id,const QRect *area)
{
	l_call_client_call("cursor",NULL,"iiiiii",id,area->x(),area->y(),area->width(),area->height(),1);
}

static gboolean client_input_key(guint id,int key,guint32 time)
{
	int ret,res;
	ret=l_call_client_call("input",&res,"iii",id,key,time);
	if(ret!=0) return 0;
	return res?TRUE:FALSE;
}

static QYongPlatformInputContext *find_context(guint id)
{
	GSList *p;
	for(p=_ctx_list;p!=NULL;p=p->next)
	{
		QYongPlatformInputContext *ctx=(QYongPlatformInputContext*)p->data;
		if(ctx->id==id)
			return ctx;
	}
	return NULL;
}

static int GetKey_r(int yk)
{
	int vk;

	yk&=~KEYM_MASK;

	switch(yk){
	case YK_BACKSPACE:vk=Qt::Key_Backspace;break;
	case YK_DELETE:vk=Qt::Key_Delete;break;
	case YK_ENTER:vk=Qt::Key_Enter;break;
	case YK_HOME:vk=Qt::Key_Delete;break;
	case YK_END:vk=Qt::Key_Delete;break;
	case YK_PGUP:vk=Qt::Key_PageUp;break;
	case YK_PGDN:vk=Qt::Key_PageDown;break;
	case YK_LEFT:vk=Qt::Key_Left;break;
	case YK_DOWN:vk=Qt::Key_Right;break;
	case YK_UP:vk=Qt::Key_Up;break;
	case YK_RIGHT:vk=Qt::Key_Right;break;
	case YK_TAB:vk=Qt::Key_Delete;break;
	default:vk=yk;
	}
	return vk;
}

static void ForwardKey(QYongPlatformInputContext *ctx,int key)
{
	if(!ctx->client_window)
		return;
	ctx->key_ignore=1;
	int vk=GetKey_r(key);
	QKeyEvent *keyevent = ctx->createKeyEvent(vk, 0);
	QGuiApplication::sendEvent(ctx->client_window, keyevent);
	delete keyevent;
	keyevent = ctx->createKeyEvent(vk, 1);
	QGuiApplication::sendEvent(ctx->client_window, keyevent);
	delete keyevent;
	ctx->key_ignore=0;
}

static int client_dispatch(const char *name,LCallBuf *buf)
{
	if(!strcmp(name,"commit"))
	{
		guint id;
		int ret;
		char text[1024];
		QYongPlatformInputContext *ctx;
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		ret=l_call_buf_get_string(buf,text,sizeof(text));
		if(ret!=0) return -1;
		if(ctx->preedit_string)
		{
			g_free(ctx->preedit_string);
			ctx->preedit_string=NULL;
		}
		QInputMethodEvent e;
		QString s(text);
		e.setCommitString(s);
		QGuiApplication::sendEvent(ctx->client_window, &e);
		s.clear();

		if(ctx->update_cursor)
		{
			QTimer::singleShot(1, ctx, [ctx]() {
				ctx->cursorRectChanged();
			});
		}
	}
	else if(!strcmp(name,"preedit"))
	{
		guint id;
		int ret;
		QYongPlatformInputContext *ctx;
		char text[1024];
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		ret=l_call_buf_get_string(buf,text,sizeof(text));
		if(ret!=0) return -1;
		if(ctx->preedit_string)
		{
			if(!text[0])
			{
				g_free(ctx->preedit_string);
				ctx->preedit_string=NULL;
				ctx->skip_cursor=TRUE;
				ctx->update_preedit();
				ctx->skip_cursor=FALSE;
				//printf("preedit end\n");
			}
			else
			{
				g_free(ctx->preedit_string);
				ctx->preedit_string=g_strdup(text);
				ctx->update_preedit();
				//printf("preedit change\n");
			}
		}
		else
		{
			ctx->preedit_string=g_strdup(text);
			ctx->skip_cursor=TRUE;
			ctx->update_preedit();
			ctx->skip_cursor=FALSE;
			//printf("preedit start\n");
		}
	}
	else if(!strcmp(name,"preedit_clear"))
	{
		guint id;
		int ret;
		QYongPlatformInputContext *ctx;
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		if(ctx->preedit_string)
		{
			g_free(ctx->preedit_string);
			ctx->preedit_string=NULL;
			
			ctx->update_preedit();
		}
	}
	else if(!strcmp(name,"forward"))
	{
		guint id;
		int ret;
		QYongPlatformInputContext *ctx;
		int key;
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		ret=l_call_buf_get_val(buf,key);
		if(ret!=0) return -1;
		ForwardKey(ctx,key);
	}
	else if(!strcmp(name,"enable"))
	{
		_enable=TRUE;
	}
	else if(!strcmp(name,"disable"))
	{
		guint id;
		int ret;
		QYongPlatformInputContext *ctx;
		_enable=FALSE;
		ret=l_call_buf_get_val(buf,id);
		if(ret!=0) return -1;
		ctx=find_context(id);
		if(ctx==NULL) return -1;
		if(ctx->preedit_string)
		{
			g_free(ctx->preedit_string);
			ctx->preedit_string=NULL;
			ctx->update_preedit();
		}
	}
	else if(!strcmp(name,"trigger"))
	{
		int ret;
		ret=l_call_buf_get_val(buf,_trigger);
		if(ret!=0) return -1;
	}
	return 0;
}

static gboolean _set_cursor_location_internal(QYongPlatformInputContext *ctx)
{
    QRect area;

    if(ctx->client_window == NULL)
        return FALSE;

    area = ctx->cursor_area;

	if(ctx->is_wayland)
		client_set_cursor_location_relative(ctx->id,&area);
	else
		client_set_cursor_location(ctx->id,&area);
    return FALSE;
}

static gboolean _focus_in_internal(QYongPlatformInputContext *ctx)
{
	_set_cursor_location_internal(ctx);
	client_focus_in(ctx->id);
	return FALSE;
}

static void client_connect(void)
{
	QYongPlatformInputContext *ctx;
	GSList *p;
	if(_debug)
		printf("yong: connected\n");
	if(!_ctx_list)
		return;
	for(p=_ctx_list;p!=NULL;p=p->next)
	{
		ctx=(QYongPlatformInputContext*)p->data;
		if(ctx->has_focus)
		{
			if(_enable)
				client_enable(ctx->id);
			_focus_in_internal(ctx);
			return;
		}
	}
	if(_enable)
	{
		ctx=(QYongPlatformInputContext*)_ctx_list->data;
		client_enable(ctx->id);
	}
}

static int in_wayland(void)
{
	char *s=getenv("WAYLAND_DISPLAY");
	if(!s || !s[0])
		return 0;
	return 1;
}

QYongPlatformInputContext::QYongPlatformInputContext()
{
	//printf("QYongPlatformInputContext\n");
	
	if(getenv("Y_DEBUG_QT"))
		_debug=1;
	
	if(_ctx_id==0)
	{
		_enable=0;
		_ctx_id=1;
		_trigger=CTRL_SPACE;
		l_call_client_dispatch(client_dispatch);
		l_call_client_set_connect(client_connect);
	}

	key_ignore=0;
	client_window=NULL;
	has_focus=0;
	use_preedit=0;
	skip_cursor=0;
	//is_wayland=QGuiApplication::platformName() ==
    //               QLatin1String("wayland");
	//fprintf(stderr,"%s\n",qApp->platformName().toStdString().data() );
	is_wayland=in_wayland();
	app_type=check_app_type();
	update_cursor=app_type==APP_HUNGRY;
	preedit_string=NULL;
	id=_ctx_id++;

	//fprintf(stderr,"is_wayland %d\n",is_wayland);
	
	_ctx_list=g_slist_prepend(_ctx_list,this);
	client_add_ic(id);
}

QYongPlatformInputContext::~QYongPlatformInputContext()
{
	//printf("~QYongPlatformInputContext\n");
	g_free(preedit_string);
	_ctx_list=g_slist_remove(_ctx_list,this);
	client_del_ic(id);
}

bool QYongPlatformInputContext::isValid() const
{
	return true;
}

#ifdef FIX_WPS_QT
void QYongPlatformInputContext::reset(bool bCancel)
{
	QPlatformInputContext::reset(bCancel);
}
#else
void QYongPlatformInputContext::reset()
{
	//printf("reset\n");
	QPlatformInputContext::reset();
}
#endif

void QYongPlatformInputContext::commit()
{
	//printf("commit\n");
	QPlatformInputContext::commit();
}

void QYongPlatformInputContext::update(Qt::InputMethodQueries queries )
{
	// fprintf(stderr,"update\n");
	if (queries & Qt::ImCursorRectangle)
	{
        cursorRectChanged();
	}
}

void QYongPlatformInputContext::invokeAction(QInputMethod::Action action, int cursorPosition)
{
	//printf("invokeAction\n");
}

static int GetKey(int sym,int modifiers)
{
	char text[64];
	int res=0;
	int mask=0;
	
	switch(sym){
	case XKB_KEY_BackSpace:
	case XKB_KEY_Tab:
	case XKB_KEY_Return:
	case XKB_KEY_Escape:
	case XKB_KEY_Delete:
		res=sym&0xff;
		break;
	case XKB_KEY_Home:
	case XKB_KEY_Left:
	case XKB_KEY_Up:
	case XKB_KEY_Right:
	case XKB_KEY_Down:
	case XKB_KEY_Page_Up:
	case XKB_KEY_Page_Down:
	case XKB_KEY_End:
	case XKB_KEY_Insert:	
		res=sym;
		break;
	case XKB_KEY_Shift_L:
	case XKB_KEY_Shift_R:
	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R:
	case XKB_KEY_Alt_L:
	case XKB_KEY_Alt_R:
	case XKB_KEY_Super_L:
	case XKB_KEY_Super_R:
		res=sym&0xff;
		break;
	case XKB_KEY_F1 ... XKB_KEY_F12:
		res=sym;
		break;
	case XKB_KEY_KP_Space:
		res=KEYM_KEYPAD|YK_SPACE;
		break;
	case XKB_KEY_KP_Enter:
		res=KEYM_KEYPAD|YK_ENTER;
		break;
	case XKB_KEY_KP_Tab:
		res=KEYM_KEYPAD|YK_TAB;
		break;
	case XKB_KEY_KP_Subtract:
		res=KEYM_KEYPAD|'-';
		break;
	case XKB_KEY_KP_Add:
		res=KEYM_KEYPAD|'+';
		break;
	case XKB_KEY_KP_Multiply:
		res=KEYM_KEYPAD|'*';
		break;
	case XKB_KEY_KP_Divide:
		res=KEYM_KEYPAD|'/';
		break;
	case XKB_KEY_KP_Decimal:
		res=KEYM_KEYPAD|'.';
		break;
	case XKB_KEY_KP_Equal:
		res=KEYM_KEYPAD|'=';
		break;
	case XKB_KEY_KP_0 ... XKB_KEY_KP_9:
		res=KEYM_KEYPAD|(sym-XKB_KEY_KP_0+'0');
		break;
	case XKB_KEY_ISO_Left_Tab:
		res=KEYM_SHIFT|YK_TAB;
		break;
	default:
		if (xkb_keysym_to_utf8(sym, text, sizeof(text)) <= 0)
			return res;
		if(strlen(text)>1)
			return res;
		res=text[0];
		break;
	}

	if((modifiers&0x04) && res!=YK_LCTRL && res!=YK_RCTRL)
		mask|=KEYM_CTRL;
	if((modifiers&0x01) && res!=YK_LSHIFT && res!=YK_RSHIFT)
		mask|=KEYM_SHIFT;
	if((modifiers&0x08) && res!=YK_LALT && res!=YK_RALT)
		mask|=KEYM_ALT;
	if((modifiers&0x40))
		mask|=KEYM_SUPER;
	if((modifiers&0x02))
		mask|=KEYM_CAPS;
	
	if(mask)
		res=mask|toupper(res);
	return res;
}

bool QYongPlatformInputContext::filterEvent(const QEvent* event)
{
	//printf("filterEvent\n");
	do{
		int release;
		int res=FALSE;
		if(key_ignore)		
			break;
		if(event->type() == QEvent::KeyPress)
			release=0;
		else if(event->type() == QEvent::KeyRelease)
			release=1;
		else
			break;
		const QKeyEvent* keyEvent = static_cast<const QKeyEvent*>(event);
		int key=GetKey(keyEvent->nativeVirtualKey(),keyEvent->nativeModifiers());
		if(_debug)
			printf("yong: key %x %d\n",key,release);
		if(!key) break;
		if(update_cursor)
			this->cursorRectChanged();
		if(release) key|=KEYM_UP;
		if(!_enable)
		{
			if((key&~KEYM_CAPS)==_trigger && !release)
			{
				client_enable(this->id);
				_enable=TRUE;
				res=TRUE;
			}
		}
		else
		{
			if(key==_trigger) l_call_client_connect();
			res=client_input_key(this->id,key,keyEvent->timestamp());
		}
		if(res)
			return TRUE;
	}while(0);
	return QPlatformInputContext::filterEvent(event);
}

void QYongPlatformInputContext::setFocusObject(QObject* object)
{
	//printf("setFocusObject %p\n",object);
	//fprintf(stderr,"%s\n",qApp->platformName().toStdString().data());
	if (!inputMethodAccepted())
		return;
	if(object!=NULL)
	{
		this->is_wayland=qApp->platformName()=="wayland";
		_focus_ctx=this;
		client_window=object;
		client_focus_in(id);
		if(this->update_cursor)
			cursorRectChanged();
	}
	else
	{
		if(this==_focus_ctx)
			_focus_ctx=NULL;
		client_window=NULL;
		client_focus_out(id);
	}
}

void QYongPlatformInputContext::cursorRectChanged()
{
	fprintf(stderr,"cursorRectChanged\n");
	QWindow *inputWindow = qApp->focusWindow();
	if (!inputWindow)
	{
		fprintf(stderr,"no input window\n");
		return;
	}
	QRect r = qApp->inputMethod()->cursorRectangle().toRect();
	if(!r.isValid())
	{
		fprintf(stderr,"invalid cursor rectangle\n");
		return;
	}
	if(is_wayland)
	{
		auto margins = inputWindow->frameMargins();
   		r.translate(margins.left(), margins.top());
	}
	auto screenGeometry = inputWindow->screen()->geometry();

#if FIX_WPS_QT
	QObject *focusobj = qApp->focusObject();
	if(!focusobj)
	{
		// fprintf(stderr,"no focus obj\n");
		return;
	}
	if(QWidget *widget = qobject_cast<QWidget*>(focusobj))
	{
        r = widget->inputMethodQuery(Qt::ImCursorRectangle).toRect();
        r.moveTopLeft(widget->mapToGlobal(r.topLeft()));
    }
	else
#endif
	{
		r.moveTopLeft(inputWindow->mapToGlobal(r.topLeft()));
	}
	qreal scale = inputWindow->devicePixelRatio();
	// fprintf(stderr,"%d %d %lf\n",r.x(),r.y(),scale);
	cursor_area.setX(r.x()*scale+screenGeometry.left());
	cursor_area.setY(r.y()*scale+screenGeometry.top());
	cursor_area.setWidth(r.width()*scale);
	cursor_area.setHeight(r.height()*scale);
	if(is_wayland)
		client_set_cursor_location_relative(id,&cursor_area);
	else
		client_set_cursor_location(id,&cursor_area);
}

QKeyEvent* QYongPlatformInputContext::createKeyEvent(uint keyval, int release)
{
    QKeyEvent* keyevent = new QKeyEvent(
        release?QEvent::KeyRelease:QEvent::KeyPress,
        keyval,
        Qt::NoModifier
    );

    return keyevent;
}

void QYongPlatformInputContext::update_preedit()
{
	if(!client_window)
		return;

	QList<QInputMethodEvent::Attribute> attrList;
	QString str;
	QTextCharFormat format;
	format.setUnderlineStyle(QTextCharFormat::DashUnderline);
	
	if(!preedit_string || !preedit_string[0])
	{
		QInputMethodEvent event(str, attrList);
		QCoreApplication::sendEvent(client_window, &event);
		return;
	}
	str=preedit_string;
	attrList.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, 0, str.length(), format));
	QInputMethodEvent event(str, attrList);
	QCoreApplication::sendEvent(client_window, &event);
}
