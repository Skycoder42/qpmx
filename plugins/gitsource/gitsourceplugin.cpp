#include "gitsourceplugin.h"
#include <QUrl>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QSettings>

QRegularExpression GitSourcePlugin::_githubRegex(QStringLiteral(R"__(^com\.github\.([^\.#]*)\.([^\.#]*)(?:#(.*))?$)__"));

GitSourcePlugin::GitSourcePlugin(QObject *parent) :
	QObject(parent),
	SourcePlugin(),
	_processCache()
{}

bool GitSourcePlugin::canSearch(const QString &provider) const
{
	Q_UNUSED(provider)
	return false;
}

bool GitSourcePlugin::canPublish(const QString &provider) const
{
	Q_UNUSED(provider)
	return true;
}

QString GitSourcePlugin::packageSyntax(const QString &provider) const
{
	if(provider == QStringLiteral("git"))
		return tr("<url>[#<prefix>]");
	else if(provider == QStringLiteral("github"))
		return tr("com.github.<user>.<repository>[#<prefix>]");
	else
		return {};
}

bool GitSourcePlugin::packageValid(const qpmx::PackageInfo &package) const
{
	if(package.provider() == QStringLiteral("git")) {
		QUrl url(package.package());
		return url.isValid() && url.path().endsWith(QStringLiteral(".git"));
	} else if(package.provider() == QStringLiteral("github"))
		return _githubRegex.match(package.package()).hasMatch();
	else
		return false;
}

QJsonObject GitSourcePlugin::createPublisherInfo(const QString &provider) const
{
	QFile console;
	if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
		throw tr("Failed to access console with error: %1").arg(console.errorString());

	QTextStream stream(&console);
	if(provider == QStringLiteral("git")) {
		QString pUrl;
		forever {
			qInfo().noquote() << tr("Enter the remote-url to push the package to:");
			pUrl = stream.readLine().trimmed().toLower();
			QUrl url(pUrl);
			if(!url.isValid() || !url.path().endsWith(QStringLiteral(".git")))
				qWarning().noquote() << tr("Invalid url! Enter a valid url that ends with \".git\"");
			else
				break;
		}

		qInfo().noquote() << tr("Enter a version prefix (optional):");
		auto pPrefix = stream.readLine().trimmed().toLower();

		QJsonObject object;
		object[QStringLiteral("url")] = pUrl;
		object[QStringLiteral("prefix")] = pPrefix;
		return object;
	} else if(provider == QStringLiteral("github")) {
		QJsonObject object;
		qInfo().noquote() << tr("Enter the users name to push to:");
		object[QStringLiteral("user")] = stream.readLine().trimmed().toLower();
		qInfo().noquote() << tr("Enter the repository name to push to:");
		object[QStringLiteral("repository")] = stream.readLine().trimmed().toLower();
		qInfo().noquote() << tr("Enter a version prefix (optional):");
		object[QStringLiteral("prefix")] = stream.readLine().trimmed().toLower();
		return object;
	} else
		return {};
}

void GitSourcePlugin::cancelAll(int timeout)
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

void GitSourcePlugin::searchPackage(int requestId, const QString &provider, const QString &query)
{
	Q_UNUSED(query);
	Q_UNUSED(provider);
	//git does not support searching
	emit searchResult(requestId, {});
}

void GitSourcePlugin::findPackageVersion(int requestId, const qpmx::PackageInfo &package)
{
	try {
		QString prefix;
		auto url = pkgUrl(package, &prefix);

		QStringList arguments {
			QStringLiteral("ls-remote"),
			QStringLiteral("--tags"),
			QStringLiteral("--exit-code"),
			url
		};
		if(!package.version().isNull())
			arguments.append(pkgTag(package));
		else if(!prefix.isNull())
			arguments.append(prefix + QLatin1Char('*'));

		auto proc = createProcess(QStringLiteral("ls-remote"), arguments);
		_processCache.insert(proc, {requestId, LsRemote, {}});
		proc->start();
	} catch(QString &s) {
		emit sourceError(requestId, s);
	}
}

void GitSourcePlugin::getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir)
{
	try {
		auto url = pkgUrl(package);
		auto tag = pkgTag(package);

		QStringList arguments {
			QStringLiteral("clone"),
			url,
			QStringLiteral("--recurse-submodules"),
			QStringLiteral("--branch"),
			tag,
			targetDir.absolutePath()
		};

		auto proc = createProcess(QStringLiteral("clone"), arguments, true);
		_processCache.insert(proc, {requestId, Clone, {}});
		proc->start();
	} catch(QString &s) {
		emit sourceError(requestId, s);
	}
}

void GitSourcePlugin::publishPackage(int requestId, const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo)
{
	QString url;
	if(provider == QStringLiteral("git"))
		url = publisherInfo[QStringLiteral("url")].toString();
	else if(provider == QStringLiteral("github")) {
		url = QStringLiteral("https://github.com/%1/%2.git")
			  .arg(publisherInfo[QStringLiteral("user")].toString())
			  .arg(publisherInfo[QStringLiteral("repository")].toString());
	} else {
		emit sourceError(requestId, tr("Unsupported provider"));
	}

	auto tag = version.toString();
	auto prefix = publisherInfo[QStringLiteral("prefix")].toString();
	if(!prefix.isEmpty())
		tag.prepend(prefix);

	//verify the url and get the origin
	QString remote;
	QSettings gitConfig(qpmxDir.absoluteFilePath(QStringLiteral(".git/config")), QSettings::IniFormat);
	QRegularExpression regex(QStringLiteral(R"__(remote\s*"(.+)")__"));
	foreach(auto group, gitConfig.childGroups()) {
		auto match = regex.match(group);
		if(match.hasMatch()) {
			gitConfig.beginGroup(group);
			if(gitConfig.value(QStringLiteral("url")).toString() == url) {
				gitConfig.endGroup();
				remote = match.captured(1);
				break;
			}
			gitConfig.endGroup();
		}
	}

	if(remote.isEmpty()) {
		qWarning().noquote() << tr("Unable to determine remote based of url. Pushing to url directly");
		remote = url;
	}

	//create the tag
	QStringList arguments {
		QStringLiteral("tag"),
		tag
	};

	auto proc = createProcess(QStringLiteral("tag"), arguments, true);
	QVariantHash params;
	params.insert(QStringLiteral("remote"), remote);
	params.insert(QStringLiteral("tag"), tag);
	_processCache.insert(proc, {requestId, Tag, params});
	proc->start();
}

void GitSourcePlugin::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
	if(exitStatus == QProcess::CrashExit)
		errorOccurred(QProcess::Crashed);
	else {
		auto proc = qobject_cast<QProcess*>(sender());
		auto data = _processCache.value(proc, {-1, Invalid, {}});
		if(std::get<0>(data) != -1) {
			_processCache.remove(proc);
			switch (std::get<1>(data)) {
			case GitSourcePlugin::Invalid:
				break;//ignore
			case GitSourcePlugin::LsRemote:
				lsRemoteDone(std::get<0>(data), proc, exitCode);
				break;
			case GitSourcePlugin::Clone:
				cloneDone(std::get<0>(data), proc, exitCode);
				break;
			case GitSourcePlugin::Tag:
				tagDone(std::get<0>(data), proc, exitCode, std::get<2>(data));
				break;
			case GitSourcePlugin::Push:
				break;
			default:
				Q_UNREACHABLE();
				break;
			}
		}
		proc->deleteLater();
	}
}

void GitSourcePlugin::errorOccurred(QProcess::ProcessError error)
{
	Q_UNUSED(error)
	auto proc = qobject_cast<QProcess*>(sender());
	auto data = _processCache.value(proc, {-1, Invalid, {}});
	if(std::get<0>(data) != -1) {
		_processCache.remove(proc);
		QString op;
		switch (std::get<1>(data)) {
		case GitSourcePlugin::Invalid:
			break;//ignore
		case GitSourcePlugin::LsRemote:
			op = tr("list versions");
			break;
		case GitSourcePlugin::Clone:
			op = tr("clone sources");
			break;
		case GitSourcePlugin::Tag:
			op = tr("create version tag");
			break;
		case GitSourcePlugin::Push:
			break;
		default:
			Q_UNREACHABLE();
			break;
		}
		emit sourceError(std::get<0>(data),
						 tr("Failed to %1 with process error: %2")
						 .arg(op)
						 .arg(proc->errorString()));
	}
	proc->deleteLater();
}

QString GitSourcePlugin::pkgUrl(const qpmx::PackageInfo &package, QString *prefix)
{
	QString pkgUrl;
	if(package.provider() == QStringLiteral("git"))
		pkgUrl = package.package();
	else if(package.provider() == QStringLiteral("github")) {
		auto match = _githubRegex.match(package.package());
		if(!match.hasMatch())
			throw tr("Package %1 is not a valid github package").arg(package.toString());
		pkgUrl = QStringLiteral("https://github.com/%1/%2.git#%3")
				 .arg(match.captured(1))
				 .arg(match.captured(2))
				 .arg(match.captured(3));
	} else
		throw tr("Unknown provider type \"%1\"").arg(package.provider());

	if(prefix) {
		QUrl pUrl(pkgUrl);
		if(pUrl.hasFragment())
			*prefix = pUrl.fragment();
		else
			prefix->clear();
	}
	return pkgUrl;
}

QString GitSourcePlugin::pkgTag(const qpmx::PackageInfo &package)
{
	QString prefix;
	QUrl packageUrl = pkgUrl(package, &prefix);
	if(!packageUrl.isValid())
		throw tr("The given package name is not a valid url");

	QString tag;
	if(!prefix.isNull()) {
		if(prefix.contains(QStringLiteral("%1")))
			tag = prefix.arg(package.version().toString());
		else
			tag = prefix + package.version().toString();
	} else
		tag = package.version().toString();
	return tag;
}

QDir GitSourcePlugin::createLogDir(const QString &action)
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

QProcess *GitSourcePlugin::createProcess(const QString &type, const QStringList &arguments, bool stdLog)
{
	auto logDir = createLogDir(type);

	auto proc = new QProcess(this);
	proc->setProgram(QStandardPaths::findExecutable(QStringLiteral("git")));
	if(stdLog)
		proc->setStandardOutputFile(logDir.absoluteFilePath(QStringLiteral("stdout.log")));
	proc->setStandardErrorFile(logDir.absoluteFilePath(QStringLiteral("stderr.log")));
	proc->setArguments(arguments);
	proc->setProperty("logDir", logDir.absolutePath());

	connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this, &GitSourcePlugin::finished,
			Qt::QueuedConnection);
	connect(proc, &QProcess::errorOccurred,
			this, &GitSourcePlugin::errorOccurred,
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

void GitSourcePlugin::lsRemoteDone(int requestId, QProcess *proc, int exitCode)
{
	if(exitCode == EXIT_SUCCESS) {
		//parse output
		QSet<QVersionNumber> versions;
		QRegularExpression tagRegex(QStringLiteral(R"__(^\w+\trefs\/tags\/(.*)$)__"));
		foreach(auto line, proc->readAllStandardOutput().split('\n')) {
			auto match = tagRegex.match(QString::fromUtf8(line));
			if(match.hasMatch())
				versions.insert(QVersionNumber::fromString(match.captured(1)));
		}

		auto vList = versions.toList();
		std::sort(vList.begin(), vList.end());
		emit versionResult(requestId, vList.last());
	} else {
		if(exitCode == 2) {
			emit versionResult(requestId, {});
		} else {
			emit sourceError(requestId,
							 tr("Failed to list versions with exit code %1. "
								"Check the logs at \"%2\" for more details")
							 .arg(exitCode)
							 .arg(proc->property("logDir").toString()));
		}
	}
}

void GitSourcePlugin::cloneDone(int requestId, QProcess *proc, int exitCode)
{
	if(exitCode == EXIT_SUCCESS)
		emit sourceFetched(requestId);
	else {
		emit sourceError(requestId,
						 tr("Failed to clone source with exit code %1. "
							"Check the logs at \"%2\" for more details")
						 .arg(exitCode)
						 .arg(proc->property("logDir").toString()));
	}
}

void GitSourcePlugin::tagDone(int requestId, QProcess *proc, int exitCode, const QVariantHash &params)
{
	//TODO here
}
