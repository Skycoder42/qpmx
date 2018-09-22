#include "qpmsourceplugin.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <qtcoawaitables.h>
#include <libqpmx.h>

QpmSourcePlugin::QpmSourcePlugin(QObject *parent) :
	QObject(parent),
	SourcePlugin(),
	_processCache(),
	_cachedDownloads()
{}

QpmSourcePlugin::~QpmSourcePlugin()
{
	cleanCaches();
}

bool QpmSourcePlugin::canSearch(const QString &provider) const
{
	Q_UNUSED(provider)
	return true;
}

bool QpmSourcePlugin::canPublish(const QString &provider) const
{
	Q_UNUSED(provider)
	return true;
}

QString QpmSourcePlugin::packageSyntax(const QString &provider) const
{
	if(provider == QStringLiteral("qpm"))
		return tr("<qpm_package_id>");
	else
		return {};
}

bool QpmSourcePlugin::packageValid(const qpmx::PackageInfo &package) const
{
	Q_UNUSED(package)
	return true;
}

QJsonObject QpmSourcePlugin::createPublisherInfo(const QString &provider)
{
	QFile console;
	if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
		throw qpmx::SourcePluginException{tr("Failed to access console with error: %1").arg(console.errorString())};

	QTextStream stream(&console);
	if(provider == QStringLiteral("qpm")) {
		qInfo().noquote() << tr("%{pkg}## Step 1:%{end} Run qpm initialization. You can leave the license and pri file parts empty:");
		qWarning().noquote() << tr("Do NOT generate boilerplate code!");

		auto proc = createProcess({QStringLiteral("init")}, true, false);
		proc->setProcessChannelMode(QProcess::ForwardedChannels);
		proc->setInputChannelMode(QProcess::ForwardedInputChannel);
		_processCache.insert(proc);
		auto res = QtCoroutine::await(proc);
		_processCache.remove(proc);
		proc->deleteLater();
		if(res != EXIT_SUCCESS)
			throw qpmx::SourcePluginException{tr("Failed to run qpm initialization step")};

		qInfo().noquote() << tr("%{pkg}## Step 2:%{end} Add additional data. You can now modify the qpm.json to provide extra data. Press <ENTER> once you are done...");
		console.readLine();

		QFile qpmFile(QStringLiteral("qpm.json"));
		if(!qpmFile.exists())
			throw qpmx::SourcePluginException{tr("qpm.json does not exist, unable to read data")};
		if(!qpmFile.open(QIODevice::ReadOnly | QIODevice::Text))
			throw qpmx::SourcePluginException{tr("Failed to open qpm.json file with error: %1").arg(qpmFile.errorString())};

		QJsonParseError error;
		auto qpmJson = QJsonDocument::fromJson(qpmFile.readAll(), &error).object();
		if(error.error != QJsonParseError::NoError)
			throw qpmx::SourcePluginException{tr("Failed to read qpm.json file with error: %1").arg(error.errorString())};

		qpmFile.close();
		if(!qpmFile.remove())
			qWarning().noquote() << tr("Failed to remove temporary qpm.json file");

		//remove unneeded fields
		qpmJson.remove(QStringLiteral("version"));
		qpmJson.remove(QStringLiteral("dependencies"));
		qpmJson.remove(QStringLiteral("license"));
		qpmJson.remove(QStringLiteral("pri_filename"));
		qpmJson.remove(QStringLiteral("priFilename"));

		return qpmJson;
	} else
		return {};
}

QStringList QpmSourcePlugin::searchPackage(const QString &provider, const QString &query)
{
	if(provider != QStringLiteral("qpm"))
		throw qpmx::SourcePluginException{tr("Unsupported provider \"%1\"").arg(provider)};

	QStringList arguments{
		QStringLiteral("search"),
		query
	};

	auto proc = createProcess(arguments, true);
	_processCache.insert(proc);
	qDebug().noquote() << tr("Running qpm search for query: %1").arg(query);
	auto res = QtCoroutine::await(proc);
	_processCache.remove(proc);
	proc->deleteLater();
	if(res != EXIT_SUCCESS)
		throw qpmx::SourcePluginException{formatProcError(tr("search qpm package"), proc)};

	qDebug().noquote() << tr("Parsing qpm search output");
	auto beginFound = false;
	QStringList results;
	while(!proc->atEnd()) {
		auto line = proc->readLine();
		if(!beginFound) {
			if(line.startsWith("-----"))
				beginFound = true;
		} else//read the line
			results.append(QString::fromUtf8(line.mid(0, line.indexOf(' ', 1)).trimmed()));
	}

	return results;
}

QVersionNumber QpmSourcePlugin::findPackageVersion(const qpmx::PackageInfo &package)
{
	if(package.provider() != QStringLiteral("qpm"))
		throw qpmx::SourcePluginException{tr("Unsupported provider \"%1\"").arg(package.provider())};

	//run qpm install, to a cached temp dir, only "safe" way
	auto dir = qpmx::tmpDir();
	QTemporaryDir tmpDir{dir.absoluteFilePath(QStringLiteral("qpm.XXXXXX"))};

	//run the install process
	QStringList arguments {
		QStringLiteral("install"),
		package.package()
	};

	auto proc = createProcess(arguments);
	proc->setWorkingDirectory(tmpDir.path());
	_processCache.insert(proc);
	qDebug().noquote() << tr("Running qpm install for qpm package %1 to find it's latest version").arg(package.package());
	auto res = QtCoroutine::await(proc);
	_processCache.remove(proc);
	proc->deleteLater();
	if(res != EXIT_SUCCESS)
		throw qpmx::SourcePluginException{formatProcError(tr("find qpm package version"), proc)};

	//validate install and extract version from qpm.json
	QDir tDir{tmpDir.path()};
	qDebug().noquote() << tr("Checking for qpm install result");
	QFile inFile{
		tDir.absoluteFilePath(QStringLiteral("vendor/%1/qpm.json")
							  .arg(package.package().replace(QLatin1Char('.'), QLatin1Char('/'))))
	};
	if(!inFile.exists())
		throw qpmx::SourcePluginException{formatProcError(tr("download qpm package"), proc)};
	qDebug().noquote() << tr("Extracting version from qpm.json");
	if(!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
		throw qpmx::SourcePluginException{tr("Failed to open qpm.json with error: %1").arg(inFile.errorString())};

	QJsonParseError error;
	auto doc = QJsonDocument::fromJson(inFile.readAll(), &error);
	if(error.error != QJsonParseError::NoError)
		throw qpmx::SourcePluginException{tr("Failed to read qpm.json with error: %1").arg(error.errorString())};
	inFile.close();

	auto qpmRoot = doc.object();
	auto versionObj = qpmRoot[QStringLiteral("version")].toObject();
	auto versionLabel = versionObj[QStringLiteral("label")].toString();
	auto version = QVersionNumber::fromString(versionLabel);

	if(!version.isNull())
		_cachedDownloads.insert({package.provider(), package.package(), version}, tDir.absolutePath());
	return version;
}

void QpmSourcePlugin::getPackageSource(const qpmx::PackageInfo &package, const QDir &targetDir)
{
	if(package.provider() != QStringLiteral("qpm"))
		throw qpmx::SourcePluginException{tr("Unsupported provider \"%1\"").arg(package.provider())};

	//check if sources already exist
	auto cacheDir = _cachedDownloads.value(package);
	if(!cacheDir.isNull()) {
		if(completeCopyInstall(package, targetDir, cacheDir)) {
			_cachedDownloads.remove(package);
			return;
		} else
			cleanCache(package);
	}

	//prepare for download
	auto subPath = QStringLiteral(".qpm");
	if(!targetDir.mkpath(subPath))
		throw qpmx::SourcePluginException{tr("Failed to create download directory")};

	auto qpmPkg = package.package();
	if(!package.version().isNull())
		qpmPkg += QLatin1Char('@') + package.version().toString();
	QStringList arguments{
		QStringLiteral("install"),
		qpmPkg
	};

	auto proc = createProcess(arguments);
	proc->setWorkingDirectory(targetDir.absoluteFilePath(subPath));
	_processCache.insert(proc);
	qDebug().noquote() << tr("Running qpm install for qpm package %1").arg(qpmPkg);
	auto res = QtCoroutine::await(proc);
	_processCache.remove(proc);
	proc->deleteLater();
	if(res != EXIT_SUCCESS)
		throw qpmx::SourcePluginException{formatProcError(tr("find qpm package version"), proc)};

	subPath = QStringLiteral(".qpm/vendor/%1")
			  .arg(package.package().replace(QLatin1Char('.'), QLatin1Char('/')));
	if(!targetDir.exists(QStringLiteral("%1/qpm.json").arg(subPath)))
		throw qpmx::SourcePluginException{formatProcError(tr("download qpm package"), proc)};

	qDebug().noquote() << tr("Successfully downloaded qpm sources");
	//move install data to a new tmp dir, delete the old one and then move back
	auto tDir = targetDir;
	QFileInfo tInfo{tDir.absolutePath()};
	QString nName = tInfo.fileName() + QStringLiteral(".mv");
	QDir nDir{tInfo.dir().absoluteFilePath(nName)};
	if(!tDir.rename(subPath, nDir.absolutePath()))
		throw qpmx::SourcePluginException{tr("Failed to move out sources from qpm download")};
	if(!tDir.removeRecursively())
		throw qpmx::SourcePluginException{tr("Failed to remove qpm junk data")};
	if(!tInfo.dir().rename(nName, tInfo.fileName()))
		throw qpmx::SourcePluginException{tr("Failed to move sources back to tmp dir")};

	//transform qpm.json
	qpmTransform(tDir);
}

void QpmSourcePlugin::publishPackage(const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo)
{
	if(provider != QStringLiteral("qpm"))
		throw qpmx::SourcePluginException{tr("Unsupported provider \"%1\"").arg(provider)};

	qInfo().noquote() << tr("%{pkg}## Step 1:%{end} Preparing publishing...");

	//read qpmx.json
	QFile qpmxFile{qpmxDir.absoluteFilePath(QStringLiteral("qpmx.json"))};
	if(!qpmxFile.exists())
		throw qpmx::SourcePluginException{tr("qpmx.json does not exist, unable to read data")};
	if(!qpmxFile.open(QIODevice::ReadOnly | QIODevice::Text))
		throw qpmx::SourcePluginException{tr("Failed to open qpmx.json file with error: %1").arg(qpmxFile.errorString())};
	QJsonParseError error;
	auto qpmxJson = QJsonDocument::fromJson(qpmxFile.readAll(), &error).object();
	if(error.error != QJsonParseError::NoError)
		throw qpmx::SourcePluginException{tr("Failed to read qpm.json file with error: %1").arg(error.errorString())};
	qpmxFile.close();

	//create the qpm file
	auto qpmJson = publisherInfo;
	QJsonObject vJson;
	vJson[QStringLiteral("label")] = version.toString();
	vJson[QStringLiteral("revision")] = QString();
	vJson[QStringLiteral("fingerprint")] = QString();
	qpmJson[QStringLiteral("version")] = vJson;

	QJsonArray dArray;
	for(auto dep : qpmxJson[QStringLiteral("dependencies")].toArray()) {
		auto depObj = dep.toObject();
		if(depObj[QStringLiteral("provider")].toString() != QStringLiteral("qpm"))
			throw qpmx::SourcePluginException{tr("A qpm package can only have qpm dependencies. Other providers are not allowed")};
		dArray.append(QStringLiteral("%1@%2")
					  .arg(depObj[QStringLiteral("package")].toString())
					  .arg(depObj[QStringLiteral("version")].toString()));
	}
	qpmJson[QStringLiteral("dependencies")] = dArray;

	if(!qpmJson.contains(QStringLiteral("license"))) {
		qpmJson[QStringLiteral("license")] = qpmxJson[QStringLiteral("license")]
				.toObject()[QStringLiteral("name")]
				.toString();
	}
	qpmJson[QStringLiteral("pri_filename")] = qpmxJson[QStringLiteral("priFile")].toString();

	//and write it
	QFile qpmFile(qpmxDir.absoluteFilePath(QStringLiteral("qpm.json")));
	if(!qpmFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw qpmx::SourcePluginException{tr("Failed to create qpm.json file with error: %1").arg(qpmFile.errorString())};
	qpmFile.write(QJsonDocument{qpmJson}.toJson(QJsonDocument::Indented));
	qpmFile.close();

	QFile console;
	if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
		throw qpmx::SourcePluginException{tr("Failed to access console with error: %1").arg(console.errorString())};
	qInfo().noquote() << tr("%{pkg}## Step 2:%{end} qpm file created. Please commit the changes and push it to the remote!");
	console.readLine();

	// publish via qpm
	qInfo().noquote() << tr("%{pkg}## Step 3:%{end} Publishing the package via qpm...");
	auto proc = createProcess({QStringLiteral("publish")}, true, false);
	proc->setWorkingDirectory(qpmxDir.absolutePath());
	proc->setProcessChannelMode(QProcess::ForwardedOutputChannel);
	proc->setInputChannelMode(QProcess::ForwardedInputChannel);
	_processCache.insert(proc);
	qDebug().noquote() << tr("Running qpm publish");
	QtCoroutine::await(proc);
	_processCache.remove(proc);
	qWarning().noquote() << proc->readAllStandardError();
	proc->deleteLater();
}

void QpmSourcePlugin::cancelAll(int timeout)
{
	auto procs = _processCache;
	_processCache.clear();

	for(auto proc : procs) {
		proc->disconnect();
		proc->terminate();
	}

	for(auto proc : procs) {
		auto startTime = QDateTime::currentMSecsSinceEpoch();
		if(!proc->waitForFinished(timeout)) {
			timeout = 1;
			proc->kill();
			proc->waitForFinished(100);
		} else {
			auto endTime = QDateTime::currentMSecsSinceEpoch();
			timeout = static_cast<int>(qMax(1ll, timeout - (endTime - startTime)));
		}
	}

	cleanCaches();
}

QProcess *QpmSourcePlugin::createProcess(const QStringList &arguments, bool keepStdout, bool timeout)
{
	auto proc = new QProcess{this};
	proc->setProgram(QStandardPaths::findExecutable(QStringLiteral("qpm")));
	if(!keepStdout)
		proc->setStandardOutputFile(QProcess::nullDevice());
	proc->setArguments(arguments);

	if(timeout) {
		//timeout after 120 seconds
		QTimer::singleShot(120000, this, [proc, this](){
			if(_processCache.contains(proc)) {
				proc->kill();
				if(!proc->waitForFinished(1000))
					proc->terminate();
			}
		});
	}

	return proc;
}

QString QpmSourcePlugin::formatProcError(const QString &type, QProcess *proc)
{
	if(proc->exitStatus() != QProcess::NormalExit) {
		return tr("Failed to %1 with process error: %2")
				.arg(type, proc->errorString());
	} else {
		auto res = tr("Failed to %1 with stderr:").arg(type);
		proc->setReadChannel(QProcess::StandardError);
		while(!proc->atEnd()) {
			auto line = QString::fromUtf8(proc->readLine()).trimmed();
			res.append(QStringLiteral("\n\t%1").arg(line));
		}
		return res;
	}
}

bool QpmSourcePlugin::completeCopyInstall(const qpmx::PackageInfo &package, QDir targetDir, QDir sourceDir)
{
	//verify cached sources, just in case
	auto subPath = QStringLiteral("vendor/%1")
				   .arg(package.package().replace(QLatin1Char('.'), QLatin1Char('/')));
	if(!sourceDir.exists(QStringLiteral("%1/qpm.json").arg(subPath)))
		return false;

	//move sources into place
	qDebug().noquote() << tr("Using cached qpm sources");
	if(!targetDir.removeRecursively())
		throw qpmx::SourcePluginException{tr("Failed to remove dummy directory")};
	if(!sourceDir.rename(subPath, targetDir.absolutePath()))
		throw qpmx::SourcePluginException{tr("Failed to move sources to tmp dir")};
	if(!sourceDir.removeRecursively())
		throw qpmx::SourcePluginException{tr("Failed to remove qpm junk data")};

	//transform qpm.json
	qpmTransform(targetDir);
	return true;
}

void QpmSourcePlugin::cleanCache(const qpmx::PackageInfo &package)
{
	if(_cachedDownloads.contains(package)) {
		QDir cDir(_cachedDownloads.take(package));
		if(cDir.exists() && !cDir.removeRecursively())
			qWarning().noquote() << tr("Failed to delete cached directory %1").arg(cDir.absolutePath());
	}
}

void QpmSourcePlugin::cleanCaches()
{
	for(const auto &dir : qAsConst(_cachedDownloads)) {
		QDir cDir{dir};
		if(cDir.exists() && !cDir.removeRecursively())
			qWarning().noquote() << tr("Failed to delete cached directory %1").arg(dir);
	}

	_cachedDownloads.clear();
}

void QpmSourcePlugin::qpmTransform(const QDir &tDir)
{
	QFile outFile(tDir.absoluteFilePath(QStringLiteral("qpmx.json")));
	if(!outFile.exists()) {
		qDebug().noquote() << tr("Transforming qpm.json to qpmx.json");
		QFile inFile(tDir.absoluteFilePath(QStringLiteral("qpm.json")));
		if(!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
			throw qpmx::SourcePluginException{tr("Failed to open qpm.json with error: %1").arg(inFile.errorString())};

		QJsonParseError error;
		auto doc = QJsonDocument::fromJson(inFile.readAll(), &error);
		if(error.error != QJsonParseError::NoError)
			throw qpmx::SourcePluginException{tr("Failed to read qpm.json with error: %1").arg(error.errorString())};
		inFile.close();
		auto qpmRoot = doc.object();

		QJsonArray depArray;
		for(auto dep : qpmRoot[QStringLiteral("dependencies")].toArray()) {
			auto nDep = dep.toString().split(QLatin1Char('@'));
			if(nDep.size() != 2) {
				qWarning().noquote() << tr("Skipping invalid qpm dependency \"%1\"").arg(dep.toString());
				continue;
			}
			QJsonObject qpmxDep;
			qpmxDep[QStringLiteral("provider")] = QStringLiteral("qpm");
			qpmxDep[QStringLiteral("package")] = nDep[0];
			qpmxDep[QStringLiteral("version")] = nDep[1];
			depArray.append(qpmxDep);
		}

		QJsonObject qpmxRoot;
		qpmxRoot[QStringLiteral("dependencies")] = depArray;
		if(qpmRoot.contains(QStringLiteral("pri_filename")))
			qpmxRoot[QStringLiteral("priFile")] = qpmRoot[QStringLiteral("pri_filename")];
		else
			qpmxRoot[QStringLiteral("priFile")] = qpmRoot[QStringLiteral("priFilename")];

		if(!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
			throw qpmx::SourcePluginException{tr("Failed to open qpmx.json with error: %1").arg(outFile.errorString())};
		outFile.write(QJsonDocument(qpmxRoot).toJson(QJsonDocument::Indented));
		outFile.close();
	}
}
