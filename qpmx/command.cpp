#include "command.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

Command::Command(QObject *parent) :
	QObject(parent),
	_registry(new PluginRegistry(this))
{
	qsrand(QDateTime::currentMSecsSinceEpoch());
}

void Command::finalize() {}

PluginRegistry *Command::registry()
{
	return _registry;
}

QDir Command::srcDir()
{
	QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
	auto name = QStringLiteral("src");
	if(!dir.mkpath(name) || !dir.cd(name))
		throw tr("Failed to create source directory");
	return dir;
}

QDir Command::srcDir(const qpmx::PackageInfo &package, bool mkDir)
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
		throw tr("Failed to create source directory");
	return dir;
}

QDir Command::buildDir(const qpmx::PackageInfo &package, bool mkDir)
{
	return buildDir(package.provider(), package.package(), package.version(), mkDir);
}

QDir Command::buildDir(const QpmxDependency &dep, bool mkDir)
{
	return buildDir(dep.provider, dep.package, dep.version, mkDir);
}

QDir Command::buildDir(const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir)
{
	auto dir = buildDir();
	return subDir(dir, provider, package, version, mkDir);
}

QDir Command::tmpDir()
{
	QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
	auto name = QStringLiteral("tmp");
	if(!dir.mkpath(name) || !dir.cd(name))
		throw tr("Failed to create source directory");
	return dir;
}

QDir Command::subDir(QDir dir, const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir)
{
	if(mkDir) {
		if(provider.isNull())
			return dir;
		if(!dir.mkpath(provider) || !dir.cd(provider))
			throw tr("Failed to create source directory");

		if(package.isNull())
			return dir;
		auto pName = QString::fromUtf8(QUrl::toPercentEncoding(package));
		if(!dir.mkpath(pName) || !dir.cd(pName))
			throw tr("Failed to create source directory");

		if(version.isNull())
			return dir;
		auto VName = version.toString();
		if(!dir.mkpath(VName) || !dir.cd(VName))
			throw tr("Failed to create source directory");

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
