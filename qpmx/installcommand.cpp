#include "initcommand.h"
#include "installcommand.h"
#include <QDebug>
#include <QStandardPaths>
using namespace qpmx;

InstallCommand::InstallCommand(QObject *parent) :
	Command(parent),
	_renew(false),
	_noPrepare(false),
	_pkgList(),
	_addPkgCount(0),
	_pkgIndex(-1),
	_current(),
	_actionCache(),
	_resCache(),
	_connectCache()
{}

QString InstallCommand::commandName()
{
	return QStringLiteral("install");
}

QString InstallCommand::commandDescription()
{
	return tr("Install a qpmx package for the current project.");
}

QSharedPointer<QCliNode> InstallCommand::createCliNode()
{
	auto installNode = QSharedPointer<QCliLeaf>::create();
	installNode->addOption({
							   {QStringLiteral("r"), QStringLiteral("renew")},
							   tr("Force a reinstallation. If the package was already downloaded, "
								  "the existing sources and other artifacts will be deleted before downloading."),
						   });
	installNode->addOption({
							   {QStringLiteral("c"), QStringLiteral("cache")},
							   tr("Only download and cache the sources. Do not add the package to a qpmx.json."),
						   });
	installNode->addOption({
							   QStringLiteral("no-prepare"),
							   tr("Do not prepare pro-files if the qpmx.json file is newly created."),
						   });
	installNode->addPositionalArgument(QStringLiteral("packages"),
									   tr("The packages to be installed. The provider determines which backend to use for the download. "
										  "If left empty, all providers are searched for the package. If the version is left out, "
										  "the latest version is installed. If no packages are specified, the packages from the qpmx.json "
										  "file will be installed."),
									   QStringLiteral("[[<provider>::]<package>[@<version>] ...]"));
	return installNode;
}

void InstallCommand::initialize(QCliParser &parser)
{
	try {
		_renew = parser.isSet(QStringLiteral("renew"));
		_noPrepare = parser.isSet(QStringLiteral("no-prepare"));
		auto cacheOnly = parser.isSet(QStringLiteral("cache"));

		if(!parser.positionalArguments().isEmpty()) {
			xDebug() << tr("Installing %n package(s) from the command line", "", parser.positionalArguments().size());
			_pkgList = devDepList(readCliPackages(parser.positionalArguments()));
			if(!cacheOnly)
				_addPkgCount = _pkgList.size();
		} else {
			auto format = QpmxUserFormat::readDefault(true);
			_pkgList = format.allDeps();
			if(_pkgList.isEmpty()) {
				xWarning() << tr("No dependencies found in qpmx.json. Nothing will be done");
				qApp->quit();
				return;
			}
			if(!format.devDependencies.isEmpty())
				setDevMode(true);

			xDebug() << tr("Installing %n package(s) from qpmx.json file", "", _pkgList.size());
		}

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
			xDebug() << tr("Downloaded sources");
		else {
			xDebug() << tr("Downloaded sources from provider %{bld}%1%{end}")
						.arg(data.provider);
		}
		_resCache.append(data);
	}
	completeSource();
}

void InstallCommand::versionResult(int requestId, QVersionNumber version)
{
	auto data = _actionCache.take(requestId);
	if(!data)
		return;

	if(version.isNull()) {
		data.lock->free();
		auto str = tr("Package%2does not exist for provider %{bld}%1%{end}")
				   .arg(data.provider);
		if(data.mustWork)
			xCritical() << str.arg(tr(" %1 ").arg(_current.toString()));
		else {
			xDebug() << str.arg(tr(" "));
			completeSource();
		}
	} else {
		if(data.mustWork)
			xDebug() << tr("Fetched latest version as %1").arg(version.toString());
		else {
			xDebug() << tr("Fetched latest version as %1 from provider %{bld}%2%{end}")
						.arg(version.toString())
						.arg(data.provider);
		}

		_current.version = version;
		_pkgList[_pkgIndex] = _current;
		_resCache.append(data);
		completeSource();
	}
}

void InstallCommand::existsResult(int requestId)
{
	//scope to drop reference before completing
	{
		auto data = _actionCache.take(requestId);
		if(!data)
			return;
		_resCache.append(data);
	}
	completeSource();
}

void InstallCommand::sourceError(int requestId, const QString &error)
{
	auto data = _actionCache.take(requestId);
	if(!data)
		return;

	//remove any active locks, after error nothing more will happen
	data.lock->free();

	QString str;
	if(data.type == SrcAction::Install)
		str = tr("Failed to get sources%3from provider %{bld}%1%{end} with error:\n%2");
	else
		str = tr("Failed to fetch version%3from provider %{bld}%1%{end} with error:\n%2");
	str = str.arg(data.provider).arg(error);

	if(data.mustWork)
		xCritical() << str.arg(tr(" for %1 ").arg(_current.toString()));
	else {
		xWarning() << str.arg(tr(" "));
		completeSource();
	}
}

void InstallCommand::getNext()
{
	if(++_pkgIndex >= _pkgList.size()) {
		if(_addPkgCount > 0)
			completeInstall();
		else
			xDebug() << tr("Skipping add to qpmx.json, only cache installs");
		xDebug() << tr("Package installation completed");
		qApp->quit();
		return;
	}

	_current = _pkgList[_pkgIndex];
	if(_current.isDev() && !_current.isComplete())
		throw tr("dev dependencies cannot be used without a provider/version");

	if(_current.provider.isEmpty()) {
		auto allProvs = registry()->providerNames();
		auto any = false;
		foreach(auto prov, allProvs) {
			auto plugin = registry()->sourcePlugin(prov);
			if(plugin->packageValid(_current.pkg(prov))) {
				any = true;
				getSource(prov, plugin, false);
			}
		}

		if(!any) {
			throw tr("Unable to get sources for package %1: "
					 "Package is not valid for any provider")
					.arg(_current.toString());
		}
	} else {
		auto plugin = registry()->sourcePlugin(_current.provider);
		if(!plugin->packageValid(_current.pkg())) {
			throw tr("The package name %1 is not valid for provider %{bld}%2%{end}")
					.arg(_current.package)
					.arg(_current.provider);
		}
		getSource(_current.provider, plugin, true);
	}
}

void InstallCommand::getSource(QString provider, SourcePlugin *plugin, bool mustWork)
{
	//no version -> fetch first
	if(_current.version.isNull()) {
		xDebug() << tr("Searching for latest version of %1").arg(_current.toString());
		//use the latest version -> query for it
		auto id = randId(_actionCache);
		_actionCache.insert(id, {SrcAction::Version, provider, nullptr, mustWork, plugin});
		connectPlg(plugin);
		plugin->findPackageVersion(id, _current.pkg(provider));
		return;
	}

	//dev dep -> skip to completed
	if(_current.isDev()) {
		xInfo() << tr("Skipping download of dev dependency %1").arg(_current.toString());
		auto id = randId(_actionCache);
		_actionCache.insert(id, {SrcAction::Exists, provider, nullptr, mustWork, plugin}); //TODO lock anyways?
		QMetaObject::invokeMethod(this, "existsResult", Qt::QueuedConnection,
								  Q_ARG(int, id));
		return;
	}

	//aquire the lock for the package
	SharedCacheLock lock = srcLock(_current.pkg(provider));

	auto sDir = srcDir(_current.pkg(provider));
	if(sDir.exists()) {
		if(_renew || !sDir.exists(QStringLiteral("qpmx.json"))) //no qpmx.json -> remove and download again
			cleanCaches(_current.pkg(provider), lock);
		else {
			xDebug() << tr("Sources for package %1 already exist. Skipping download").arg(_current.toString());
			auto id = randId(_actionCache);
			_actionCache.insert(id, {SrcAction::Exists, provider, nullptr, mustWork, plugin, lock});
			QMetaObject::invokeMethod(this, "existsResult", Qt::QueuedConnection,
									  Q_ARG(int, id));
			return;
		}
	}

	auto tDir = new QTemporaryDir(tmpDir().absoluteFilePath(QStringLiteral("src.XXXXXX")));
	if(!tDir->isValid())
		throw tr("Failed to create temporary directory with error: %1").arg(tDir->errorString());

	xDebug() << tr("Gettings sources for package %1").arg(_current.toString());
	auto id = randId(_actionCache);
	_actionCache.insert(id, {SrcAction::Install, provider, tDir, mustWork, plugin, lock});
	connectPlg(plugin);
	plugin->getPackageSource(id, _current.pkg(provider), tDir->path());
	return;
}

void InstallCommand::completeSource()
{
	if(!_actionCache.isEmpty())
		return;

	try {
		if(_resCache.isEmpty())
			throw tr("Unable to find any provider for package %1").arg(_current.toString());
		else if(_resCache.size() > 1) {
			QStringList provList;
			foreach(auto data, _resCache)
				provList.append(data.provider);
			_resCache.clear();//to unlock
			throw tr("Found more then one provider for package %1. Providers are: %2")
					.arg(_current.toString())
					.arg(provList.join(tr(", ")));
		}

		auto data = _resCache.first();
		_resCache.clear();
		//store choosen provider!
		_current.provider = data.provider;
		_pkgList[_pkgIndex] = _current;

		QpmxFormat format;
		switch(data.type) {
		case SrcAction::Version:
			getSource(data.provider, data.plugin, data.mustWork);
			return;
		case SrcAction::Exists:
			format = QpmxFormat::readFile(srcDir(_current), true);
			break;
		case SrcAction::Install:
		{
			auto str = tr("Using provider %{bld}%1%{end}")
					   .arg(data.provider);
			xDebug() << str;

			//load the format from the temp dir
			format = QpmxFormat::readFile(data.tDir->path(), true);

			//move the sources to cache
			auto wp = data.tDir.toWeakRef();
			data.tDir->setAutoRemove(false);
			QFileInfo path = data.tDir->path();
			data.tDir.reset();
			Q_ASSERT(wp.isNull());

			auto tDir = srcDir(_current.provider, _current.package, {}, true);
			auto vSubDir = tDir.absoluteFilePath(_current.version.toString());
			if(!path.dir().rename(path.fileName(), vSubDir))
				throw tr("Failed to move downloaded sources of %1 from temporary directory to cache directory!").arg(_current.toString());
			xDebug() << tr("Moved sources to cache directory");
			xInfo() << tr("Installed package %1").arg(_current.toString());
			break;
		}
		default:
			Q_UNREACHABLE();
			break;
		}

		//create the src_include in the build dir
		createSrcInclude(format);
		//add new dependencies
		detectDeps(format);
		//unlock & next
		data.lock->free();
		getNext();
	} catch(QString &s) {
		_resCache.clear();
		xCritical() << s;
	}
}

void InstallCommand::completeInstall()
{
	auto prepare = false;
	if(!_noPrepare) {
		try {
			QpmxFormat::readDefault(true);
		} catch(QString &) {
			//file does not exist -> prepare if possible
			prepare = true;
		}
	}

	auto format = QpmxFormat::readDefault();
	foreach(auto pkg, _pkgList.mid(0, _addPkgCount)) {
		auto depIndex = format.dependencies.indexOf(pkg);
		if(depIndex == -1) {
			xDebug() << tr("Added package %1 to qpmx.json").arg(pkg.toString());
			format.dependencies.append(pkg);
		} else {
			xWarning() << tr("Package %1 is already a dependency. Replacing with that version")
						  .arg(pkg.toString());
			format.dependencies[depIndex] = pkg;
		}
	}
	QpmxFormat::writeDefault(format);
	xInfo() << "Added all packages to qpmx.json";

	if(prepare) {
		auto dir = QDir::current();
		dir.setFilter(QDir::Files);
		dir.setNameFilters({QStringLiteral("*.pro")});
		foreach(auto proFile, dir.entryList())
			InitCommand::prepare(proFile, true);
	}
}

void InstallCommand::connectPlg(SourcePlugin *plugin)
{
	if(_connectCache.contains(plugin))
	   return;

	auto plgobj = dynamic_cast<QObject*>(plugin);
	connect(plgobj, SIGNAL(sourceFetched(int)),
			this, SLOT(sourceFetched(int)),
			Qt::QueuedConnection);
	connect(plgobj, SIGNAL(versionResult(int,QVersionNumber)),
			this, SLOT(versionResult(int,QVersionNumber)),
			Qt::QueuedConnection);
	connect(plgobj, SIGNAL(sourceError(int,QString)),
			this, SLOT(sourceError(int,QString)),
			Qt::QueuedConnection);
	_connectCache.insert(plugin);
}

void InstallCommand::createSrcInclude(const QpmxFormat &format)
{
	auto _bl = buildLock(QStringLiteral("src"), _current);

	auto sDir = srcDir(_current);
	auto bDir = buildDir(QStringLiteral("src"), _current, true);

	QFile srcPriFile(bDir.absoluteFilePath(QStringLiteral("include.pri")));
	if(srcPriFile.exists()) {
		qDebug() << "source include.pri already exists. Skipping generation";
		return;
	}

	if(!srcPriFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to open src_include.pri with error: %1").arg(srcPriFile.errorString());
	QTextStream stream(&srcPriFile);
	stream << "!contains(QPMX_INCLUDE_GUARDS, \"" << _current.package << "\") {\n\n"
		   << "\tQPMX_INCLUDE_GUARDS += \"" << _current.package << "\"\n"
		   << "\t#dependencies\n";
	foreach(auto dep, format.dependencies) {
		auto depDir = buildDir(QStringLiteral("src"), dep);
		stream << "\tinclude(" << bDir.relativeFilePath(depDir.absoluteFilePath(QStringLiteral("include.pri"))) << ")\n";
	}
	auto srcIncPri = sDir.absoluteFilePath(format.priFile);
	if(!devMode())
		srcIncPri =  bDir.relativeFilePath(srcIncPri);
	stream << "\t#sources\n"
		   << "\tinclude(" << srcIncPri << ")\n"
		   << "}\n";
	stream.flush();
	srcPriFile.close();
	xInfo() << tr("Generated source include.pri");
}

void InstallCommand::detectDeps(const QpmxFormat &format)
{
	foreach(auto dep, format.dependencies) {
		auto dIndex = -1;
		do {
			dIndex = _pkgList.indexOf(dep, dIndex + 1);
			if(dIndex != -1 && _pkgList[dIndex].version == dep.version) { //fine here, as dependencies do not trigger "duplicate" warnings
				xDebug() << tr("Skipping dependency %1 as it is already in the install list").arg(dep.toString());
				break;
			}
		} while(dIndex != -1);
		if(dIndex == -1) {
			xDebug() << tr("Detected dependency to install: %1").arg(dep.toString());
			_pkgList.append(dep);
		}
	}
}



InstallCommand::SrcAction::SrcAction(ResType type, QString provider, QTemporaryDir *tDir, bool mustWork, SourcePlugin *plugin, SharedCacheLock lock) :
	type(type),
	provider(provider),
	tDir(tDir),
	mustWork(mustWork),
	plugin(plugin),
	lock(lock)
{}

InstallCommand::SrcAction::operator bool() const
{
	return plugin;
}
