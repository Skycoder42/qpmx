#ifndef PLUGINREGISTRY_H
#define PLUGINREGISTRY_H

#include <QObject>
#include <QHash>
#include <QPluginLoader>

#include "sourceplugin.h"

class PluginRegistry : public QObject
{
	Q_OBJECT

public:
	explicit PluginRegistry(QObject *parent = nullptr);

	QStringList providerNames();
	qpmx::SourcePlugin *sourcePlugin(const QString &provider);

	void cancelAll();

private:
	QHash<QString, QPluginLoader*> _srcCache;
	QHash<QString, qpmx::SourcePlugin*> _loadCache;

	void initSrcCache();
};

#endif // PLUGINREGISTRY_H
