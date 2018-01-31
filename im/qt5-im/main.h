#pragma once

#include <QtGui/qpa/qplatforminputcontextplugin_p.h>

#include "qyongplatforminputcontext.h"

class QYongPlatformInputContextPlugin : public QPlatformInputContextPlugin{
	Q_OBJECT
public:
	Q_PLUGIN_METADATA(IID QPlatformInputContextFactoryInterface_iid FILE "yong.json")
	QStringList keys() const;
	QYongPlatformInputContext *create(const QString& system, const QStringList& paramList);
};
