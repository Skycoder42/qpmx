#include "compilecommand.h"
#include "qpmxformat.h"

#include <QProcess>
#include <QStandardPaths>
using namespace qpmx;

CompileCommand::CompileCommand(QObject *parent) :
	Command(parent),
	_global(false),
	_recompile(false),
	_settings(new QSettings(this)),
	_pkgList(),
	_qtKits()
{}

void CompileCommand::initialize(QCliParser &parser)
{
	try {
		_global = parser.isSet(QStringLiteral("global"));
		_recompile = parser.isSet(QStringLiteral("recompile"));

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

		initKits(parser.values(QStringLiteral("qmake")));

		qApp->quit();
	} catch(QString &s) {
		xCritical() << s;
	}
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
				allKits.removeAt(i);
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
