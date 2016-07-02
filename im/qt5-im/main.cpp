#include "main.h"

QStringList QYongPlatformInputContextPlugin::keys() const
{
	return QStringList(QStringLiteral("yong"));
}

QYongPlatformInputContext *QYongPlatformInputContextPlugin::create(const QString& system, const QStringList& paramList)
{
	Q_UNUSED(paramList);
	if (system.compare(system, QStringLiteral("yong"), Qt::CaseInsensitive) == 0)
		return new QYongPlatformInputContext();
	return 0;
}
