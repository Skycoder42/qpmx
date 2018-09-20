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
	QObject{parent},
	_factory{new QPluginFactory<SourcePlugin>{QStringLiteral("qpmx"), this}}
{}

PluginRegistry *PluginRegistry::instance()
{
	return registry;
}

QStringList PluginRegistry::providerNames()
{
	return _factory->allKeys();
}

SourcePlugin *PluginRegistry::sourcePlugin(const QString &provider)
{
	try {
		auto srcPlg = _loadCache.value(provider, nullptr);
		if(srcPlg)
		   return srcPlg;

		srcPlg = _factory->plugin(provider);
		if(!srcPlg)
			throw tr("No plugin found for provider: %{bld}%1%{end}").arg(provider);
		_loadCache.insert(provider, srcPlg);
		return srcPlg;
	} catch (QPluginLoadException &e) {
		throw QString::fromUtf8(e.what());
	}
}

void PluginRegistry::cancelAll()
{
	for(const auto &loadedPlg : qAsConst(_loadCache))
		loadedPlg->cancelAll(2500);
}
