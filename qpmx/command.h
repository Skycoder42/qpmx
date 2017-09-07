#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <QDebug>
#include <QDir>
#include <qcliparser.h>
#include <QUuid>

#include "packageinfo.h"
#include "pluginregistry.h"
#include "qpmxformat.h"

class Command : public QObject
{
	Q_OBJECT

public:
	explicit Command(QObject *parent = nullptr);

public slots:
	virtual void initialize(QCliParser &parser) = 0;
	virtual void finalize();

protected:
	PluginRegistry *registry();

	QUuid findKit(const QString &qmake) const;

	static QDir srcDir();
	static QDir srcDir(const qpmx::PackageInfo &package, bool mkDir = true);
	static QDir srcDir(const QpmxDependency &dep, bool mkDir = true);
	static QDir srcDir(const QString &provider, const QString &package, const QVersionNumber &version = {}, bool mkDir = true);

	static QDir buildDir();
	static QDir buildDir(const qpmx::PackageInfo &package, bool mkDir = true);
	static QDir buildDir(const qpmx::PackageInfo &package, const QUuid &kitId);
	static QDir buildDir(const QpmxDependency &dep, bool mkDir = true);
	static QDir buildDir(const QString &provider, const QString &package, const QVersionNumber &version = {}, bool mkDir = true);

	static QDir tmpDir();

private:
	PluginRegistry *_registry;

	static QDir subDir(QDir dir, const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir);
};

#define xDebug(...) qDebug(__VA_ARGS__).noquote()
#define xInfo(...) qInfo(__VA_ARGS__).noquote()
#define xWarning(...) qWarning(__VA_ARGS__).noquote()
#define xCritical(...) qCritical(__VA_ARGS__).noquote()

#endif // COMMAND_H
