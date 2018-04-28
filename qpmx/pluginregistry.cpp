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

PluginRegistry::PluginRegistry(QObject *parent) :
	QObject(parent),
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
		throw tr("Unable to find a plugin for provider %{bld}%1%{end}").arg(provider);

	auto instance = loader->instance();
	if(!instance) {
		throw tr("Failed to load plugin %{bld}%1%{end} with error: %2")
				.arg(loader->fileName(), loader->errorString());
	}

	srcPlg = qobject_cast<SourcePlugin*>(instance);
	if(!srcPlg)
		throw tr("Plugin %{bld}%1%{end} is not a qpmx::SourcePlugin").arg(loader->fileName());
	_loadCache.insert(provider, srcPlg);
	return srcPlg;
}

void PluginRegistry::cancelAll()
{
	for(const auto &loadedPlg : qAsConst(_loadCache))
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
	for(const auto &plg : plugDir.entryList()) {
		auto loader = new QPluginLoader(plugDir.absoluteFilePath(plg), this);
		auto meta = loader->metaData();
		if(meta[QStringLiteral("IID")].toString() == QStringLiteral(SourcePlugin_iid)) {
			auto data = meta[QStringLiteral("MetaData")].toObject();
			auto providers = data[QStringLiteral("Providers")].toArray();
			if(!providers.isEmpty()) {
				for(const auto provider : providers)
					_srcCache.insert(provider.toString(), loader);
				continue;
			} else
				xWarning() << tr("Found plugin without any providers: %1").arg(plg);
		}

		loader->deleteLater();
	}
}
