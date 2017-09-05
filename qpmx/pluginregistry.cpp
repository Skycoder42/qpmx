#include "pluginregistry.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonValue>
#include <QLibraryInfo>
#include <QTranslator>
#include <QCoreApplication>
#include "command.h"

PluginRegistry::PluginRegistry(QObject *parent) :
	QObject(parent)
{}

QStringList PluginRegistry::providerNames()
{
	initSrcCache();
	return _srcCache.keys();
}

qpmx::SourcePlugin *PluginRegistry::sourcePlugin(const QString &provider)
{
	initSrcCache();

	auto loader = _srcCache.value(provider);
	if(!loader)
		throw tr("Unable to find a plugin for provider \"%1\"").arg(provider);

	auto instance = loader->instance();
	if(!instance) {
		throw tr("Failed to load plugin \"%1\" with error: %2")
				.arg(loader->fileName())
				.arg(loader->errorString());
	}

	auto srcPlg = qobject_cast<qpmx::SourcePlugin*>(instance);
	if(!srcPlg)
		throw tr("Plugin \"%1\" is not a qpmx::SourcePlugin").arg(loader->fileName());
	return srcPlg;
}

void PluginRegistry::initSrcCache()
{
	if(!_srcCache.isEmpty())
		return;

#ifndef QT_NO_DEBUG
	QDir plugDir(QCoreApplication::applicationDirPath());
	if(!plugDir.cd(QStringLiteral("../plugins")))
		throw QStringLiteral("debug plugins are not available!");
#else
	QDir plugDir(QLibraryInfo::location(QLibraryInfo::PluginsPath));
#endif
	if(!plugDir.cd(QStringLiteral("qpmx"))) {
		xWarning() << tr("No \"qpmx\" folder found. No plugins will be loaded");
		return;
	}

	plugDir.setFilter(QDir::Files | QDir::Readable);
	foreach (auto plg, plugDir.entryList()) {
		auto loader = new QPluginLoader(plugDir.absoluteFilePath(plg), this);
		auto meta = loader->metaData();
		if(meta[QStringLiteral("IID")].toString() == QStringLiteral(SourcePlugin_iid)) {
			auto data = meta[QStringLiteral("MetaData")].toObject();
			auto providers = data[QStringLiteral("Providers")].toArray();
			if(!providers.isEmpty()) {
				foreach(auto provider, providers)
					_srcCache.insert(provider.toString(), loader);
				continue;
			} else
				xWarning() << tr("Found plugin without any providers: %1").arg(plg);
		}

		loader->deleteLater();
	}
}
