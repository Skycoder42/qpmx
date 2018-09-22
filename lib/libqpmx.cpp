#include "libqpmx.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include "qpmxbridge_p.h"

QDir qpmx::qpmxCacheDir()
{
	return qpmx::priv::QpmxBridge::instance()->qpmxCacheDir();
}

QDir qpmx::srcDir()
{
	return qpmx::priv::QpmxBridge::instance()->srcDir();
}

QDir qpmx::buildDir()
{
	return qpmx::priv::QpmxBridge::instance()->buildDir();
}

QDir qpmx::tmpDir()
{
	return qpmx::priv::QpmxBridge::instance()->tmpDir();
}

QString qpmx::pkgEncode(const QString &name)
{
	return qpmx::priv::QpmxBridge::instance()->pkgEncode(name);
}

QString qpmx::pkgDecode(QString name)
{
	return qpmx::priv::QpmxBridge::instance()->pkgDecode(std::move(name));
}
