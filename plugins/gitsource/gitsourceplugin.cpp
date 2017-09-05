#include "gitsourceplugin.h"
#include <QUrl>
#include <QStandardPaths>

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
	auto ok = false;

	if(package.provider().isEmpty() ||
	   package.provider() == QStringLiteral("git")){
		QUrl url(package.package());
		ok = ok || (url.isValid() && url.path().endsWith(QStringLiteral(".git")));
	}

	return ok;
}

void GitSourcePlugin::searchPackage(int requestId, const QString &provider, const QString &query)
{
	Q_UNUSED(query);
	Q_UNUSED(provider);
	//git does not support searching
	emit searchResult(requestId, {});
}

void GitSourcePlugin::getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir, const QVariantHash &extraParameters)
{
	QUrl packageUrl(package.package());
	if(!packageUrl.isValid()) {
		emit sourceError(requestId, tr("The given package name is not a valid url"));
		return;
	}

	QString tag;
	if(packageUrl.hasFragment()) {
		auto prefix = packageUrl.fragment();
		if(prefix.contains(QStringLiteral("%1")))
			tag = prefix.arg(package.version().toString());
		else
			tag = prefix + package.version().toString();
	} else
		tag = package.version().toString();

	auto logBase = QFileInfo(targetDir.absolutePath()).fileName();
	logBase = QStringLiteral("qpmx.logs/") + logBase;
	QDir logDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
	if(!logDir.mkpath(logBase) || !logDir.cd(logBase)) {
		emit sourceError(requestId, tr("Failed to create log directory \"%1\"").arg(logDir.absolutePath()));
		return;
	}

	auto proc = new QProcess(this);
	proc->setProgram(QStandardPaths::findExecutable(QStringLiteral("git")));
	proc->setStandardOutputFile(logDir.absoluteFilePath(QStringLiteral("stdout.log")));
	proc->setStandardErrorFile(logDir.absoluteFilePath(QStringLiteral("stderr.log")));
	proc->setProperty("logDir", logDir.absolutePath());

	QStringList arguments{
							  QStringLiteral("clone"),
							  packageUrl.toString(),
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

	_processCache.insert(proc, requestId);
	proc->start();
}

void GitSourcePlugin::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
	if(exitStatus == QProcess::CrashExit)
		errorOccurred(QProcess::Crashed);
	else {
		auto proc = qobject_cast<QProcess*>(sender());
		auto id = _processCache.value(proc, -1);
		if(id != -1) {
			_processCache.remove(proc);
			if(exitCode == EXIT_SUCCESS)
				emit sourceFetched(id);
			else {
				emit sourceError(id,
								 tr("Failed to clone source with exit code %1. "
									"Check the logs at \"%2\" for more details")
								 .arg(exitCode)
								 .arg(proc->property("logDir").toString()));
			}
		}
		proc->deleteLater();
	}
}

void GitSourcePlugin::errorOccurred(QProcess::ProcessError error)
{
	Q_UNUSED(error)
	auto proc = qobject_cast<QProcess*>(sender());
	auto id = _processCache.value(proc, -1);
	if(id != -1) {
		_processCache.remove(proc);
		emit sourceError(id,
						 tr("Failed to clone source with process error %1")
						 .arg(proc->errorString()));
	}
	proc->deleteLater();
}
