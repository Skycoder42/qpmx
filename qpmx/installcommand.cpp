#include "installcommand.h"
#include <QDebug>
#include <QStandardPaths>
using namespace qpmx;

InstallCommand::InstallCommand(QObject *parent) :
	Command(parent),
	_renew(false),
	_cacheOnly(false),
	_pkgList(),
	_pkgIndex(-1),
	_current(),
	_actionCache(),
	_resCache(),
	_connectCache()
{}

void InstallCommand::initialize(QCliParser &parser)
{
	try {
		_renew = parser.isSet(QStringLiteral("renew"));
		_cacheOnly = parser.isSet(QStringLiteral("cache"));

		if(!parser.positionalArguments().isEmpty()) {
			xDebug() << tr("Installing %n package(s) from the command line", "", parser.positionalArguments().size());
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
		} else {
			auto format = QpmxFormat::readDefault(true);
			_pkgList = format.dependencies;
			if(_pkgList.isEmpty()) {
				xWarning() << tr("No dependencies found in qpmx.json. Nothing will be done");
				qApp->quit();
				return;
			}
			_cacheOnly = true; //implicitly, because all sources to download are already a dependency
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
			xDebug() << tr("Downloaded sources for \"%1\"").arg(_current);
		else {
			xDebug() << tr("Downloaded sources for \"%1\" from provider \"%3\"")
						.arg(_current)
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
		auto str = tr("Package \"%1\" does not exist for provider \"%2\"")
				   .arg(_current)
				   .arg(data.provider);
		if(data.mustWork)
			xCritical() << str;
		else {
			xDebug() << str;
			completeSource();
		}
	} else {
		if(data.mustWork)
			xDebug() << tr("Fetched latest version for \"%1\"").arg(_current);
		else {
			xDebug() << tr("Fetched latest version for \"%1\" from provider \"%3\"")
						.arg(_current)
						.arg(data.provider);
		}

		_current.version = version;
		_pkgList[_pkgIndex] = _current;
		_resCache.append(data);
		completeSource();
	}
}

void InstallCommand::sourceError(int requestId, const QString &error)
{
	auto data = _actionCache.take(requestId);
	if(!data)
		return;

	auto str = tr("Failed to get sources for \"%1\" from provider \"%2\" with error: %3")
			   .arg(_current)
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
	if(++_pkgIndex >= _pkgList.size()) {
		if(!_cacheOnly)
			completeInstall();
		else
			xDebug() << tr("Skipping add to qpmx.json, only cache installs");
		xDebug() << tr("Package installation completed");
		qApp->quit();
		return;
	}

	_current = _pkgList[_pkgIndex];
	if(_current.provider.isEmpty()) {
		auto allProvs = registry()->providerNames();
		auto any = false;
		foreach(auto prov, allProvs) {
			auto plugin = registry()->sourcePlugin(prov);
			if(plugin->packageValid(_current.pkg(prov))) {
				any = true;
				if(getSource(prov, plugin, false))
					break;
			}
		}

		if(!any) {
			throw tr("Unable to get sources for package \"%1\": "
					 "Package is not valid for any provider")
					.arg(_current);
		}
	} else {
		auto plugin = registry()->sourcePlugin(_current.provider);
		if(!plugin->packageValid(_current.pkg())) {
			throw tr("The package name \"%1\" is not valid for provider \"%2\"")
					.arg(_current.package)
					.arg(_current.provider);
		}
		getSource(_current.provider, plugin, true);
	}
}

int InstallCommand::randId()
{
	int id;
	do {
		id = qrand();
	} while(_actionCache.contains(id));
	return id;
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

bool InstallCommand::getSource(QString provider, SourcePlugin *plugin, bool mustWork)
{
	if(_current.version.isNull()) {
		//use the latest version -> query for it
		auto id = randId();
		_actionCache.insert(id, {provider, nullptr, mustWork, plugin});
		connectPlg(plugin);
		plugin->findPackageVersion(id, _current.pkg(provider));
		return false;
	}

	auto sDir = srcDir(provider, _current.package, _current.version, false);
	if(sDir.exists()) {
		if(_renew) {
			xDebug() << tr("Deleting sources for \"%1\"").arg(_current);
			if(!sDir.removeRecursively())
				throw tr("Failed to remove old sources for \"%1\"").arg(_current);

			//remove compile dirs aswell
			auto bDir = buildDir();
			foreach(auto cmpDir, bDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)) {
				auto rDir = buildDir(cmpDir, provider, _current.package, _current.version, false);
				if(!rDir.removeRecursively())
					throw tr("Failed to remove old compilation cache for \"%1\"").arg(_current);
			}
		} else {
			xDebug() << tr("Sources for package \"%1\" already exist. Skipping download").arg(_current);
			getNext();
			return true;
		}
	}

	auto tDir = new QTemporaryDir(tmpDir().absoluteFilePath(QStringLiteral("src.XXXXXX")));
	if(!tDir->isValid())
		throw tr("Failed to create temporary directory with error: %1").arg(tDir->errorString());

	auto id = randId();
	_actionCache.insert(id, {provider, tDir, mustWork, plugin});
	connectPlg(plugin);
	plugin->getPackageSource(id, _current.pkg(provider), tDir->path());
	return false;
}

void InstallCommand::completeSource()
{
	if(!_actionCache.isEmpty())
		return;

	try {
		if(_resCache.isEmpty())
			throw tr("Unable to find a provider for package \"%1\"").arg(_current);
		else if(_resCache.size() > 1) {
			QStringList provList;
			foreach(auto data, _resCache)
				provList.append(data.provider);
			throw tr("Found more then one provider for package \"%1\".\nProviders are: %2")
					.arg(_current)
					.arg(provList.join(tr(", ")));
		}

		auto data = _resCache.first();
		_resCache.clear();
		//store choosen provider!
		_current.provider = data.provider;
		_pkgList[_pkgIndex] = _current;

		//no tDir means no download yet, only version check. thus, download!
		if(!data.tDir) {
			getSource(data.provider, data.plugin, data.mustWork);
			return;
		}

		auto str = tr("Using provider \"%1\" for package \"%2\"")
				   .arg(data.provider)
				   .arg(_current);
		if(data.mustWork)
			xDebug() << str;
		else
			xInfo() << str;

		//load the format from the temp dir
		auto format = QpmxFormat::readFile(data.tDir->path(), true);

		//move the sources to cache
		auto wp = data.tDir.toWeakRef();
		data.tDir->setAutoRemove(false);
		QFileInfo path = data.tDir->path();
		data.tDir.reset();
		Q_ASSERT(wp.isNull());

		auto tDir = srcDir(_current.provider, _current.package);
		auto vSubDir = tDir.absoluteFilePath(_current.version.toString());
		if(!path.dir().rename(path.fileName(), vSubDir))
			throw tr("Failed to move downloaded sources from temporary directory to cache directory!");
		xDebug() << tr("Moved sources for \"%1\" from \"%2\" to \"%3\"")
					.arg(_current)
					.arg(path.filePath())
					.arg(vSubDir);

		//create the src_include in the build dir
		createSrcInclude(format);

		//add new dependencies
		foreach(auto dep, format.dependencies) {
			auto dIndex = -1;
			do {
				dIndex = _pkgList.indexOf(dep, dIndex + 1);
				if(dIndex != -1 && _pkgList[dIndex].version == dep.version) {
					xDebug() << tr("Skipping dependency \"%1\" as it is already in the install list").arg(dep);
					break;
				}
			} while(dIndex != -1);
			if(dIndex == -1) {
				xDebug() << tr("Detected dependency to install as well: \"%1\"").arg(dep);
				_pkgList.append(dep);
			}
		}

		xInfo() << tr("Installed package \"%1\"").arg(_current);
		getNext();
	} catch(QString &s) {
		_resCache.clear();
		xCritical() << s;
	}
}

void InstallCommand::completeInstall()
{
	auto format = QpmxFormat::readDefault();
	foreach(auto pkg, _pkgList) {
		auto depIndex = format.dependencies.indexOf(pkg);
		if(depIndex == -1) {
			xDebug() << tr("Added package \"%1\" to qpmx.json").arg(pkg);
			format.dependencies.append(pkg);
		} else {
			xWarning() << tr("Package \"%1\" is already a dependency. Replacing with this version").arg(pkg);
			format.dependencies[depIndex] = pkg;
		}
	}
	QpmxFormat::writeDefault(format);
	xInfo() << "Added all packages to qpmx.json";
}

void InstallCommand::createSrcInclude(const QpmxFormat &format)
{
	auto sDir = srcDir(_current);
	auto bDir = buildDir(QStringLiteral("src"), _current);

	QFile srcPriFile(bDir.absoluteFilePath(QStringLiteral("include.pri")));
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
	stream << "\t#sources\n"
		   << "\tinclude(" << bDir.relativeFilePath(sDir.absoluteFilePath(format.priFile)) << ")\n"
		   << "}\n";
	stream.flush();
	srcPriFile.close();
}



InstallCommand::SrcAction::SrcAction(QString provider, QTemporaryDir *tDir, bool mustWork, SourcePlugin *plugin) :
	provider(provider),
	tDir(tDir),
	mustWork(mustWork),
	plugin(plugin)
{}

InstallCommand::SrcAction::operator bool() const
{
	return plugin;
}
