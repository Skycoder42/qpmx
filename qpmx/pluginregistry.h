#ifndef PLUGINREGISTRY_H
#define PLUGINREGISTRY_H

#include "sourceplugin.h"
#include "qpluginfactory.h"

class PluginRegistry : public QObject
{
	Q_OBJECT

public:
	explicit PluginRegistry(QObject *parent = nullptr);

	static PluginRegistry *instance();

	QStringList providerNames();
	qpmx::SourcePlugin *sourcePlugin(const QString &provider);

	void cancelAll();

private:
	QPluginFactory<qpmx::SourcePlugin> *_factory;
	QHash<QString, qpmx::SourcePlugin*> _loadCache;
};

#endif // PLUGINREGISTRY_H
