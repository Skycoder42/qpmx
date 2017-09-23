#include "pluginregistry.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonValue>
#include <QLibraryInfo>
#include <QTranslator>
#include <QCoreApplication>
#include "command.h"
using namespace qpmx;

Q_GLOBAL_STATIC(PluginRegistry, registry)

PluginRegistry::PluginRegistry() :
	QObject(),
	_srcCache(),
	_loadCache()
{}

PluginRegistry *PluginRegistry::instance()
{
	return registry;
}

QStringList PluginRegistry::providerNames()
{
	initSrcCache();
	return _srcCache.keys();
}

SourcePlugin *PluginRegistry::sourcePlugin(const QString &provider)
{
	auto srcPlg = _loadCache.value(provider, nullptr);
	if(srcPlg)
	   return srcPlg;

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

	srcPlg = qobject_cast<SourcePlugin*>(instance);
	if(!srcPlg)
		throw tr("Plugin \"%1\" is not a qpmx::SourcePlugin").arg(loader->fileName());
	_loadCache.insert(provider, srcPlg);
	return srcPlg;
}

void PluginRegistry::cancelAll()
{
	foreach(auto loadedPlg, _loadCache.values())
		loadedPlg->cancelAll(2500);
}

void PluginRegistry::initSrcCache()
{
	if(!_srcCache.isEmpty())
		return;

#ifndef QT_NO_DEBUG
	QDir plugDir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
	if(!plugDir.cd(QStringLiteral("../../plugins")))
#else
	if(!plugDir.cd(QStringLiteral("../plugins")))
#endif
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
