#include "compilecommand.h"
#include "qpmxformat.h"
#include "topsort.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QProcess>
#include <QQueue>
#include <QStandardPaths>
#include <QUrl>
using namespace qpmx;

CompileCommand::CompileCommand(QObject *parent) :
	Command(parent),
	_recompile(false),
	_fwdStderr(false),
	_clean(false),
	_pkgList(),
	_qtKits(),
#ifndef QPMX_NO_MAKEBUG
	_procEnv(),
#endif
	_current(),
	_kitIndex(-1),
	_kit(),
	_compileDir(nullptr),
	_format(),
	_stage(None),
	_process(nullptr),
	_hasBinary(true)
{}

QString CompileCommand::commandName()
{
	return QStringLiteral("compile");
}

QString CompileCommand::commandDescription()
{
	return tr("Compile or recompile source packages to generate the precompiled libraries "
			  "for faster building explicitly.");
}

QSharedPointer<QCliNode> CompileCommand::createCliNode()
{
	auto compileNode = QSharedPointer<QCliLeaf>::create();
	compileNode->addOption({
							   {QStringLiteral("m"), QStringLiteral("qmake")},
							   tr("The different <qmake> versions to generate binaries for. Can be specified "
								  "multiple times. Binaries are precompiled for every qmake in the list. If "
								  "not specified, binaries are generated for all known qmakes."),
							   tr("qmake")
						   });
	compileNode->addOption({
							   {QStringLiteral("g"), QStringLiteral("global")},
							   tr("Don't limit the packages to rebuild to only the ones specified in the qpmx.json. "
								  "Instead, build every ever cached package (Ignored if packages are specified as arguments)."),
						   });
	compileNode->addOption({
							   {QStringLiteral("r"), QStringLiteral("recompile")},
							   tr("By default, compilation is skipped for unchanged packages. By specifying "
								  "this flags, all (explicitly specified) packages are recompiled."),
						   });
	compileNode->addOption({
							   {QStringLiteral("e"), QStringLiteral("stderr")},
							   tr("Forward stderr of sub-processes like qmake to this processes stderr instead of "
								  "logging it to a file in the compile directory."),
						   });
	compileNode->addOption({
							   {QStringLiteral("c"), QStringLiteral("clean")},
							   tr("Generate clean builds for dev dependencies. This means instead of caching the build directory "
								  "of a dev dependency to speed up build, always start with a clean directory like for a normal build. "
								  "Has no effects for non dev dependencies."),
						   });
	compileNode->addPositionalArgument(QStringLiteral("packages"),
									   tr("The packages to compile binaries for. Installed packages are "
										  "matched against those, and binaries compiled for all of them. If no "
										  "packages are specified, all installed packages will be compiled."),
									   QStringLiteral("[<provider>::<package>@<version> ...]"));
	return compileNode;
}

void CompileCommand::initialize(QCliParser &parser)
{
	try {
		auto global = parser.isSet(QStringLiteral("global"));
		_recompile = parser.isSet(QStringLiteral("recompile"));
		_fwdStderr = parser.isSet(QStringLiteral("stderr"));
		_clean = parser.isSet(QStringLiteral("clean"));

		if(!parser.positionalArguments().isEmpty()) {
			xDebug() << tr("Compiling %n package(s) from the command line", "", parser.positionalArguments().size());
			_pkgList = devDepList(readCliPackages(parser.positionalArguments(), true));
			_explicitPkg = _pkgList;
		} else if(global){
			auto wDir = srcDir();
			auto flags = QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable;
			foreach(auto provider, wDir.entryList(flags)) {
				if(!wDir.cd(provider))
					continue;
				foreach(auto package, wDir.entryList(flags)) {
					if(!wDir.cd(package))
						continue;
					foreach(auto version, wDir.entryList(flags)) {
						PackageInfo pkg {provider, QUrl::fromPercentEncoding(package.toUtf8()), QVersionNumber::fromString(version)};
						_pkgList.append((QpmxDependency)pkg);
					}
					wDir.cdUp();
				}
				wDir.cdUp();
			}
			if(_pkgList.isEmpty()) {
				xInfo() << tr("No globally cached packages found. Nothing will be done");
				qApp->quit();
				return;
			}
			xDebug() << tr("Compiling all %n globally cached package(s)", "", _pkgList.size());
		} else {
			auto format = QpmxUserFormat::readDefault(true);
			if(format.source) {
				xInfo() << tr("qpmx.json has the sources flag set. No binaries will be compiled");
				qApp->quit();
				return;
			}

			_pkgList = format.allDeps();
			if(_pkgList.isEmpty()) {
				xWarning() << tr("No packages to compile found in qpmx.json. Nothing will be done");
				qApp->quit();
				return;
			}
			if(!format.devmode.isEmpty())
				setDevMode(true);

			xDebug() << tr("Compiling %n package(s) from qpmx.json file", "", _pkgList.size());
		}

		//collect all dependencies
		depCollect();
		//setup environment and qt kits
#ifndef QPMX_NO_MAKEBUG
		setupEnv();
#endif
		initKits(parser.values(QStringLiteral("qmake")));

		_kitIndex = _qtKits.size();//to trigger the loop
		compileNext();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void CompileCommand::finalize()
{
	if(_process) {
		_process->terminate();
		if(!_process->waitForFinished(2500)) {
			_process->kill();
			_process->waitForFinished(100);
		}
	}
}

void CompileCommand::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
	if(exitStatus == QProcess::CrashExit)
		errorOccurred(QProcess::Crashed);
	else {
		if(exitCode != EXIT_SUCCESS) {
			_compileDir->setAutoRemove(false);
			xCritical() << tr("Failed to run %1 step for %2 compilation with exit code %3. Check the error logs at \"%4\"")
						   .arg(stage())
						   .arg(_current.toString())
						   .arg(exitCode)
						   .arg(_compileDir->path());
			_process->deleteLater();
			_process = nullptr;
		} else
			makeStep();
	}
}

void CompileCommand::errorOccurred(QProcess::ProcessError error)
{
	Q_UNUSED(error)
	_compileDir->setAutoRemove(false);
	xCritical() << tr("Failed to run %1 step for %2 compilation. Error: %3")
				   .arg(stage())
				   .arg(_current.toString())
				   .arg(_process->errorString());
	_process->deleteLater();
	_process = nullptr;
}

void CompileCommand::compileNext()
{
	if(++_kitIndex >= _qtKits.size()) {
		if(_pkgList.isEmpty()) {
			xDebug() << tr("Package compilation completed");
			qApp->quit();
			return;
		}

		_kitIndex = 0;
		_current = _pkgList.takeFirst();
	}
	_kit = _qtKits[_kitIndex];

	//lock the package and kit
	buildLock(_kit.id, _current);

	//check if include.pri exists
	auto bDir = buildDir(_kit.id, _current);
	if(bDir.exists()) {
		if(_current.isDev() || //always recompile dev deps
		   !bDir.exists(QStringLiteral("include.pri")) || //no include.pri -> invalid -> delete and recompile
		   (_recompile && _explicitPkg.contains(_current))) { //only recompile explicitly specified (which is all except if passing as arguments)
			xInfo() << tr("Recompiling package %1 with qmake \"%2\"")
					   .arg(_current.toString())
					   .arg(_kit.path);
			if(!bDir.removeRecursively()) {
				throw tr("Failed to remove previous build of %1 with \"%2\"")
				.arg(_current.toString())
				.arg(_kit.path);
			}
			xDebug() << tr("Removed previous build of %1 with \"%2\"")
						.arg(_current.toString())
						.arg(_kit.path);
		} else {
			//done -> unlock
			buildUnlock(_kit.id, _current);
			xDebug() << tr("Package %1 already has compiled binaries for \"%2\"")
						.arg(_current.toString())
						.arg(_kit.path);
			compileNext();
			return;
		}
	} else {
		xInfo() << tr("Compiling package %1 with qmake \"%2\"")
				   .arg(_current.toString())
				   .arg(_kit.path);
	}

	//create temp dir and load qpmx.json
	if(_current.isDev() && !_clean) {
		buildLock(QStringLiteral("build"), _current);
		_compileDir.reset(new BuildDir(buildDir(QStringLiteral("build"), _current, true)));
	} else
		_compileDir.reset(new BuildDir());

	srcLock(_current);
	_format = QpmxFormat::readFile(srcDir(_current), true);
	_stage = None;
	if(_format.source)
		xWarning() << tr("Compiling a source-only package %1. This can lead to unexpected behaviour").arg(_current.toString());

	makeStep();
}

void CompileCommand::makeStep()
{
	try {
		switch (_stage) {
		case None:
			_stage = QMake;
			xDebug() << tr("Setting up build via qmake");
			qmake();
			break;
		case QMake:
			_stage = Make;
			xDebug() << tr("Completed setup. Continuing with compile (make)");
			make();
			break;
		case Make:
			_stage = Source;
			xDebug() << tr("Completed compile. Installing to cache directory");
			install();
			break;
		case Source:
			_stage = PriGen;
			priGen();
			xDebug() << tr("Completed installation. Compliation succeeded");
			//done -> unlock (both)
			if(_current.isDev() && !_clean)
				buildUnlock(QStringLiteral("build"), _current);
			srcUnlock(_current);
			buildUnlock(_kit.id, _current);
			compileNext();
			break;
		default:
			Q_UNREACHABLE();
			break;
		}
	} catch(QString &s) {
		xCritical() << s;
	}
}

void CompileCommand::qmake()
{
	// create pro file
	auto priBase = QFileInfo(_format.priFile).completeBaseName();
	auto proFile = _compileDir->filePath(QStringLiteral("static.pro"));

	//cleanup (in case of dev build) - no error check on purpose
	QFile::remove(proFile);
	QFile::remove(_compileDir->filePath(QStringLiteral(".qpmx_resources")));
	QFile::remove(_compileDir->filePath(QStringLiteral(".no_sources_detected")));
	//keep hooks file, will be regenerated on changes

	if(!QFile::copy(QStringLiteral(":/build/template_static.pro"), proFile))
		throw tr("Failed to create compilation pro file");
	QFile::setPermissions(proFile, QFile::permissions(proFile) | QFile::WriteUser);
	auto bDir = buildDir(_kit.id, _current, true);

	//create qmake.conf file
	QFile confFile(_compileDir->filePath(QStringLiteral(".qmake.conf")));
	if(!confFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to create qmake config with error: %1").arg(confFile.errorString());
	QTextStream stream(&confFile);
	stream << "QPMX_TARGET = " << priBase << "\n"
		   << "QPMX_VERSION = " << _current.version.toString() << "\n"
		   << "QPMX_PRI_INCLUDE = \"" << srcDir(_current).absoluteFilePath(_format.priFile) << "\"\n"
		   << "QPMX_INSTALL = \"" << bDir.absolutePath() << "\"\n"
		   << "QPMX_BIN = \"" << QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) << "\"\n"
		   << "TS_TMP = $$TRANSLATIONS\n\n";
	foreach(auto dep, _format.dependencies) {
		auto depDir = buildDir(_kit.id, dep);
		stream << "include(" << depDir.absoluteFilePath(QStringLiteral("include.pri")) << ")\n";
	}
	stream << "\nTRANSLATIONS = $$TS_TMP\n";

	stream.flush();
	confFile.close();

	initProcess();
	_process->setProgram(_kit.path);
	QStringList args;
	args.append(proFile);
	_process->setArguments(args);
	_process->start();
}

void CompileCommand::make()
{
	//check if anything is to be compiled
	if(QFile::exists(_compileDir->filePath(QStringLiteral(".no_sources_detected")))) {
		//skip to the install step, and cache information for generatePri
		xDebug() << tr("No sources to compile detected. skipping make step");
		_hasBinary = false;
		makeStep();
	} else {
		_hasBinary = true;
		//just run make
		initProcess();
		_process->setProgram(findMake());
		_process->setArguments({QStringLiteral("all")});
		_process->start();
	}
}

void CompileCommand::install()
{
	//just run make install
	initProcess();
	_process->setProgram(findMake());
	_process->setArguments({QStringLiteral("all-install")});
	_process->start();
}

void CompileCommand::priGen()
{
	auto bDir = buildDir(_kit.id, _current, true);

	//create include.pri file
	QFile metaFile(bDir.absoluteFilePath(QStringLiteral("include.pri")));
	if(!metaFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to create meta.pri with error: %1").arg(metaFile.errorString());
	auto libName = QFileInfo(_format.priFile).completeBaseName();
	QTextStream stream(&metaFile);
	stream << "!contains(QPMX_INCLUDE_GUARDS, \"" << _current.package << "\") {\n"
		   << "\tQPMX_INCLUDE_GUARDS += \"" << _current.package << "\"\n\n";
	stream << "\t#dependencies\n";
	foreach(auto dep, _format.dependencies) {
		auto depDir = buildDir(_kit.id, dep);
		stream << "\tinclude(" << bDir.relativeFilePath(depDir.absoluteFilePath(QStringLiteral("include.pri"))) << ")\n";
	}
	stream << "\n\t#includes\n"
		   << "\tINCLUDEPATH += \"$$PWD/include\"\n"
		   << "\texists($$PWD/translations): QPMX_TS_DIRS += \"$$PWD/translations\"\n";
	if(_hasBinary) {
		stream << "\n\t#lib\n"
			   << "\twin32:CONFIG(release, debug|release): LIBS += \"-L$$PWD/lib\" -l" << libName << "\n"
			   << "\twin32:CONFIG(debug, debug|release): LIBS += \"-L$$PWD/lib\" -l" << libName << "d\n"
			   << "\telse:unix: LIBS += \"-L$$PWD/lib\" -l" << libName << "\n\n"

			   << "\twin32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/lib/lib" << libName << ".a\n"
			   << "\telse:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/lib/lib" << libName << "d.a\n"
			   << "\telse:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/lib/" << libName << ".lib\n"
			   << "\telse:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/lib/" << libName << "d.lib\n"
			   << "\telse:unix: PRE_TARGETDEPS += $$PWD/lib/lib" << libName << ".a\n\n";
		//add startup hook (if needed)
		auto hooks = readMultiVar(_compileDir->filePath(QStringLiteral(".qpmx_startup_hooks")));
		if(!hooks.isEmpty())
			stream << "\tQPMX_STARTUP_HOOKS += \"" << hooks.join(QStringLiteral("\" \"")) << "\"\n";

		auto resources = readVar(_compileDir->filePath(QStringLiteral(".qpmx_resources")));
		if(!resources.isEmpty()) {
			stream << "\tQPMX_RESOURCE_FILES +=";
			foreach(auto res, resources)
				stream << " \"" << QFileInfo(res).completeBaseName() << "\"";
			stream << "\n";
		}
	}
	if(!_format.prcFile.isEmpty()) {
		stream << "\n\t#prc include\n"
			   << "\tinclude(" << bDir.relativeFilePath(srcDir(_current).absoluteFilePath(_format.prcFile)) << ")\n";
	}
	stream << "}\n";
	stream.flush();
	metaFile.close();
}

QString CompileCommand::stage()
{
	switch (_stage) {
	case None:
		return tr("none");
	case QMake:
		return tr("qmake");
	case Make:
		return tr("make");
	case Source:
		return tr("install");
	case PriGen:
		return tr("pri-generate");
	default:
		Q_UNREACHABLE();
		return {};
	}
}

void CompileCommand::depCollect()
{
	TopSort<QpmxDevDependency> sortHelper(_pkgList);

	QQueue<QpmxDevDependency> queue;
	foreach(auto pkg, _pkgList)
		queue.enqueue(pkg);

	while(!queue.isEmpty()) {
		auto pkg = queue.dequeue();
		srcLock(pkg);
		auto format = QpmxFormat::readFile(srcDir(pkg), true);
		srcUnlock(pkg);
		foreach(auto dep, format.dependencies) {
			if(!sortHelper.contains(dep)) {
				sortHelper.addData(dep);
				queue.enqueue(dep);
			}
			sortHelper.addDependency(pkg, dep);
		}
	}

	_pkgList = sortHelper.sort();
	if(_pkgList.isEmpty())
		throw tr("Cyclic dependencies detected! Unable to compile packages");
	if(_explicitPkg.isEmpty())
		_explicitPkg = _pkgList;
}

QString CompileCommand::findMake()
{
	QString make;

	if(make.isEmpty() && _kit.xspec.contains(QStringLiteral("msvc")))
		make = QStandardPaths::findExecutable(QStringLiteral("nmake"));
	if(make.isEmpty() && _kit.xspec == QStringLiteral("win32-g++"))
		make = QStandardPaths::findExecutable(QStringLiteral("mingw32-make"));

	if(make.isEmpty())
		make = QStandardPaths::findExecutable(QStringLiteral("make"));
	if(make.isEmpty())
		throw tr("Unable to find make executable. Make shure make can be found in your path");
	return make;
}

QStringList CompileCommand::readMultiVar(const QString &dirName, bool recursive)
{
	QDir dir(dirName);
	dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
	QDirIterator iter(dir,
					  recursive ?
						  (QDirIterator::Subdirectories | QDirIterator::FollowSymlinks) :
						  QDirIterator::NoIteratorFlags);

	QStringList resList;
	while(iter.hasNext()) {
		iter.next();
		resList.append(readVar(iter.filePath()));
	}
	return resList;
}

QStringList CompileCommand::readVar(const QString &fileName)
{
	QFile file(fileName);
	if(!file.exists())
		return {};

	if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
		throw tr("Failed to qmake result with error: %1").arg(file.errorString());
	QTextStream resStream(&file);
	QStringList resList;
	while(!resStream.atEnd())
		resList.append(resStream.readLine().trimmed());
	file.close();

	return resList;
}

void CompileCommand::initProcess()
{
	if(_process)
		_process->deleteLater();
	_process = new QProcess(this);
	_process->setWorkingDirectory(_compileDir->path());

	QString logBase;
	switch (_stage) {
	case CompileCommand::QMake:
		logBase = QStringLiteral("qmake");
		break;
	case CompileCommand::Make:
		logBase = QStringLiteral("make");
		break;
	case CompileCommand::Source:
		logBase = QStringLiteral("install");
		break;
	case CompileCommand::PriGen:
		logBase = QStringLiteral("generate");
		break;
	default:
		Q_UNREACHABLE();
		break;
	}
	_process->setStandardOutputFile(_compileDir->filePath(QStringLiteral("%1.stdout.log").arg(logBase)));
	if(_fwdStderr)
		_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
	else
		_process->setStandardErrorFile(_compileDir->filePath(QStringLiteral("%1.stderr.log").arg(logBase)));

#ifndef QPMX_NO_MAKEBUG
	_process->setProcessEnvironment(_procEnv);
#endif

	connect(_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this, &CompileCommand::finished,
			Qt::QueuedConnection);
	connect(_process, &QProcess::errorOccurred,
			this, &CompileCommand::errorOccurred,
			Qt::QueuedConnection);
}

#ifndef QPMX_NO_MAKEBUG
void CompileCommand::setupEnv()
{
	QRegularExpression regex(QStringLiteral(R"__(--jobserver-auth=\d+,\d+)__"), QRegularExpression::OptimizeOnFirstUsageOption);
	_procEnv = QProcessEnvironment::systemEnvironment();

	if(_procEnv.contains(QStringLiteral("MAKEFLAGS"))) {
		auto flags = _procEnv.value(QStringLiteral("MAKEFLAGS")).split(QLatin1Char(' '));
		auto removed = false;
		foreach(auto flag, flags) {
			if(regex.match(flag).hasMatch()) {
				flags.removeOne(flag);
				removed = true;
				break;
			}
		}

		if(removed)
			_procEnv.insert(QStringLiteral("MAKEFLAGS"), flags.join(QLatin1Char(' ')));
	}
}
#endif

void CompileCommand::initKits(const QStringList &qmakes)
{
	//read exising qmakes
	kitLock();
	auto allKits = QtKitInfo::readFromSettings(settings());

	//collect the kits to use, and ALWAYS update them!
	if(qmakes.isEmpty()) {
		// update all known kits
		QStringList pathsCache;
		for(auto i = 0; i < allKits.size(); i++) {
			auto nKit = updateKit(allKits[i], false);
			if(nKit) {
				allKits[i] = nKit;
				pathsCache.append(nKit.path);
			} else
				allKits.removeAt(i--);
		}

		//add system qmake, if valid and not already added
		auto qmakePath = QStandardPaths::findExecutable(QStringLiteral("qmake"));
		if(!qmakePath.isEmpty() && !pathsCache.contains(qmakePath)) {
			auto nKit = createKit(qmakePath);
			allKits.append(nKit);
			xDebug() << tr("Added qmake from path: \"%1\"").arg(nKit.path);
		}
		_qtKits = allKits;
	} else {
		foreach(auto qmake, qmakes) {
			//try to find a matching existing qmake config
			auto found = false;
			for(auto i = 0; i < allKits.size(); i++) {
				if(allKits[i].path == qmake) {
					found = true;
					allKits[i] = updateKit(allKits[i], true);
					_qtKits.append(allKits[i]);
					break;
				}
			}
			if(found)
				continue;

			//not found -> create one
			auto nKit = createKit(qmake);
			allKits.append(nKit);
			_qtKits.append(nKit);
			xDebug() << tr("Added qmake from commandline: \"%1\"").arg(nKit.path);
		}
	}

	if(_qtKits.isEmpty())
		throw tr("No qmake versions found! Explicitly set them via \"--qmake <path_to_qmake>\"");

	//save back all kits
	QtKitInfo::writeToSettings(settings(), allKits);
	kitUnlock();
}

QtKitInfo CompileCommand::createKit(const QString &qmakePath)
{
	if(!QFile::exists(qmakePath))
		throw tr("The qmake \"%1\" does not exist").arg(qmakePath);
	QtKitInfo kit(qmakePath);

	//run qmake-query to collect params
	QProcess proc(this);
	proc.setProgram(qmakePath);
	proc.setArguments({
						  QStringLiteral("-query"),
						  QStringLiteral("QMAKE_VERSION"),
						  QStringLiteral("-query"),
						  QStringLiteral("QT_VERSION"),
						  QStringLiteral("-query"),
						  QStringLiteral("QMAKE_SPEC"),
						  QStringLiteral("-query"),
						  QStringLiteral("QMAKE_XSPEC"),
						  QStringLiteral("-query"),
						  QStringLiteral("QT_HOST_PREFIX"),
						  QStringLiteral("-query"),
						  QStringLiteral("QT_INSTALL_PREFIX"),
						  QStringLiteral("-query"),
						  QStringLiteral("QT_SYSROOT")
					  });
	proc.start();
	if(!proc.waitForFinished(2500)) {
		proc.terminate();
		proc.waitForFinished(500);
		throw tr("Failed to run qmake \"%1\"").arg(qmakePath);
	}

	if(proc.exitStatus() == QProcess::CrashExit) {
		throw tr("qmake process for qmake \"%1\" crashed with error: %2")
				.arg(qmakePath)
				.arg(proc.errorString());
	}
	if(proc.exitCode() != EXIT_SUCCESS) {
		throw tr("qmake process for qmake \"%1\" failed with exit code: %2")
				.arg(qmakePath)
				.arg(proc.exitCode());
	}

	auto data = proc.readAllStandardOutput();
	auto results = data.split('\n');
	if(results.size() < 7)
		throw tr("qmake output for qmake \"%1\" is invalid (not a qt5 qmake?)").arg(qmakePath);

	//assing values
	QByteArrayList params;
	foreach(auto res, results)
		params.append(res.split(':').last());//contains always at least 1 element
	kit.qmakeVer = QVersionNumber::fromString(QString::fromUtf8(params[0]));
	kit.qtVer = QVersionNumber::fromString(QString::fromUtf8(params[1]));
	kit.spec = QString::fromUtf8(params[2]);
	kit.xspec = QString::fromUtf8(params[3]);
	kit.hostPrefix = QString::fromUtf8(params[4]);
	kit.installPrefix = QString::fromUtf8(params[5]);
	kit.sysRoot = QString::fromUtf8(params[6]);

	return kit;
}

QtKitInfo CompileCommand::updateKit(QtKitInfo oldKit, bool mustWork)
{
	try {
		auto newKit = createKit(oldKit.path);
		if(newKit == oldKit) {
			xDebug() << tr("Validated existing qmake configuration for \"%1\"").arg(oldKit.path);
			return oldKit;
		} else {
			xInfo() << tr("Updating existing qmake configuration for \"%1\"...").arg(oldKit.path);
			auto oDir = buildDir(oldKit.id);
			if(!oDir.removeRecursively())
				throw tr("Failed to remove build cache directory for \"%1\"").arg(oldKit.path);
			xDebug() << tr("Updated existing qmake configuration for \"%1\"").arg(oldKit.path);
			return newKit;
		}
	} catch (QString &s) {
		if(mustWork)
			throw;
		xWarning() << tr("Invalid qmake \"%1\" removed from qmake list. Error: %2")
					  .arg(oldKit.path)
					  .arg(s);
		return {};
	}
}



QtKitInfo::QtKitInfo(const QString &path) :
	id(QUuid::createUuid()),
	path(path),
	qmakeVer(),
	qtVer(),
	spec(),
	xspec(),
	hostPrefix(),
	installPrefix(),
	sysRoot()
{}

QList<QtKitInfo> QtKitInfo::readFromSettings(QSettings *settings)
{
	QList<QtKitInfo> allKits;
	auto kitCnt = settings->beginReadArray(QStringLiteral("qt-kits"));
	for(auto i = 0; i < kitCnt; i++) {
		settings->setArrayIndex(i);
		QtKitInfo info;
		info.id = settings->value(QStringLiteral("id"), info.id).toUuid();
		info.path = settings->value(QStringLiteral("path"), info.path).toString();
		info.qmakeVer = settings->value(QStringLiteral("qmakeVer"), QVariant::fromValue(info.qmakeVer)).value<QVersionNumber>();
		info.qtVer = settings->value(QStringLiteral("qtVer"), QVariant::fromValue(info.qtVer)).value<QVersionNumber>();
		info.spec = settings->value(QStringLiteral("spec"), info.spec).toString();
		info.xspec = settings->value(QStringLiteral("xspec"), info.xspec).toString();
		info.hostPrefix = settings->value(QStringLiteral("hostPrefix"), info.hostPrefix).toString();
		info.installPrefix = settings->value(QStringLiteral("installPrefix"), info.installPrefix).toString();
		info.sysRoot = settings->value(QStringLiteral("sysRoot"), info.sysRoot).toString();
		allKits.append(info);
	}
	settings->endArray();
	return allKits;
}

void QtKitInfo::writeToSettings(QSettings *settings, const QList<QtKitInfo> &kitInfos)
{
	auto kitCnt = kitInfos.size();
	settings->remove(QStringLiteral("qt-kits"));
	settings->beginWriteArray(QStringLiteral("qt-kits"), kitCnt);
	for(auto i = 0; i < kitCnt; i++) {
		settings->setArrayIndex(i);
		const auto &info = kitInfos[i];

		settings->setValue(QStringLiteral("id"), info.id);
		settings->setValue(QStringLiteral("path"), info.path);
		settings->setValue(QStringLiteral("qmakeVer"), QVariant::fromValue(info.qmakeVer));
		settings->setValue(QStringLiteral("qtVer"), QVariant::fromValue(info.qtVer));
		settings->setValue(QStringLiteral("spec"), info.spec);
		settings->setValue(QStringLiteral("xspec"), info.xspec);
		settings->setValue(QStringLiteral("hostPrefix"), info.hostPrefix);
		settings->setValue(QStringLiteral("installPrefix"), info.installPrefix);
		settings->setValue(QStringLiteral("sysRoot"), info.sysRoot);
	}
	settings->endArray();
}

QtKitInfo::operator bool() const
{
	return !id.isNull() &&
			!path.isEmpty();
}

bool QtKitInfo::operator ==(const QtKitInfo &other) const
{
	//id and path are skipped, as the only define identity, not equality
	return qmakeVer == other.qmakeVer &&
			qtVer == other.qtVer &&
			spec == other.spec &&
			xspec == other.xspec &&
			hostPrefix == other.hostPrefix &&
			installPrefix == other.installPrefix &&
			sysRoot == other.sysRoot;
}



BuildDir::BuildDir() :
	_tDir(),
	_pDir(_tDir.path())
{
	if(!_tDir.isValid())
		throw CompileCommand::tr("Failed to create temporary directory for compilation with error: %1").arg(_tDir.errorString());
}

BuildDir::BuildDir(const QDir &buildDir) :
	_tDir(),
	_pDir(buildDir)
{
	_tDir.setAutoRemove(false);
	_tDir.remove();
}

bool BuildDir::isValid() const
{
	return _pDir.exists();
}

void BuildDir::setAutoRemove(bool b)
{
	_tDir.setAutoRemove(b);
}

QString BuildDir::path() const
{
	return _pDir.absolutePath();
}

QString BuildDir::filePath(const QString &fileName) const
{
	return _pDir.absoluteFilePath(fileName);
}
