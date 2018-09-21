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
#include <iostream>
#include <qtcoawaitables.h>

#define print(x) do { \
	std::cout << QString(x).toStdString(); \
	std::cout.flush(); \
} while(false)

QRegularExpression GitSourcePlugin::_githubRegex(QStringLiteral(R"__(^com\.github\.([^\.#]*)\.([^\.#]*)(?:#(.*))?$)__"));

GitSourcePlugin::GitSourcePlugin(QObject *parent) :
	QObject{parent}
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

QJsonObject GitSourcePlugin::createPublisherInfo(const QString &provider)
{
	QFile console;
	if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
		throw qpmx::SourcePluginException{tr("Failed to access console with error: %1").arg(console.errorString())};

	QTextStream stream(&console);
	if(provider == QStringLiteral("git")) {
		QString pUrl;
		forever {
			print(tr("Enter the remote-url to push the package to: "));
			pUrl = stream.readLine().trimmed().toLower();
			QUrl url(pUrl);
			if(!url.isValid() || !url.path().endsWith(QStringLiteral(".git")))
				qWarning().noquote() << tr("Invalid url! Enter a valid url that ends with \".git\"");
			else
				break;
		}

		print(tr("Enter a version prefix (optional): "));
		auto pPrefix = stream.readLine().trimmed().toLower();

		QJsonObject object;
		object[QStringLiteral("url")] = pUrl;
		object[QStringLiteral("prefix")] = pPrefix;
		return object;
	} else if(provider == QStringLiteral("github")) {
		QJsonObject object;
		print(tr("Enter the users name to push to: "));
		object[QStringLiteral("user")] = stream.readLine().trimmed().toLower();
		print(tr("Enter the repository name (NOT url) to push to: "));
		object[QStringLiteral("repository")] = stream.readLine().trimmed().toLower();
		print(tr("Enter a version prefix (optional): "));
		object[QStringLiteral("prefix")] = stream.readLine().trimmed().toLower();
		return object;
	} else
		return {};
}

QStringList GitSourcePlugin::searchPackage(const QString &provider, const QString &query)
{
	Q_UNUSED(query);
	Q_UNUSED(provider);
	return {};
}

QVersionNumber GitSourcePlugin::findPackageVersion(const qpmx::PackageInfo &package)
{
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

	auto proc = createProcess(arguments, true);
	_processCache.insert(proc);
	qDebug().noquote() << tr("Listing remote for repository %1").arg(url);
	auto res = QtCoroutine::await(proc);
	_processCache.remove(proc);
	proc->deleteLater();

	if(res == EXIT_SUCCESS) {
		//parse output
		qDebug().noquote() << tr("Parsing ls-remote output");
		QSet<QVersionNumber> versions;
		QRegularExpression tagRegex(QStringLiteral(R"__(^\w+\trefs\/tags\/(.*)$)__"));
		for(const auto &line : proc->readAllStandardOutput().split('\n')) {
			auto match = tagRegex.match(QString::fromUtf8(line));
			if(match.hasMatch())
				versions.insert(QVersionNumber::fromString(match.captured(1)));
		}

		auto vList = versions.toList();
		std::sort(vList.begin(), vList.end());
		if(vList.isEmpty())
			return {};
		else
			return vList.last();
	} else if(res == 2)
		return {};
	else
		throw qpmx::SourcePluginException{formatProcError(tr("list versions"), proc)};
}

void GitSourcePlugin::getPackageSource(const qpmx::PackageInfo &package, const QDir &targetDir)
{
	auto url = pkgUrl(package);
	auto tag = pkgTag(package);

	QStringList arguments {
		QStringLiteral("clone"),
		QStringLiteral("--recurse-submodules"),
		QStringLiteral("--depth"),
		QStringLiteral("1"),
		QStringLiteral("--branch"),
		tag,
		url,
		targetDir.absolutePath()
	};

	auto proc = createProcess(arguments);
	_processCache.insert(proc);
	qDebug().noquote() << tr("Cloning git repository %1").arg(url);
	auto res = QtCoroutine::await(proc);
	_processCache.remove(proc);
	proc->deleteLater();
	if(res == EXIT_SUCCESS) {
		auto tDir = targetDir;
		if(tDir.cd(QStringLiteral(".git")))
			tDir.removeRecursively();
	} else
		throw qpmx::SourcePluginException{formatProcError(tr("clone versions"), proc)};
}

void GitSourcePlugin::publishPackage(const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo)
{
	QString url;
	if(provider == QStringLiteral("git"))
		url = publisherInfo[QStringLiteral("url")].toString();
	else if(provider == QStringLiteral("github")) {
		url = QStringLiteral("https://github.com/%1/%2.git")
			  .arg(publisherInfo[QStringLiteral("user")].toString(), publisherInfo[QStringLiteral("repository")].toString());
	} else
		throw qpmx::SourcePluginException{tr("Unsupported provider")};

	auto tag = version.toString();
	auto prefix = publisherInfo[QStringLiteral("prefix")].toString();
	if(!prefix.isEmpty())
		tag.prepend(prefix);

	//verify the url and get the origin
	QString remote;
	QSettings gitConfig{qpmxDir.absoluteFilePath(QStringLiteral(".git/config")), QSettings::IniFormat};
	QRegularExpression regex(QStringLiteral(R"__(remote\s*"(.+)")__"));
	for(const auto &group : gitConfig.childGroups()) {
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
	auto proc = createProcess(arguments);
	_processCache.insert(proc);
	qDebug().noquote() << tr("Create new tag %{bld}%1%{end}").arg(tag);
	auto res = QtCoroutine::await(proc);
	_processCache.remove(proc);
	proc->deleteLater();
	if(res != EXIT_SUCCESS)
		throw qpmx::SourcePluginException{formatProcError(tr("create tag"), proc)};

	//push the tag to origin
	arguments = QStringList {
		QStringLiteral("push"),
		remote,
		tag
	};
	proc = createProcess(arguments, true);
	proc->setProcessChannelMode(QProcess::ForwardedOutputChannel);
	proc->setInputChannelMode(QProcess::ForwardedInputChannel);
	_processCache.insert(proc);
	qDebug().noquote() << tr("Pushing tag to remote \"%1\"").arg(remote);
	res = QtCoroutine::await(proc);
	_processCache.remove(proc);
	proc->deleteLater();
	if(res != EXIT_SUCCESS)
		throw qpmx::SourcePluginException{formatProcError(tr("push tag to remote"), proc)};
}

void GitSourcePlugin::cancelAll(int timeout)
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
}


QString GitSourcePlugin::pkgUrl(const qpmx::PackageInfo &package, QString *prefix)
{
	QString pkgUrl;
	if(package.provider() == QStringLiteral("git"))
		pkgUrl = package.package();
	else if(package.provider() == QStringLiteral("github")) {
		auto match = _githubRegex.match(package.package());
		if(!match.hasMatch())
			throw qpmx::SourcePluginException{tr("The Package %1 is not a valid github package").arg(package.toString())};
		pkgUrl = QStringLiteral("https://github.com/%1/%2.git")
				 .arg(match.captured(1), match.captured(2));
		if(!match.captured(3).isEmpty())
			pkgUrl += QLatin1Char('#') + match.captured(3);
	} else
		throw qpmx::SourcePluginException{tr("Unsupported provider %{bld}%1%{end}").arg(package.provider())};

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
		throw qpmx::SourcePluginException{tr("The name of package %1 is not a valid url").arg(package.toString())};

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

QProcess *GitSourcePlugin::createProcess(const QStringList &arguments, bool keepStdout)
{
	auto proc = new QProcess(this);
	proc->setProgram(QStandardPaths::findExecutable(QStringLiteral("git")));
	if(!keepStdout)
		proc->setStandardOutputFile(QProcess::nullDevice());
	proc->setArguments(arguments);
	return proc;
}

QString GitSourcePlugin::formatProcError(const QString &type, QProcess *proc)
{
	auto res = tr("Failed to %1 with exit code %2 and stderr:")
			   .arg(type)
			   .arg(proc->exitCode());
	proc->setReadChannel(QProcess::StandardError);
	while(!proc->atEnd()) {
		auto line = QString::fromUtf8(proc->readLine()).trimmed();
		res.append(QStringLiteral("\n\t%1").arg(line));
	}
	return res;
}
