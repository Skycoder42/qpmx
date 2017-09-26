#include "qpmsourceplugin.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

QpmSourcePlugin::QpmSourcePlugin(QObject *parent) :
	QObject(parent),
	SourcePlugin()
{}

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

QJsonObject QpmSourcePlugin::createPublisherInfo(const QString &provider) const
{
	QFile console;
	if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
		throw tr("Failed to access console with error: %1").arg(console.errorString());

	QTextStream stream(&console);
	if(provider == QStringLiteral("qpm")) {
		qInfo().noquote() << tr("%{pkg}## Step 1:%{endpkg} Run qpm initialization. You can leave the license and pri file parts empty:");
		qWarning().noquote() << tr("Do NOT generate boilerplate code!");

		QProcess p;
		p.setProgram(QStringLiteral("qpm"));
		p.setArguments({QStringLiteral("init")});
		p.setProcessChannelMode(QProcess::ForwardedChannels);
		p.setInputChannelMode(QProcess::ForwardedInputChannel);
		p.start();
		p.waitForFinished(-1);

		if(p.exitStatus() != QProcess::NormalExit || p.exitCode() != EXIT_SUCCESS)
			throw tr("Failed to run qpm initialization step");

		qInfo().noquote() << tr("%{pkg}## Step 2:%{endpkg} Add additional data. You can now modify the qpm.json to provide extra data. Press <ENTER> once you are done...");
		console.readLine();

		QFile qpmFile(QStringLiteral("qpm.json"));
		if(!qpmFile.exists())
			throw tr("qpm.json does not exist, unable to read data");
		if(!qpmFile.open(QIODevice::ReadOnly | QIODevice::Text))
			throw tr("Failed to open qpm.json file with error: %1").arg(qpmFile.errorString());

		QJsonParseError error;
		auto qpmJson = QJsonDocument::fromJson(qpmFile.readAll(), &error).object();
		if(error.error != QJsonParseError::NoError)
			throw tr("Failed to read qpm.json file with error: %1").arg(error.errorString());

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

void QpmSourcePlugin::cancelAll(int timeout)
{
	auto procs = _processCache.keys();
	_processCache.clear();

	foreach(auto proc, procs) {
		proc->disconnect();
		proc->terminate();
	}

	foreach(auto proc, procs) {
		auto startTime = QDateTime::currentMSecsSinceEpoch();
		if(!proc->waitForFinished(timeout)) {
			timeout = 1;
			proc->kill();
			proc->waitForFinished(100);
		} else {
			auto endTime = QDateTime::currentMSecsSinceEpoch();
			timeout = qMax(1ll, timeout - (endTime - startTime));
		}
	}
}

void QpmSourcePlugin::searchPackage(int requestId, const QString &provider, const QString &query)
{
	try {
		if(provider != QStringLiteral("qpm"))
			throw tr("Unsupported provider \"%1\"").arg(provider);

		QStringList arguments{
								  QStringLiteral("search"),
								  query
							  };

		auto proc = createProcess(QStringLiteral("search"), arguments);
		_processCache.insert(proc, tpl{requestId, Search, {}});
		proc->start();
	} catch (QString &s) {
		emit sourceError(requestId, s);
	}
}

void QpmSourcePlugin::findPackageVersion(int requestId, const qpmx::PackageInfo &package)
{
	try {
		if(package.provider() != QStringLiteral("qpm"))
			throw tr("Unsupported provider \"%1\"").arg(package.provider());

		QStringList arguments{
								  QStringLiteral("search"),
								  package.package()
							  };

		auto proc = createProcess(QStringLiteral("search"), arguments);
		_processCache.insert(proc, tpl{requestId, Version, {}});
		proc->start();
	} catch (QString &s) {
		emit sourceError(requestId, s);
	}
}

void QpmSourcePlugin::getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir)
{
	try {
		if(package.provider() != QStringLiteral("qpm"))
			throw tr("Unsupported provider \"%1\"").arg(package.provider());
		auto subPath = QStringLiteral(".qpm");
		if(!targetDir.mkpath(subPath))
			throw tr("Failed to create download directory");

		QStringList arguments{
								  QStringLiteral("install"),
								  package.package()
							  };

		QVariantHash params;
		params.insert(QStringLiteral("dir"), targetDir.absolutePath());
		params.insert(QStringLiteral("package"), package.package());
		auto proc = createProcess(QStringLiteral("install"), arguments);
		proc->setWorkingDirectory(targetDir.absoluteFilePath(subPath));
		_processCache.insert(proc, tpl{requestId, Install, params});
		proc->start();
	} catch (QString &s) {
		emit sourceError(requestId, s);
	}
}

void QpmSourcePlugin::publishPackage(int requestId, const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo)
{

}

void QpmSourcePlugin::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
	Q_UNUSED(exitCode)
	if(exitStatus == QProcess::CrashExit)
		errorOccurred(QProcess::Crashed);
	else {
		auto proc = qobject_cast<QProcess*>(sender());
		auto data = _processCache.value(proc, tpl{-1, Search, {}});
		if(std::get<0>(data) != -1) {
			_processCache.remove(proc);
			switch (std::get<1>(data)) {
			case Search:
				completeSearch(std::get<0>(data), proc);
				break;
			case Version:
				completeVersion(std::get<0>(data), proc);
				break;
			case Install:
				completeInstall(std::get<0>(data), proc, std::get<2>(data));
				break;
			default:
				Q_UNREACHABLE();
				break;
			}
		}
		proc->deleteLater();
	}
}

void QpmSourcePlugin::errorOccurred(QProcess::ProcessError error)
{
	Q_UNUSED(error)
	auto proc = qobject_cast<QProcess*>(sender());
	auto data = _processCache.value(proc, tpl{-1, Search, {}});
	if(std::get<0>(data) != -1) {
		_processCache.remove(proc);
		switch (std::get<1>(data)) {
		case Search:
			emit sourceError(std::get<0>(data),
							 tr("Failed to search qpm package with process error: %1")
							 .arg(proc->errorString()));
			break;
		case Version:
			emit sourceError(std::get<0>(data),
							 tr("Failed to find qpm package version with process error: %1")
							 .arg(proc->errorString()));
			break;
		case Install:
			emit sourceError(std::get<0>(data),
							 tr("Failed to download qpm package with process error: %1")
							 .arg(proc->errorString()));
			break;
		default:
			Q_UNREACHABLE();
			break;
		}
	}
	proc->deleteLater();
}

QDir QpmSourcePlugin::createLogDir(const QString &action)
{
	auto subPath = QStringLiteral("qpmx.logs/%1").arg(action);
	QDir pDir(QDir::temp());
	pDir.mkpath(subPath);
	if(!pDir.cd(subPath))
		throw tr("Failed to create log directory \"%1\"").arg(pDir.absolutePath());

	QTemporaryDir tDir(pDir.absoluteFilePath(QStringLiteral("XXXXXX")));
	tDir.setAutoRemove(false);
	if(tDir.isValid())
		return tDir.path();
	else
		throw tr("Failed to create log directory \"%1\"").arg(tDir.path());
}

QProcess *QpmSourcePlugin::createProcess(const QString &type, const QStringList &arguments, bool stdLog)
{
	auto logDir = createLogDir(type);

	auto proc = new QProcess(this);
	proc->setProgram(QStandardPaths::findExecutable(QStringLiteral("qpm")));
	if(stdLog)
		proc->setStandardOutputFile(logDir.absoluteFilePath(QStringLiteral("stdout.log")));
	proc->setStandardErrorFile(logDir.absoluteFilePath(QStringLiteral("stderr.log")));
	proc->setArguments(arguments);
	proc->setProperty("logDir", logDir.absolutePath());

	connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this, &QpmSourcePlugin::finished,
			Qt::QueuedConnection);
	connect(proc, &QProcess::errorOccurred,
			this, &QpmSourcePlugin::errorOccurred,
			Qt::QueuedConnection);

	//timeout after 30 seconds
	QTimer::singleShot(30000, this, [proc, this](){
		if(_processCache.contains(proc)) {
			proc->kill();
			if(!proc->waitForFinished(1000))
				proc->terminate();
		}
	});

	return proc;
}

void QpmSourcePlugin::completeSearch(int id, QProcess *proc)
{
	//parse output
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

	emit searchResult(id, results);
}

void QpmSourcePlugin::completeVersion(int id, QProcess *proc)
{
	QVersionNumber version;

	//skip to first line
	QByteArray header;
	int vIndex, lIndex;
	do {
		header = proc->readLine().trimmed();
		vIndex = header.indexOf("Version");
		lIndex = header.indexOf("License");
	} while(!proc->atEnd() && vIndex == -1);

	if(vIndex != -1) {
		if(lIndex != -1)
			lIndex -= vIndex + 1;

		proc->readLine();//skip seperator line
		auto nLine = proc->readLine().mid(vIndex, lIndex).trimmed();
		version = QVersionNumber::fromString(QString::fromUtf8(nLine));
	}

	emit versionResult(id, version);
}

void QpmSourcePlugin::completeInstall(int id, QProcess *proc, const QVariantHash &params)
{
	try {
		QDir tDir(params.value(QStringLiteral("dir")).toString());
		auto package = params.value(QStringLiteral("package")).toString();
		auto subPath = QStringLiteral(".qpm/vendor/%1")
					   .arg(package.replace(QLatin1Char('.'), QLatin1Char('/')));
		if(!tDir.exists(QStringLiteral("%1/qpm.json").arg(subPath))) {
			throw tr("Failed to download qpm package. "
					 "Check the logs at \"%2\" for more details")
				  .arg(proc->property("logDir").toString());
		}

		//move install data to a new tmp dir, delete the old one and then move back
		QFileInfo tInfo(tDir.absolutePath());
		QString nName = tInfo.fileName() + QStringLiteral(".mv");
		QDir nDir(tInfo.dir().absoluteFilePath(nName));
		if(!tDir.rename(subPath, nDir.absolutePath()))
			throw tr("Failed to move out sources from qpm download");
		if(!tDir.removeRecursively())
			throw tr("Failed to remove qpm junk data");
		if(!tInfo.dir().rename(nName, tInfo.fileName()))
			throw tr("Failed to move sources back to tmp dir");

		//transform qpm.json
		QFile outFile(tDir.absoluteFilePath(QStringLiteral("qpmx.json")));
		if(!outFile.exists()) {
			QFile inFile(tDir.absoluteFilePath(QStringLiteral("qpm.json")));
			if(!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
				throw tr("Failed to open qpm.json with error: %1").arg(inFile.errorString());

			QJsonParseError error;
			auto doc = QJsonDocument::fromJson(inFile.readAll(), &error);
			if(error.error != QJsonParseError::NoError)
				throw tr("Failed to read qpm.json with error: %1").arg(error.errorString());
			inFile.close();
			auto qpmRoot = doc.object();

			QJsonArray depArray;
			foreach(auto dep, qpmRoot[QStringLiteral("dependencies")].toArray()) {
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
				throw tr("Failed to open qpmx.json with error: %1").arg(outFile.errorString());
			outFile.write(QJsonDocument(qpmxRoot).toJson(QJsonDocument::Indented));
			outFile.close();
		}

		emit sourceFetched(id);
	} catch (QString &s) {
		emit sourceError(id, s);
	}
}
