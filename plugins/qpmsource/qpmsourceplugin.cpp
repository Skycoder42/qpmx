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
		qInfo().noquote() << tr("%{pkg}## Step 1:%{end} Run qpm initialization. You can leave the license and pri file parts empty:");
		qWarning().noquote() << tr("Do NOT generate boilerplate code!");

		QProcess p;
		p.setProgram(QStandardPaths::findExecutable(QStringLiteral("qpm")));
		p.setArguments({QStringLiteral("init")});
		p.setProcessChannelMode(QProcess::ForwardedChannels);
		p.setInputChannelMode(QProcess::ForwardedInputChannel);
		p.start();
		p.waitForFinished(-1);

		if(p.exitStatus() != QProcess::NormalExit || p.exitCode() != EXIT_SUCCESS)
			throw tr("Failed to run qpm initialization step");

		qInfo().noquote() << tr("%{pkg}## Step 2:%{end} Add additional data. You can now modify the qpm.json to provide extra data. Press <ENTER> once you are done...");
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

		auto proc = createProcess(arguments, true);
		_processCache.insert(proc, tpl{requestId, Search, {}});
		qDebug().noquote() << tr("Running qpm search for query: %1").arg(query);
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

		auto proc = createProcess(arguments, true);
		_processCache.insert(proc, tpl{requestId, Version, {}});
		qDebug().noquote() << tr("Running qpm search for \"%1\" to find latest version").arg(package.package());
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

		auto qpmPkg = package.package();
		if(!package.version().isNull())
			qpmPkg += QLatin1Char('@') + package.version().toString();

		QStringList arguments{
								  QStringLiteral("install"),
								  qpmPkg
							  };

		QVariantHash params;
		params.insert(QStringLiteral("dir"), targetDir.absolutePath());
		params.insert(QStringLiteral("package"), package.package());
		auto proc = createProcess(arguments);
		proc->setWorkingDirectory(targetDir.absoluteFilePath(subPath));
		_processCache.insert(proc, tpl{requestId, Install, params});
		qDebug().noquote() << tr("Running qpm install for qpm package %1").arg(qpmPkg);
		proc->start();
	} catch (QString &s) {
		emit sourceError(requestId, s);
	}
}

void QpmSourcePlugin::publishPackage(int requestId, const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo)
{
	try {
		if(provider != QStringLiteral("qpm"))
			throw tr("Unsupported provider \"%1\"").arg(provider);

		qInfo().noquote() << tr("%{pkg}## Step 1:%{end} Preparing publishing...");
		//read qpmx.json
		QFile qpmxFile(qpmxDir.absoluteFilePath(QStringLiteral("qpmx.json")));
		if(!qpmxFile.exists())
			throw tr("qpmx.json does not exist, unable to read data");
		if(!qpmxFile.open(QIODevice::ReadOnly | QIODevice::Text))
			throw tr("Failed to open qpmx.json file with error: %1").arg(qpmxFile.errorString());

		QJsonParseError error;
		auto qpmxJson = QJsonDocument::fromJson(qpmxFile.readAll(), &error).object();
		if(error.error != QJsonParseError::NoError)
			throw tr("Failed to read qpm.json file with error: %1").arg(error.errorString());
		qpmxFile.close();

		//create the qpm file
		auto qpmJson = publisherInfo;

		QJsonObject vJson;
		vJson[QStringLiteral("label")] = version.toString();
		vJson[QStringLiteral("revision")] = QString();
		vJson[QStringLiteral("fingerprint")] = QString();
		qpmJson[QStringLiteral("version")] = vJson;

		QJsonArray dArray;
		foreach(auto dep, qpmxJson[QStringLiteral("dependencies")].toArray()) {
			auto depObj = dep.toObject();
			if(depObj[QStringLiteral("provider")].toString() != QStringLiteral("qpm"))
				throw tr("A qpm package can only have qpm dependencies. Other providers are not allowed");
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
			throw tr("Failed to create qpm.json file with error: %1").arg(qpmFile.errorString());
		qpmFile.write(QJsonDocument(qpmJson).toJson(QJsonDocument::Indented));
		qpmFile.close();

		QFile console;
		if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
			throw tr("Failed to access console with error: %1").arg(console.errorString());
		qInfo().noquote() << tr("%{pkg}## Step 2:%{end} qpm file created. Please commit the changes and push it to the remote!");
		console.readLine();

		// publish via qpm
		qInfo().noquote() << tr("%{pkg}## Step 3:%{end} Publishing the package via qpm...");
		auto proc = createProcess({QStringLiteral("publish")}, true, false);
		proc->setWorkingDirectory(qpmxDir.absolutePath());
		proc->setProcessChannelMode(QProcess::ForwardedOutputChannel);
		proc->setInputChannelMode(QProcess::ForwardedInputChannel);
		_processCache.insert(proc, tpl{requestId, Publish, {}});
		qDebug().noquote() << tr("Running qpm publish");
		proc->start();
	} catch (QString &s) {
		emit sourceError(requestId, s);
	}
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
			case Publish:
				completePublish(std::get<0>(data), proc);
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
		QString reason;
		switch (std::get<1>(data)) {
		case Search:
			reason = tr("search qpm package");
			break;
		case Version:
			reason = tr("find qpm package version");
			break;
		case Install:
			reason = tr("download qpm package");
			break;
		case Publish:
			reason = tr("publish qpm package");
			break;
		default:
			Q_UNREACHABLE();
			break;
		}
		emit sourceError(std::get<0>(data),
						 tr("Failed to %1 with process error: %2")
						 .arg(reason)
						 .arg(proc->errorString()));
	}
	proc->deleteLater();
}

QProcess *QpmSourcePlugin::createProcess(const QStringList &arguments, bool keepStdout, bool timeout)
{
	auto proc = new QProcess(this);
	proc->setProgram(QStandardPaths::findExecutable(QStringLiteral("qpm")));
	if(!keepStdout)
		proc->setStandardOutputFile(QProcess::nullDevice());
	proc->setArguments(arguments);

	connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this, &QpmSourcePlugin::finished,
			Qt::QueuedConnection);
	connect(proc, &QProcess::errorOccurred,
			this, &QpmSourcePlugin::errorOccurred,
			Qt::QueuedConnection);

	if(timeout) {
		//timeout after 30 seconds
		QTimer::singleShot(30000, this, [proc, this](){
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
	auto res = tr("Failed to %1 with stderr:").arg(type);
	proc->setReadChannel(QProcess::StandardError);
	while(!proc->atEnd()) {
		auto line = QString::fromUtf8(proc->readLine()).trimmed();
		res.append(QStringLiteral("\n\t%1").arg(line));
	}
	return res;
}

void QpmSourcePlugin::completeSearch(int id, QProcess *proc)
{
	//parse output
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

	emit searchResult(id, results);
}

void QpmSourcePlugin::completeVersion(int id, QProcess *proc)
{
	QVersionNumber version;

	//skip to first line
	qDebug().noquote() << tr("Parsing qpm search output");
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
		qDebug().noquote() << tr("Preparing qpm sources for qpmx");
		QDir tDir(params.value(QStringLiteral("dir")).toString());
		auto package = params.value(QStringLiteral("package")).toString();
		auto subPath = QStringLiteral(".qpm/vendor/%1")
					   .arg(package.replace(QLatin1Char('.'), QLatin1Char('/')));
		if(!tDir.exists(QStringLiteral("%1/qpm.json").arg(subPath)))
			throw formatProcError(tr("download qpm package"), proc);

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
			qDebug().noquote() << tr("Transforming qpm.json to qpmx.json");
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

void QpmSourcePlugin::completePublish(int id, QProcess *proc)
{
	Q_UNUSED(proc)
	proc->setReadChannel(QProcess::StandardError);
	if(proc->atEnd())
		emit packagePublished(id);
	else
		emit sourceError(id, formatProcError(tr("publish qpm package"), proc));
}
