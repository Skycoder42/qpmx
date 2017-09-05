#include "compilecommand.h"
#include "qpmxformat.h"

#include <QJsonDocument>
#include <QJsonSerializer>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
using namespace qpmx;

CompileCommand::CompileCommand(QObject *parent) :
	Command(parent),
	_global(false),
	_recompile(false),
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
	QFile kitsFile(cfgDir.absoluteFilePath(QStringLiteral("qt-kits.json")));
	if(kitsFile.exists()) {
		if(!kitsFile.open(QIODevice::ReadOnly | QIODevice::Text))
			throw tr("Failed to open qt-kits.json config file with error: %1").arg(kitsFile.errorString());

		try {
			QJsonSerializer ser;
			ser.addJsonTypeConverter(new VersionConverter());
			allKits = ser.deserializeFrom<QList<QtKitInfo>>(&kitsFile);
		} catch(QJsonSerializerException &e) {
			qDebug() << e.what();
			throw tr("qt-kits.json config file contains invalid data");
		}
	}

	//collect the kits to use, and ALWAYS update them!
	if(qmakes.isEmpty()) {
		// no kits -> try to find qmakes!
		if(allKits.isEmpty()) {
			auto qmakePath = QStandardPaths::findExecutable(QStringLiteral("qmake"));
			if(!qmakePath.isEmpty())
				allKits.append(createKit(qmakePath));
		} else {
			for(auto i = 0; i < allKits.size(); i++)
				allKits[i] = updateKit(allKits[i]);
		}
		_qtKits = allKits;
	} else {
		foreach(auto qmake, qmakes) {
			//try to find a matching existing qmake config
			auto found = false;
			for(auto i = 0; i < allKits.size(); i++) {
				if(allKits[i].path == qmake) {
					found = true;
					allKits[i] = updateKit(allKits[i]);
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
		}
	}

	if(_qtKits.isEmpty())
		throw tr("No qmake versions found! Explicitly set them via \"--qmake <path_to_qmake>\"");

	//save back all kits
	QSaveFile kitsSaveFile(kitsFile.fileName());
	if(!kitsSaveFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to open qt-kits.json config file with error: %1").arg(kitsFile.errorString());

	try {
		QJsonSerializer ser;
		ser.addJsonTypeConverter(new VersionConverter());
		//ser.serializeTo(&qpmxFile, data);
		auto json = ser.serialize(allKits);
		kitsSaveFile.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
	} catch(QJsonSerializerException &e) {
		qDebug() << e.what();
		throw tr("Failed to write qt-kits.json config file");
	}

	if(!kitsSaveFile.commit())
		throw tr("Failed to save qt-kits.json config file with error: %1").arg(kitsSaveFile.errorString());
}

QtKitInfo CompileCommand::createKit(const QString &qmakePath)
{
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

QtKitInfo CompileCommand::updateKit(QtKitInfo oldKit)
{
	auto newKit = createKit(oldKit.path);
	if(newKit == oldKit)
		return oldKit;
	else {
		//TODO remove build cache dir!
		return newKit;
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
	return !id.isNull();
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
