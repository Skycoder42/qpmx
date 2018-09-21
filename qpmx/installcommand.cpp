#include "initcommand.h"
#include "installcommand.h"
#include <QDebug>
#include <QStandardPaths>
#include <qtcoroutine.h>
using namespace qpmx;

InstallCommand::InstallCommand(QObject *parent) :
	Command{parent}
{}

QString InstallCommand::commandName() const
{
	return QStringLiteral("install");
}

QString InstallCommand::commandDescription() const
{
	return tr("Install a qpmx package for the current project.");
}

QSharedPointer<QCliNode> InstallCommand::createCliNode() const
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
			_aliases = format.devAliases;
			if(_pkgList.isEmpty()) {
				xWarning() << tr("No dependencies found in qpmx.json. Nothing will be done");
				qApp->quit();
				return;
			}
			if(format.hasDevOptions())
				setDevMode(true);

			xDebug() << tr("Installing %n package(s) from qpmx.json file", "", _pkgList.size());
		}

		getPackages();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void InstallCommand::getPackages()
{
	for(auto i = 0; i < _pkgList.size(); ++i) {
		auto &currentDep = _pkgList[i];
		if(currentDep.isDev() && !currentDep.isComplete())
			throw tr("dev dependencies cannot be used without a provider/version");

		// first: find the correct version if nothing but the name was specified
		// this will either yield a single package, set to currentDep, or fail in an exception
		if(currentDep.version.isNull() && currentDep.provider.isEmpty()) {
			const auto allProvs = registry()->providerNames();
			QList<QpmxDevDependency> foundDeps;
			QtCoroutine::awaitEach(allProvs, [this, currentDep, &foundDeps](const QString &prov) {
				auto plugin = registry()->sourcePlugin(prov);
				if(plugin->packageValid(currentDep.pkg(prov))) {
					auto cpDep = currentDep;
					cpDep.provider = prov;
					if(getVersion(cpDep, plugin, false))
						foundDeps.append(cpDep);
				}
			});
			verifyDeps(foundDeps, currentDep);
			currentDep = foundDeps.takeFirst();
		}

		// second: provider is not set (but version is)
		if(currentDep.provider.isEmpty()) {
			Q_ASSERT(!currentDep.version.isNull());
			const auto allProvs = registry()->providerNames();
			QList<QpmxDevDependency> foundDeps;
			QtCoroutine::awaitEach(allProvs, [this, currentDep, &foundDeps](const QString &prov) {
				auto plugin = registry()->sourcePlugin(prov);
				if(plugin->packageValid(currentDep.pkg(prov))) {
					auto cpDep = currentDep;
					cpDep.provider = prov;
					if(installPackage(cpDep, plugin, false))
						foundDeps.append(cpDep);
				}
			});
			verifyDeps(foundDeps, currentDep);
			currentDep = foundDeps.takeFirst();
		} else { // third: provider provider set, version may or may not be set
			Q_ASSERT(!currentDep.provider.isEmpty());
			auto plugin = registry()->sourcePlugin(currentDep.provider);
			if(!plugin->packageValid(currentDep.pkg())) {
				throw tr("The package name %1 is not valid for provider %{bld}%2%{end}")
						.arg(currentDep.package, currentDep.provider);
			}
			installPackage(currentDep, plugin, true);
		}
	}

	if(_addPkgCount > 0)
		;//TODO completeInstall();
	else
		xDebug() << tr("Skipping add to qpmx.json, only cache installs");
	xDebug() << tr("Package installation completed");
	qApp->quit();
	return;
}

bool InstallCommand::getVersion(QpmxDevDependency &current, SourcePlugin *plugin, bool mustWork)
{
	try {
		xDebug() << tr("Searching for latest version of %1").arg(current.toString());
		auto version = plugin->findPackageVersion(current.pkg());
		if(version.isNull()) {
			auto str = tr("Package%1does not exist for the given provider");
			if(mustWork)
				xCritical() << str.arg(tr(" %1 ").arg(current.toString()));
			else
				xDebug() << str.arg(tr(" "));
			return false;
		} else {
			xDebug() << tr("Fetched latest version as %1").arg(version.toString());
			current.version = version;
			return true;
		}
	} catch(SourcePluginException &e) {
		const auto str = tr("Failed to get version for package%1with error:");
		if(mustWork)
			xCritical() << str.arg(tr(" %1 ").arg(current.toString())) << e.what();
		else
			xWarning() << str.arg(tr(" ")) << e.what();
		return false;
	}
}

bool InstallCommand::installPackage(QpmxDevDependency &current, SourcePlugin *plugin, bool mustWork)
{
	CacheLock lock; // lock is acquired by getSource method
	if(!getSource(current, plugin, mustWork, lock))
		return false;

	Q_ASSERT(lock.isLocked());
	auto format = QpmxFormat::readFile(srcDir(current), true);
	//create the src_include in the build dir
	createSrcInclude(current, format, lock);
	//add new dependencies
	detectDeps(format);
	return true;
}

bool InstallCommand::getSource(QpmxDevDependency &current, SourcePlugin *plugin, bool mustWork, CacheLock &lock)
{
	//no version -> fetch first
	if(current.version.isNull()) {
		//unlock again, then check the version
		if(!getVersion(current, plugin, mustWork))
			return false;
		// if version was found, lock again and continue the install
	}

	//acquire the lock for the package
	lock = pkgLock(current);

	//dev dep -> skip to completed
	if(current.isDev()) {
		xInfo() << tr("Skipping download of dev dependency %1").arg(current.toString());
		return true;
	}

	auto sDir = srcDir(current);
	if(sDir.exists()) {
		if(_renew || !sDir.exists(QStringLiteral("qpmx.json"))) //no qpmx.json -> remove and download again
			cleanCaches(current.pkg(), lock);
		else {
			xDebug() << tr("Sources for package %1 already exist. Skipping download").arg(current.toString());
			return true;
		}
	}

	try {
		QTemporaryDir tDir {tmpDir().absoluteFilePath(QStringLiteral("src.XXXXXX"))};
		if(!tDir.isValid())
			throw tr("Failed to create temporary directory with error: %1").arg(tDir.errorString());
		xDebug() << tr("Gettings sources for package %1").arg(current.toString());
		plugin->getPackageSource(current.pkg(), tDir.path());

		//load the format from the temp dir
		auto format = QpmxFormat::readFile(tDir.path(), true);
		//move the sources to cache
		tDir.setAutoRemove(false);
		QFileInfo path = tDir.path();
		auto oDir = srcDir(current.provider, current.package, {}, true);
		auto vSubDir = oDir.absoluteFilePath(current.version.toString());
		if(!path.dir().rename(path.fileName(), vSubDir))
			throw tr("Failed to move downloaded sources of %1 from temporary directory to cache directory!").arg(current.toString());
		xDebug() << tr("Moved sources to cache directory");
		xInfo() << tr("Installed package %1").arg(current.toString());
		return true;
	} catch(SourcePluginException &e) {
		const auto str = tr("Failed to get sources for package%1with error:");
		if(mustWork)
			xCritical() << str.arg(tr(" %1 ").arg(current.toString())) << e.what();
		else
			xWarning() << str.arg(tr(" ")) << e.what();
		return false;
	}
}

void InstallCommand::createSrcInclude(const QpmxDevDependency &current, const QpmxFormat &format, const Command::CacheLock &lock)
{
	Q_ASSERT(lock.isLocked());
	auto sDir = srcDir(current);
	auto bDir = buildDir(QStringLiteral("src"), current, true);

	QFile srcPriFile{bDir.absoluteFilePath(QStringLiteral("include.pri"))};
	if(srcPriFile.exists()) {
		qDebug() << "source include.pri already exists. Skipping generation";
		return;
	}

	if(!srcPriFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to open src_include.pri with error: %1").arg(srcPriFile.errorString());
	QTextStream stream{&srcPriFile};
	stream << "!contains(QPMX_INCLUDE_GUARDS, \"" << current.package << "\") {\n\n"
		   << "\tQPMX_INCLUDE_GUARDS += \"" << current.package << "\"\n"
		   << "\t#dependencies\n";
	for(auto dep : format.dependencies) {
		// replace alias
		replaceAlias(dep, _aliases);
		// add dep
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

void InstallCommand::verifyDeps(const QList<QpmxDevDependency> &list, const QpmxDevDependency &base) const
{
	if(list.isEmpty())
		throw tr("Unable to find any provider for package %1").arg(base.toString());
	else if(list.size() > 1) {
		QStringList provList;
		for(const auto &data : qAsConst(_resCache))
			provList.append(data.provider);
		throw tr("Found more then one provider for package %1. Providers are: %2")
				.arg(base.toString(), provList.join(tr(", ")));
	}
}

void InstallCommand::detectDeps(const QpmxFormat &format)
{
	for(auto dep : format.dependencies) {
		// replace aliases
		replaceAlias(dep, _aliases);
		// check if needed
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

//void InstallCommand::completeInstall()
//{
//	auto prepare = false;
//	if(!_noPrepare) {
//		try {
//			QpmxFormat::readDefault(true);
//		} catch(QString &) {
//			//file does not exist -> prepare if possible
//			prepare = true;
//		}
//	}

//	auto format = QpmxFormat::readDefault();
//	for(const auto &pkg : _pkgList.mid(0, _addPkgCount))
//		format.putDependency(pkg);
//	QpmxFormat::writeDefault(format);
//	xInfo() << "Added all packages to qpmx.json";

//	if(prepare) {
//		auto dir = QDir::current();
//		dir.setFilter(QDir::Files);
//		dir.setNameFilters({QStringLiteral("*.pro")});
//		for(const auto &proFile : dir.entryList())
//			InitCommand::prepare(proFile, true);
//	}
//}



InstallCommand::SrcAction::SrcAction(ResType type, QString provider, QTemporaryDir *tDir, bool mustWork, SourcePlugin *plugin, const SharedCacheLock &lock) :
	type(type),
	provider(std::move(provider)),
	tDir(tDir),
	mustWork(mustWork),
	plugin(plugin),
	lock(lock)
{}

InstallCommand::SrcAction::operator bool() const
{
	return plugin;
}
