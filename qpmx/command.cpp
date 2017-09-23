#include "command.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
using namespace qpmx;

Command::Command(QObject *parent) :
	QObject(parent),
	_registry(PluginRegistry::instance()),
	_settings(new QSettings(this)),
	_locks()
{}

void Command::init(QCliParser &parser)
{
	qsrand(QDateTime::currentMSecsSinceEpoch());
	initialize(parser);
}

void Command::fin()
{
	finalize();
	_registry->cancelAll();
	for(auto it = _locks.begin(); it != _locks.end(); it++) {
		xDebug() << QStringLiteral("Freeing remaining lock for %1/%2")
					.arg(it.key().first ? QStringLiteral("src") : QStringLiteral("build"))
					.arg(it.key().second);
		it.value()->release();
		delete it.value();
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

QList<PackageInfo> Command::readCliPackages(const QStringList &arguments, bool fullPkgOnly) const
{
	QList<PackageInfo> pkgList;

	auto regex = PackageInfo::packageRegexp();
	foreach(auto arg, arguments) {
		auto match = regex.match(arg);
		if(!match.hasMatch())
			throw tr("Malformed package: \"%1\"").arg(arg);

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

QUuid Command::findKit(const QString &qmake) const
{
	QUuid id;
	auto kitCnt = _settings->beginReadArray(QStringLiteral("qt-kits"));
	for(auto i = 0; i < kitCnt; i++) {
		_settings->setArrayIndex(i);
		auto path = _settings->value(QStringLiteral("path")).toString();
		if(path == qmake) {
			id = _settings->value(QStringLiteral("id")).toUuid();
			break;
		}
	}

	_settings->endArray();
	return id;
}

void Command::cleanCaches(const PackageInfo &package)
{
	auto sDir = srcDir(package, false);
	if(!sDir.removeRecursively())
		throw tr("Failed to remove source cache for %1").arg(package.toString());
	auto bDir = buildDir();
	foreach(auto cmpDir, bDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)) {
		buildLock(cmpDir, package);
		auto rDir = buildDir(cmpDir, package, false);
		if(!rDir.removeRecursively())
			throw tr("Failed to remove compilation cache for %1").arg(package.toString());
		buildUnlock(cmpDir, package);
	}
	xDebug() << tr("Removed cached sources and binaries for %1").arg(package.toString());
}

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

QDir Command::srcDir(const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir)
{
	auto dir = srcDir();
	return subDir(dir, provider, package, version, mkDir);
}

QDir Command::buildDir()
{
	QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
	auto name = QStringLiteral("build");
	if(!dir.mkpath(name) || !dir.cd(name))
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
	auto dir = buildDir(kitId);
	return subDir(dir, provider, package, version, mkDir);
}

QDir Command::tmpDir()
{
	QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
	auto name = QStringLiteral("tmp");
	if(!dir.mkpath(name) || !dir.cd(name))
		throw tr("Failed to create temporary directory");
	return dir;
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
		auto pName = QString::fromUtf8(QUrl::toPercentEncoding(package));
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
			auto pName = QString::fromUtf8(QUrl::toPercentEncoding(package));
			path += QStringLiteral("/") + pName;
			if(!version.isNull()) {
				auto VName = version.toString();
				path += QStringLiteral("/") + VName;
			}
		}

		return QDir(dir.absoluteFilePath(path));
	}
}

void Command::lock(bool isSource, const QString &key)
{
	auto prefix = isSource ? QStringLiteral("src") : QStringLiteral("build");
	if(_locks.contains({isSource, key})){
		throw tr("Resource \"%1/%2\" has already been locked")
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
