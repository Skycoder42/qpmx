#include "installcommand.h"
#include <QDebug>
#include <QStandardPaths>
#include <QUrl>
using namespace qpmx;

InstallCommand::InstallCommand(QObject *parent) :
	Command(parent),
	_pkgList(),
	_current(),
	_actionCache(),
	_resCache()
{}

void InstallCommand::initialize(const QCliParser &parser)
{
	try {
		auto regex = PackageInfo::packageRegexp();
		foreach(auto arg, parser.positionalArguments()) {
			auto match = regex.match(arg);
			if(!match.hasMatch())
				throw tr("Malformed package: \"%1\"").arg(arg);

			PackageInfo info(match.captured(1),
							 match.captured(2),
							 QVersionNumber::fromString(match.captured(3)));
			_pkgList.append(info);
			xDebug() << tr("Parsed package: \"%1\" at version %2 (Provider: %3)")
						.arg(info.package())
						.arg(info.version().toString())
						.arg(info.provider());
		}

		if(_pkgList.isEmpty())
			throw tr("You must specify at least one package!");

		getNext();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void InstallCommand::sourceFetched(int requestId)
{
	//scope to drop reference before completing
	{
		auto data = _actionCache.take(requestId);
		if(!data)
			return;

		if(data.mustWork)
			xDebug() << tr("Downloaded sources for \"%1\"").arg(_current.toString());
		else {
			xDebug() << tr("Downloaded sources for \"%1\" from provider \"%3\"")
						.arg(_current.toString())
						.arg(data.provider);
		}

		_resCache.append(data);
	}
	completeSource();
}

void InstallCommand::sourceError(int requestId, const QString &error)
{
	auto data = _actionCache.take(requestId);
	if(!data)
		return;

	auto str = tr("Failed to get sources for \"%1\" from provider \"%3\" with error: %4")
			   .arg(_current.toString())
			   .arg(data.provider)
			   .arg(error);
	if(data.mustWork)
		xCritical() << str;
	else {
		xDebug() << str;
		completeSource();
	}
}

void InstallCommand::getNext()
{
	if(_pkgList.isEmpty()) {
		xDebug() << tr("Package installation completed");
		qApp->quit();
		return;
	}

	_current = _pkgList.takeFirst();
	if(_current.provider().isEmpty()) {
		auto allProvs = registry()->providerNames();
		foreach(auto prov, allProvs) {
			auto plugin = registry()->sourcePlugin(prov);
			if(plugin->packageValid(_current))
				getSource(prov, plugin, false);
		}
	} else {
		auto plugin = registry()->sourcePlugin(_current.provider());
		if(!plugin->packageValid(_current)) {
			throw tr("The package name \"%1\" is not valid for provider \"%2\"")
					.arg(_current.package())
					.arg(_current.provider());
		}
		getSource(_current.provider(), plugin, true);
	}
}

void InstallCommand::getSource(QString provider, SourcePlugin *plugin, bool mustWork)
{
	int id;
	do {
		id = qrand();
	} while(_actionCache.contains(id));

	QDir tmpDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
	tmpDir.mkpath(QStringLiteral("tmp"));
	auto tDir = new QTemporaryDir(tmpDir.absoluteFilePath(QStringLiteral("tmp/src.XXXXXX")));
	if(!tDir->isValid())
		throw tr("Failed to create temporary directory with error: %1").arg(tDir->errorString());

	_actionCache.insert(id, {provider, tDir, mustWork, plugin});
	auto plgobj = dynamic_cast<QObject*>(plugin);
	connect(plgobj, SIGNAL(sourceFetched(int)),
			this, SLOT(sourceFetched(int)),
			Qt::QueuedConnection);
	connect(plgobj, SIGNAL(sourceError(int,QString)),
			this, SLOT(sourceError(int,QString)),
			Qt::QueuedConnection);

	plugin->getPackageSource(id, _current, tDir->path());
}

void InstallCommand::completeSource()
{
	if(!_actionCache.isEmpty())
		return;

	try {
		if(_resCache.isEmpty())
			throw tr("Unable to find a provider for package \"%1\"").arg(_current.toString());
		else if(_resCache.size() > 1) {
			QStringList provList;
			foreach(auto data, _resCache)
				provList.append(data.provider);
			throw tr("Found more then one provider for package \"%1\".\nProviders are: %2")
					.arg(_current.toString())
					.arg(provList.join(tr(", ")));
		}

		auto data = _resCache.first();
		_resCache.clear();

		auto str = tr("Using provider \"%1\" for package \"%2\"")
				   .arg(data.provider)
				   .arg(_current.toString());
		if(data.mustWork)
			xDebug() << str;
		else
			xInfo() << str;

		//move the sources to cache
		auto wp = data.tDir.toWeakRef();
		data.tDir->setAutoRemove(false);
		QFileInfo path = data.tDir->path();
		data.tDir.reset();
		Q_ASSERT(wp.isNull());

		QDir tDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
		auto subPath = QStringLiteral("src/%1/%2")
					   .arg(data.provider)
					   .arg(QString::fromUtf8(QUrl::toPercentEncoding(_current.package())));
		tDir.mkpath(subPath);
		if(!tDir.cd(subPath))
			throw tr("Failed to move downloaded sources from temporary directory to cache directory!");

		if(!path.dir().rename(path.fileName(), tDir.absoluteFilePath(_current.version().toString())))
			throw tr("Failed to move downloaded sources from temporary directory to cache directory!");
		xDebug() << tr("Moved sources for \"%1\" from \"%2\" to \"%3\"")
					.arg(_current.toString())
					.arg(path.filePath())
					.arg(tDir.absoluteFilePath(_current.version().toString()));
	} catch(QString &s) {
		_resCache.clear();
		xCritical() << s;
	}
}



InstallCommand::SrcAction::SrcAction(QString provider, QTemporaryDir *tDir, bool mustWork, SourcePlugin *plugin) :
	provider(provider),
	tDir(tDir),
	mustWork(mustWork),
	plugin(plugin)
{}

InstallCommand::SrcAction::operator bool() const
{
	return tDir && plugin;
}
