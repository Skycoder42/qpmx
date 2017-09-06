#include "gitsourceplugin.h"
#include <QUrl>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QSet>

GitSourcePlugin::GitSourcePlugin(QObject *parent) :
	QObject(parent),
	SourcePlugin(),
	_processCache()
{}

QString GitSourcePlugin::packageSyntax() const
{
	return tr("<url>[#<prefix>]");
}

bool GitSourcePlugin::packageValid(const qpmx::PackageInfo &package) const
{
	if(package.provider() == QStringLiteral("git")) {
		QUrl url(package.package());
		return url.isValid() && url.path().endsWith(QStringLiteral(".git"));
	} else
		return false;
}

void GitSourcePlugin::searchPackage(int requestId, const QString &provider, const QString &query)
{
	Q_UNUSED(query);
	Q_UNUSED(provider);
	//git does not support searching
	emit searchResult(requestId, {});
}

void GitSourcePlugin::listPackageVersions(int requestId, const qpmx::PackageInfo &package)
{
	try {
		auto tag = pkgTag(package);
		auto logDir = createLogDir(QStringLiteral("ls-remote"));

		auto proc = new QProcess(this);
		proc->setProgram(QStandardPaths::findExecutable(QStringLiteral("git")));
		proc->setStandardOutputFile(logDir.absoluteFilePath(QStringLiteral("stdout.log")));
		proc->setStandardErrorFile(logDir.absoluteFilePath(QStringLiteral("stderr.log")));
		proc->setProperty("logDir", logDir.absolutePath());

		QStringList arguments{
								  QStringLiteral("ls-remote"),
								  QStringLiteral("--tags"),
								  QStringLiteral("--exit-code"),
								  package.package()
							  };
		if(!package.version().isNull())
			arguments.append(package.version().toString());
		proc->setArguments(arguments);

		connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
				this, &GitSourcePlugin::finished);
		connect(proc, &QProcess::errorOccurred,
				this, &GitSourcePlugin::errorOccurred);

		_processCache.insert(proc, {requestId, false});
		proc->start();
	} catch(QString &s) {
		emit sourceError(requestId, s);
	}
}

void GitSourcePlugin::getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir, const QVariantHash &extraParameters)
{
	try {
		auto tag = pkgTag(package);
		auto logDir = createLogDir(QStringLiteral("clone"));

		auto proc = new QProcess(this);
		proc->setProgram(QStandardPaths::findExecutable(QStringLiteral("git")));
		proc->setStandardOutputFile(logDir.absoluteFilePath(QStringLiteral("stdout.log")));
		proc->setStandardErrorFile(logDir.absoluteFilePath(QStringLiteral("stderr.log")));
		proc->setProperty("logDir", logDir.absolutePath());

		QStringList arguments{
								  QStringLiteral("clone"),
								  package.package(),
								  QStringLiteral("--recurse-submodules"),
								  QStringLiteral("--branch"),
								  tag,
								  targetDir.absolutePath()
							  };
		if(extraParameters.contains(QStringLiteral("gitargs")))
			arguments.append(extraParameters.value(QStringLiteral("gitargs")).toStringList());
		proc->setArguments(arguments);

		connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
				this, &GitSourcePlugin::finished);
		connect(proc, &QProcess::errorOccurred,
				this, &GitSourcePlugin::errorOccurred);

		_processCache.insert(proc, {requestId, true});
		proc->start();
	} catch(QString &s) {
		emit sourceError(requestId, s);
	}
}

void GitSourcePlugin::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
	if(exitStatus == QProcess::CrashExit)
		errorOccurred(QProcess::Crashed);
	else {
		auto proc = qobject_cast<QProcess*>(sender());
		auto data = _processCache.value(proc, {-1, false});
		if(data.first != -1) {
			_processCache.remove(proc);
			if(data.second) {
				if(exitCode == EXIT_SUCCESS)
					emit sourceFetched(data.first);
				else {
					emit sourceError(data.first,
									 tr("Failed to clone source with exit code %1. "
										"Check the logs at \"%2\" for more details")
									 .arg(exitCode)
									 .arg(proc->property("logDir").toString()));
				}
			} else {
				if(exitCode == EXIT_SUCCESS) {
					//parse output
					QSet<QVersionNumber> versions;
					QRegularExpression tagRegex(QStringLiteral(R"__(^\w+\trefs\/tags\/(.*)$)__"));
					foreach(auto line, proc->readAllStandardOutput().split('\n')) {
						auto match = tagRegex.match(QString::fromUtf8(line));
						if(match.hasMatch())
							versions.insert(QVersionNumber::fromString(match.captured(1)));
					}
					emit versionResult(data.first, versions.toList());
				} else {
					if(exitCode == 2) {
						emit sourceError(data.first, tr("Failed to find the given or any version"));
					} else {
						emit sourceError(data.first,
										 tr("Failed to list versions with exit code %1. "
											"Check the logs at \"%2\" for more details")
										 .arg(exitCode)
										 .arg(proc->property("logDir").toString()));
					}
				}
			}
		}
		proc->deleteLater();
	}
}

void GitSourcePlugin::errorOccurred(QProcess::ProcessError error)
{
	Q_UNUSED(error)
	auto proc = qobject_cast<QProcess*>(sender());
	auto data = _processCache.value(proc, {-1, false});
	if(data.first != -1) {
		_processCache.remove(proc);
		if(data.second) {
			emit sourceError(data.first,
							 tr("Failed to clone source with process error: %1")
							 .arg(proc->errorString()));
		} else {
			emit sourceError(data.first,
							 tr("Failed to list versions with process error: %1")
							 .arg(proc->errorString()));
		}
	}
	proc->deleteLater();
}

QString GitSourcePlugin::pkgTag(const qpmx::PackageInfo &package)
{
	QUrl packageUrl(package.package());
	if(!packageUrl.isValid())
		throw tr("The given package name is not a valid url");

	QString tag;
	if(packageUrl.hasFragment()) {
		auto prefix = packageUrl.fragment();
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
	QTemporaryDir tDir(QDir::temp().absoluteFilePath(QStringLiteral("qpmx.logs/%1/XXXXXX").arg(action)));
	tDir.setAutoRemove(false);
	if(tDir.isValid())
		return tDir.path();
	else
		throw tr("Failed to create log directory \"%1\"").arg(tDir.path());
}
