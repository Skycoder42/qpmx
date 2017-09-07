#include "compilecommand.h"
#include "qpmxformat.h"

#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
using namespace qpmx;

CompileCommand::CompileCommand(QObject *parent) :
	Command(parent),
	_recompile(false),
	_settings(new QSettings(this)),
	_pkgList(),
	_qtKits(),
	_current(),
	_kitIndex(-1),
	_kit(),
	_compileDir(nullptr),
	_format(),
	_stage(QMake),
	_process(nullptr)
{}

void CompileCommand::initialize(QCliParser &parser)
{
	try {
		auto global = parser.isSet(QStringLiteral("global"));
		_recompile = parser.isSet(QStringLiteral("recompile"));

		if(!parser.positionalArguments().isEmpty()) {
			xDebug() << tr("Compiling %n package(s) from the command line", "", parser.positionalArguments().size());
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
		} else if(global){
			auto wDir = srcDir();
			auto flags = QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable;
			foreach(auto provider, wDir.entryList(flags)) {
				if(!wDir.cd(provider))
					continue;
				foreach(auto package, wDir.entryList(flags)) {
					if(!wDir.cd(package))
						continue;
					qDebug() << QUrl::fromPercentEncoding(package.toUtf8());
					foreach(auto version, wDir.entryList(flags))
						_pkgList.append({provider, QUrl::fromPercentEncoding(package.toUtf8()), QVersionNumber::fromString(version)});
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
			auto format = QpmxFormat::readDefault(true);
			foreach(auto dep, format.dependencies) {
				if(!dep.source)
					_pkgList.append(dep.pkg());
				else
					xDebug() << tr("Skipping package \"%1\" as it's a source only dependency").arg(dep);
			}

			if(_pkgList.isEmpty()) {
				xWarning() << tr("No dependencies to compile found in qpmx.json. Nothing will be done");
				qApp->quit();
				return;
			}
			xDebug() << tr("Compiling %n package(s) from qpmx.json file", "", _pkgList.size());
		}

		initKits(parser.values(QStringLiteral("qmake")));
		_kitIndex = _qtKits.size();//to trigger the loop
		compileNext();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void CompileCommand::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
	if(exitStatus == QProcess::CrashExit)
		errorOccurred(QProcess::Crashed);
	else {
		if(exitCode != EXIT_SUCCESS) {
			_compileDir->setAutoRemove(false);
			xCritical() << tr("Failed to run qmake for \"%1\" compilation. Exit code %2. Check the error logs at \"%3\"")
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
	xCritical() << tr("Failed to run qmake for \"%1\" compilation. Error: %2")
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

	//check if include.pri exists
	auto bDir = buildDir(_current, _kit.id);
	if(bDir.exists(QStringLiteral("include.pri"))) {
		if(_recompile) {
			if(!bDir.removeRecursively()) {
				throw tr("Failed to remove previous build of \"%1\" with \"%2\"")
				.arg(_current.toString())
				.arg(_kit.path);
			}
			xDebug() << tr("Removed previous build of \"%1\" with \"%2\"")
						.arg(_current.toString())
						.arg(_kit.path);
		} else {
			xDebug() << tr("Package \"%1\" already has compiled binaries for \"%2\"")
						.arg(_current.toString())
						.arg(_kit.path);
			compileNext();
			return;
		}
	}

	//create temp dir and load qpmx.json
	_compileDir.reset(new QTemporaryDir());
	if(!_compileDir->isValid())
		throw tr("Failed to create temporary directory for compilation with error: %1").arg(_compileDir->errorString());
	_format = QpmxFormat::readFile(srcDir(_current), true);
	_stage = QMake;

	makeStep();
}

void CompileCommand::makeStep()
{
	try {
		switch (_stage) {
		case QMake:
			xDebug() << tr("Beginning compilation of \"%1\" with qmake \"%2\"")
						.arg(_current.toString())
						.arg(_kit.path);
			qmake();
			_stage = Make;
			break;
		case Make:
			xDebug() << tr("Completed qmake for \"%1\". Continuing with compile (make)")
						.arg(_current.toString());
			make();
			_stage = Install;
			break;
		case Install:
			xDebug() << tr("Completed compile (make) for \"%1\". Installing to cache directory")
						.arg(_current.toString());
			install();
			_stage = PriGen;
			break;
		case PriGen:
			priGen();
			xDebug() << tr("Completed installation for \"%1\"")
						.arg(_current.toString());
			xInfo() << tr("Successfully compiled \"%1\" with qmake \"%2\"")
					   .arg(_current.toString())
					   .arg(_kit.path);
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
	if(!QFile::copy(QStringLiteral(":/build/template_static.pro"), proFile))
		throw tr("Failed to create compilation pro file");
	auto bDir = buildDir(_current, _kit.id);

	//create qmake.conf file
	QFile confFile(_compileDir->filePath(QStringLiteral(".qmake.conf")));
	if(!confFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to create qmake config with error: \"%1\"").arg(confFile.errorString());
	QTextStream stream(&confFile);
	stream << "QPMX_TARGET=" << priBase << "\n"
		   << "QPMX_VERSION=" << _current.version().toString() << "\n"
		   << "QPMX_PRI_INCLUDE=\"" << srcDir(_current).absoluteFilePath(_format.priFile) << "\"\n"
		   << "QPMX_INSTALL_LIB=\"" << bDir.absoluteFilePath(QStringLiteral("lib")) << "\"\n"
		   << "QPMX_INSTALL_INC=\"" << bDir.absoluteFilePath(QStringLiteral("include")) << "\"\n";
	stream.flush();
	confFile.close();

	initProcess();
	_process->setProgram(_kit.path);
	_process->setArguments({
							   //TODO extra parameters via qpmx.json
							   proFile
						   });
	_process->setWorkingDirectory(_compileDir->path());
	_process->start();
}

void CompileCommand::make()
{
	//just run make
	initProcess();
	_process->setProgram(QStandardPaths::findExecutable(QStringLiteral("make")));
	_process->setWorkingDirectory(_compileDir->path());
	_process->start();
}

void CompileCommand::install()
{
	//just run make install
	initProcess();
	_process->setProgram(QStandardPaths::findExecutable(QStringLiteral("make")));
	_process->setArguments({QStringLiteral("install")});
	_process->setWorkingDirectory(_compileDir->path());
	_process->start();
}

void CompileCommand::priGen()
{
	auto bDir = buildDir(_current, _kit.id);

	//create meta.pri file
	QFile metaFile(bDir.absoluteFilePath(QStringLiteral("meta.pri")));
	if(!metaFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to create meta.pri with error: \"%1\"").arg(metaFile.errorString());
	QTextStream stream(&metaFile);
	if(!_format.prcFile.isEmpty())
		stream << "include(" << bDir.relativeFilePath(srcDir(_current).absoluteFilePath(_format.prcFile)) << ")\n";
	stream << "QPMX_LIBNAME=" << QFileInfo(_format.priFile).completeBaseName() << "\n";
	stream.flush();
	metaFile.close();

	//copy include.pri
	if(!QFile::copy(QStringLiteral(":/build/template_include.pri"),
					bDir.absoluteFilePath(QStringLiteral("include.pri"))))
		throw tr("Failed to library include file");
}

void CompileCommand::initKits(const QStringList &qmakes)
{
	QDir cfgDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
	cfgDir.mkpath(QStringLiteral("."));

	//read exising qmakes
	QList<QtKitInfo> allKits;
	auto kitCnt = _settings->beginReadArray(QStringLiteral("qt-kits"));
	for(auto i = 0; i < kitCnt; i++) {
		_settings->setArrayIndex(i);
		QtKitInfo info;
		info.id = _settings->value(QStringLiteral("id"), info.id).toUuid();
		info.path = _settings->value(QStringLiteral("path"), info.path).toString();
		info.qmakeVer = _settings->value(QStringLiteral("qmakeVer"), QVariant::fromValue(info.qmakeVer)).value<QVersionNumber>();
		info.qtVer = _settings->value(QStringLiteral("qtVer"), QVariant::fromValue(info.qtVer)).value<QVersionNumber>();
		info.spec = _settings->value(QStringLiteral("spec"), info.spec).toString();
		info.xspec = _settings->value(QStringLiteral("xspec"), info.xspec).toString();
		info.hostPrefix = _settings->value(QStringLiteral("hostPrefix"), info.hostPrefix).toString();
		info.installPrefix = _settings->value(QStringLiteral("installPrefix"), info.installPrefix).toString();
		info.sysRoot = _settings->value(QStringLiteral("sysRoot"), info.sysRoot).toString();
		allKits.append(info);
	}
	_settings->endArray();

	//collect the kits to use, and ALWAYS update them!
	if(qmakes.isEmpty()) {
		// no kits -> try to find qmakes!
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
	kitCnt = allKits.size();
	_settings->remove(QStringLiteral("qt-kits"));
	_settings->beginWriteArray(QStringLiteral("qt-kits"), kitCnt);
	for(auto i = 0; i < kitCnt; i++) {
		_settings->setArrayIndex(i);
		const auto &info = allKits[i];

		_settings->setValue(QStringLiteral("id"), info.id);
		_settings->setValue(QStringLiteral("path"), info.path);
		_settings->setValue(QStringLiteral("qmakeVer"), QVariant::fromValue(info.qmakeVer));
		_settings->setValue(QStringLiteral("qtVer"), QVariant::fromValue(info.qtVer));
		_settings->setValue(QStringLiteral("spec"), info.spec);
		_settings->setValue(QStringLiteral("xspec"), info.xspec);
		_settings->setValue(QStringLiteral("hostPrefix"), info.hostPrefix);
		_settings->setValue(QStringLiteral("installPrefix"), info.installPrefix);
		_settings->setValue(QStringLiteral("sysRoot"), info.sysRoot);
	}
	_settings->endArray();
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
		throw tr("Failed to run qmake with path \"%1\"").arg(qmakePath);
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
		throw tr("qmake output for qmake \"%1\" is invalid").arg(qmakePath);

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
			xDebug() << tr("Updated existing qmake configuration for \"%1\"").arg(oldKit.path);
			//TODO remove build cache dir!
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

void CompileCommand::initProcess()
{
	if(_process)
		_process->deleteLater();
	_process = new QProcess(this);

	QString logBase;
	switch (_stage) {
	case CompileCommand::QMake:
		logBase = QStringLiteral("qmake");
		break;
	case CompileCommand::Make:
		logBase = QStringLiteral("make");
		break;
	case CompileCommand::Install:
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
	_process->setStandardErrorFile(_compileDir->filePath(QStringLiteral("%1.stderr.log").arg(logBase)));

	connect(_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this, &CompileCommand::finished,
			Qt::QueuedConnection);
	connect(_process, &QProcess::errorOccurred,
			this, &CompileCommand::errorOccurred,
			Qt::QueuedConnection);
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