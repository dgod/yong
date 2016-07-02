#pragma once

#include <qpa/qplatforminputcontext.h>
#include <QWindow>
#include <glib.h>

class QYongPlatformInputContext : public QPlatformInputContext{
	Q_OBJECT;
public:
	QYongPlatformInputContext();
	virtual ~QYongPlatformInputContext();
	
	virtual bool isValid() const;
	
	virtual void reset();
	virtual void commit();
	virtual void update(Qt::InputMethodQueries quries );
	virtual void invokeAction(QInputMethod::Action , int cursorPosition);
	virtual bool filterEvent(const QEvent* event);
	virtual void setFocusObject(QObject* object);
	
	QKeyEvent* createKeyEvent(uint keyval, int type);	
	void update_preedit();
	
	guint id;
	QObject *client_window;
	int has_focus:1;
	int use_preedit:1;
	int skip_cursor:1;
	int key_ignore;
	QRect cursor_area;
	char *preedit_string;

public Q_SLOTS:
	void cursorRectChanged();
};
