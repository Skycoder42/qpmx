#include "command.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <QProcess>
#include <iostream>
#ifdef Q_OS_UNIX
#include <sys/ioctl.h>
#include <unistd.h>
#endif
using namespace qpmx;

int Command::_ExitCode = EXIT_FAILURE;

Command::Command(QObject *parent) :
	QObject(parent),
	_registry(PluginRegistry::instance()),
	_settings(new QSettings(this)),
	_locks(),
	_devMode(false),
	_verbose(false),
	_quiet(false),
#ifndef Q_OS_WIN
	_noColor(false),
#endif
	_qmakeRun(false),
	_cacheDir()
{}

void Command::setupParser(QCliParser &parser, const QHash<QString, Command *> &commands)
{
	parser.setApplicationDescription(QCoreApplication::translate("parser", "A frontend for qpm, to provide source and build caching."));
	parser.addHelpOption();
	parser.addVersionOption();

	//global
	parser.addOption({
						 QStringLiteral("verbose"),
						 QCoreApplication::translate("parser", "Enable verbose output.")
					 });
	parser.addOption({
						 {QStringLiteral("q"), QStringLiteral("quiet")},
						 QCoreApplication::translate("parser", "Limit output to error messages only.")
					 });
#ifndef Q_OS_WIN
	parser.addOption({
						 QStringLiteral("no-color"),
						 QCoreApplication::translate("parser", "Do not use colors to highlight output.")
					 });
#endif
	parser.addOption({
						 {QStringLiteral("d"), QStringLiteral("dir")},
						 QCoreApplication::translate("parser", "Set the working <directory>, i.e. the directory to check for the qpmx.json file."),
						 QCoreApplication::translate("parser", "directory"),
						 QDir::currentPath()
					 });
	parser.addOption({
						 QStringLiteral("dev-cache"),
						 tr("Explicitly set the <path> to the directory to generate the dev build files in. This can be used to share "
							"one dev build cache between multiple projects. The default path is the directory of the qpmx.json file."),
						 tr("path")
					 });
	QCommandLineOption qOpt(QStringLiteral("qmake-run"));
	qOpt.setFlags(QCommandLineOption::HiddenFromHelp);
	parser.addOption(qOpt);

	foreach(auto cmd, commands) {
		parser.addCliNode(cmd->commandName(),
						  cmd->commandDescription(),
						  cmd->createCliNode());
	}
}

void Command::init(QCliParser &parser)
{
	_verbose = parser.isSet(QStringLiteral("verbose"));
	_quiet = parser.isSet(QStringLiteral("quiet"));
#ifndef Q_OS_WIN
	_noColor = parser.isSet(QStringLiteral("no-color"));
#endif
	_qmakeRun = parser.isSet(QStringLiteral("qmake-run"));
	_cacheDir = parser.value(QStringLiteral("dev-cache"));

	qsrand(QDateTime::currentMSecsSinceEpoch());
	initialize(parser);
}

void Command::fin()
{
	finalize();
	_registry->cancelAll();
	for(auto it = _locks.begin(); it != _locks.end(); it++) {
		xDebug() << QStringLiteral("Freeing remaining lock for %{bld}%1/%2%{end}")
					.arg(it.key().first ? QStringLiteral("src") : QStringLiteral("build"))
					.arg(it.key().second);
		it.value()->release();
		delete it.value();
	}
}

int Command::exitCode()
{
	return _ExitCode;
}

QDir Command::subDir(QDir dir, const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir)
{
	if(mkDir) {
		if(provider.isNull())
			return dir;
		if(!dir.mkpath(provider) || !dir.cd(provider))
			throw tr("Failed to create sub directory");

		if(package.isNull())
			return dir;
		auto pName = QString::fromUtf8(QUrl::toPercentEncoding(package))
				.replace(QLatin1Char('%'), QLatin1Char('~'));
		if(!dir.mkpath(pName) || !dir.cd(pName))
			throw tr("Failed to create sub directory");

		if(version.isNull())
			return dir;
		auto VName = version.toString();
		if(!dir.mkpath(VName) || !dir.cd(VName))
			throw tr("Failed to create sub directory");

		return dir;
	} else {
		if(provider.isNull())
			return dir;

		auto path = provider;
		if(!package.isNull()) {
			auto pName = QString::fromUtf8(QUrl::toPercentEncoding(package))
					.replace(QLatin1Char('%'), QLatin1Char('~'));
			path += QStringLiteral("/") + pName;
			if(!version.isNull()) {
				auto VName = version.toString();
				path += QStringLiteral("/") + VName;
			}
		}

		return QDir(dir.absoluteFilePath(path));
	}
}

void Command::finalize() {}

PluginRegistry *Command::registry()
{
	return _registry;
}

QSettings *Command::settings()
{
	return _settings;
}

void Command::setDevMode(bool devModeActive)
{
	_devMode = devModeActive;
}

bool Command::devMode() const
{
	return _devMode;
}

void Command::srcLock(const PackageInfo &package)
{
	if(package.provider().isEmpty() ||
		package.version().isNull())
		throw tr("Semaphores require full packages");
	lock(true, package.toString(false));
}

void Command::srcLock(const QpmxDependency &dep)
{
	srcLock(dep.pkg());
}

void Command::srcUnlock(const PackageInfo &package)
{
	unlock(true, package.toString(false));
}

void Command::srcUnlock(const QpmxDependency &dep)
{
	srcUnlock(dep.pkg());
}

void Command::buildLock(const Command::BuildId &kitId, const PackageInfo &package)
{
	if(package.provider().isEmpty() ||
		package.version().isNull())
		throw tr("Semaphores require full packages");
	lock(false, QStringLiteral("%1/%2").arg(kitId).arg(package.toString(false)));
}

void Command::buildLock(const Command::BuildId &kitId, const QpmxDependency &dep)
{
	buildLock(kitId, dep.pkg());
}

void Command::buildUnlock(const Command::BuildId &kitId, const PackageInfo &package)
{
	unlock(false, QStringLiteral("%1/%2").arg(kitId).arg(package.toString(false)));
}

void Command::buildUnlock(const Command::BuildId &kitId, const QpmxDependency &dep)
{
	buildUnlock(kitId, dep.pkg());
}

void Command::kitLock()
{
	lock(true, QStringLiteral("qt/kits"));//as source, to always lock globally
}

void Command::kitUnlock()
{
	unlock(true, QStringLiteral("qt/kits"));//as source, to always lock globally
}

QList<PackageInfo> Command::readCliPackages(const QStringList &arguments, bool fullPkgOnly) const
{
	QList<PackageInfo> pkgList;

	auto regex = PackageInfo::packageRegexp();
	foreach(auto arg, arguments) {
		auto match = regex.match(arg);
		if(!match.hasMatch())
			throw tr("Malformed package: %1").arg(arg);

		PackageInfo info(match.captured(1),
						 match.captured(2),
						 QVersionNumber::fromString(match.captured(3)));
		if(fullPkgOnly &&
		   (info.provider().isEmpty() ||
			info.version().isNull()))
			throw tr("You must explicitly specify provider, package name and version for this command");
		pkgList.append(info);
		xDebug() << tr("Parsed package: \"%1\" at version %2 (Provider: %3)")
					.arg(info.package())
					.arg(info.version().toString())
					.arg(info.provider());
	}

	if(pkgList.isEmpty())
		throw tr("You must specify at least one package!");
	return pkgList;
}

QList<QpmxDependency> Command::depList(const QList<PackageInfo> &pkgList)
{
	QList<QpmxDependency> depList;
	foreach(auto pkg, pkgList)
		depList.append(pkg);
	return depList;
}

QList<QpmxDevDependency> Command::devDepList(const QList<PackageInfo> &pkgList)
{
	QList<QpmxDevDependency> depList;
	foreach(auto pkg, pkgList)
		depList.append({pkg, QString()});
	return depList;
}

void Command::cleanCaches(const PackageInfo &package)
{
	auto sDir = srcDir(package);
	if(!sDir.removeRecursively())
		throw tr("Failed to remove source cache for %1").arg(package.toString());
	auto bDir = buildDir();
	foreach(auto cmpDir, bDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)) {
		buildLock(cmpDir, package);
		auto rDir = buildDir(cmpDir, package);
		if(!rDir.removeRecursively())
			throw tr("Failed to remove compilation cache for %1").arg(package.toString());
		buildUnlock(cmpDir, package);
	}
	xInfo() << tr("Removed cached sources and binaries for %1").arg(package.toString());
}

bool Command::readBool(const QString &message, QTextStream &stream, bool defaultValue)
{
	forever {
		std::cout << message.arg(defaultValue ? tr("Y/n") : tr("y/N")).toStdString();

		auto read = stream.read(1);
		auto cleanBytes = stream.device()->bytesAvailable();
		if(cleanBytes > 0)
			stream.device()->read(cleanBytes);//discard

		if(read.toLower() == QLatin1Char('y') || read.toLower() == tr("y"))
			return true;
		if(read.toLower() == QLatin1Char('n') || read.toLower() == tr("n"))
			return false;
		else if(read == QLatin1Char('\n') || read == QLatin1Char('\r'))
			return defaultValue;
		else
			xWarning() << "Invalid input! Please type y or n";
	}

	return defaultValue;
}

#define print(x) std::cout << QString(x).toStdString() << std::endl

void Command::printTable(const QStringList &headers, const QList<int> &minimals, const QList<QStringList> &rows)
{
	QStringList maskList;
	for(auto i = 0; i < headers.size(); i++)
		maskList.append(QStringLiteral("%%1").arg(i + 1));
	QString mask = QLatin1Char(' ') + maskList.join(tr(" | ")) + QLatin1Char(' ');

	auto hString = mask;
	for(auto i = 0; i < headers.size(); i++)
		hString = hString.arg(headers[i], -1 * minimals[i]);
	print(hString);

	auto sLine = tr("-").repeated(80);
	auto ctr = -1;
	for(auto i = 0; i < headers.size() - 1; i++) { //skip the last one
		ctr += 3 + qMax(headers[i].size(), minimals[i]);
		sLine.replace(ctr, 1, QLatin1Char('|'));
	}
	print(sLine);

	foreach(auto row, rows) {
		Q_ASSERT_X(row.size() == headers.size(), Q_FUNC_INFO, "tables must have constant row lengths!");
		auto rString = mask;
		for(auto i = 0; i < headers.size(); i++)
			rString = rString.arg(row[i], -1 * qMax(headers[i].size(), minimals[i]));
		print(rString);
	}
}

void Command::subCall(QStringList arguments, const QString &workingDir)
{
	if(!_cacheDir.isEmpty()) {
		arguments.prepend(_cacheDir);
		arguments.prepend(QStringLiteral("--dev-cache"));
	}
	if(_verbose)
		arguments.prepend(QStringLiteral("--verbose"));
	if(_quiet)
		arguments.prepend(QStringLiteral("--quiet"));
#ifndef Q_OS_WIN
	if(_noColor)
		arguments.prepend(QStringLiteral("--no-color"));
#endif
	if(_qmakeRun)
		arguments.prepend(QStringLiteral("--qmake-run"));

	xDebug() << tr("Running subcommand with arguments: %1")
				.arg(arguments.join(QLatin1Char(' ')));

	QProcess p;
	p.setProgram(QCoreApplication::applicationFilePath());
	p.setArguments(arguments);
	if(!workingDir.isNull())
		p.setWorkingDirectory(workingDir);
	p.setProcessChannelMode(QProcess::ForwardedChannels);
	p.setInputChannelMode(QProcess::ForwardedInputChannel);
	p.start();
	p.waitForFinished(-1);

	if(p.error() != QProcess::UnknownError)
		throw tr("Failed to run qpmx subprocess with process error: %1").arg(p.errorString());
	if(p.exitStatus() != QProcess::NormalExit)
		throw tr("Failed to run qpmx subprocess with process - it crashed");
	else if(p.exitCode() != EXIT_SUCCESS) {
		_ExitCode = p.exitCode();
		throw tr("qpmx subprocess failed.");
	}
}

#undef print

QDir Command::srcDir()
{
	QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
	auto name = QStringLiteral("src");
	if(!dir.mkpath(name) || !dir.cd(name))
		throw tr("Failed to create source directory");
	return dir;
}

QDir Command::srcDir(const PackageInfo &package, bool mkDir)
{
	return srcDir(package.provider(), package.package(), package.version(), mkDir);
}

QDir Command::srcDir(const QpmxDependency &dep, bool mkDir)
{
	return srcDir(dep.provider, dep.package, dep.version, mkDir);
}

QDir Command::srcDir(const QpmxDevDependency &dep, bool mkDir)
{
	if(dep.path.isEmpty())
		return srcDir(dep.provider, dep.package, dep.version, mkDir);
	else
		return dep.path;
}

QDir Command::srcDir(const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir)
{
	return subDir(srcDir(), provider, package, version, mkDir);
}

QDir Command::buildDir()
{
	QDir dir;
	QString subFolder;
	if(_devMode) {
		if(_cacheDir.isEmpty())
			dir = QDir::current();
		else
			dir = _cacheDir;
		subFolder = QStringLiteral(".qpmx-dev-cache");
	} else {
		dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
		subFolder = QStringLiteral("build");
	}
	if(!dir.mkpath(subFolder) || !dir.cd(subFolder))
		throw tr("Failed to create build directory");
	return dir;
}

QDir Command::buildDir(const BuildId &kitId)
{
	auto dir = buildDir();
	if(!dir.mkpath(kitId) || !dir.cd(kitId))
		throw tr("Failed to create build kit directory");
	return dir;
}

QDir Command::buildDir(const BuildId &kitId, const PackageInfo &package, bool mkDir)
{
	return buildDir(kitId, package.provider(), package.package(), package.version(), mkDir);
}

QDir Command::buildDir(const BuildId &kitId, const QpmxDependency &dep, bool mkDir)
{
	return buildDir(kitId, dep.provider, dep.package, dep.version, mkDir);
}

QDir Command::buildDir(const BuildId &kitId, const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir)
{
	return subDir(buildDir(kitId), provider, package, version, mkDir);
}

QDir Command::tmpDir()
{
	QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
	auto name = QStringLiteral("tmp");
	if(!dir.mkpath(name) || !dir.cd(name))
		throw tr("Failed to create temporary directory");
	return dir;
}

QString Command::dashed(QString option)
{
	if(option.size() == 1)
		return QLatin1Char('-') + option;
	else
		return QStringLiteral("--") + option;
}

void Command::lock(bool isSource, const QString &key)
{
	QString prefix;
	if(isSource)
		prefix = QStringLiteral("src");
	else if(_devMode)
		prefix = QStringLiteral("dev-build");//TODO use local lock
	else
		prefix = QStringLiteral("build");
	if(_locks.contains({isSource, key})){
		throw tr("Resource %{bld}%1/%2%{end} has already been locked")
				.arg(prefix)
				.arg(key);
	}

	auto name = QStringLiteral("%1/%2").arg(prefix).arg(key);
	auto sem = new QSystemSemaphore(name, 1, QSystemSemaphore::Open);
	if(sem->error() != QSystemSemaphore::NoError)
		throw tr("Failed to create semaphore with error: %1").arg(sem->errorString());

	sem->acquire();
	_locks.insert({isSource, key}, sem);
}

void Command::unlock(bool isSource, const QString &key)
{
	auto sem = _locks.take({isSource, key});
	if(sem) {
		sem->release();
		delete sem;
	}
}
