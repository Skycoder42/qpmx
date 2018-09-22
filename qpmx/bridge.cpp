#include "bridge.h"
using namespace qpmx::priv;

Bridge::Bridge(Command *command) :
	_command{command}
{}

QDir Bridge::qpmxCacheDir() const
{
	try {
		return _command->cacheDir();
	} catch(QString &e) {
		qFatal("%s", qUtf8Printable(e));
	}
}

QDir Bridge::srcDir() const
{
	try {
		return _command->srcDir();
	} catch(QString &e) {
		qFatal("%s", qUtf8Printable(e));
	}
}

QDir Bridge::buildDir() const
{
	try {
		return _command->buildDir();
	} catch(QString &e) {
		qFatal("%s", qUtf8Printable(e));
	}
}

QDir Bridge::tmpDir() const
{
	try {
		return _command->tmpDir();
	} catch(QString &e) {
		qFatal("%s", qUtf8Printable(e));
	}
}

QString Bridge::pkgEncode(const QString &name) const
{
	return Command::pkgEncode(name);
}

QString Bridge::pkgDecode(QString name) const
{
	return Command::pkgDecode(std::move(name));
}
