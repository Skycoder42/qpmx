#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <QDebug>
#include <QDir>
#include <qcliparser.h>
#include <QUuid>
#include <QSettings>
#include <QSystemSemaphore>

#include "packageinfo.h"
#include "pluginregistry.h"
#include "qpmxformat.h"

class Command : public QObject
{
	Q_OBJECT

public:
	explicit Command(QObject *parent = nullptr);

	virtual QString commandName() = 0;
	virtual QString commandDescription() = 0;
	virtual QSharedPointer<QCliNode> createCliNode() = 0;

	void init(QCliParser &parser);
	void fin();

protected slots:
	virtual void initialize(QCliParser &parser) = 0;
	virtual void finalize();

protected:
	struct BuildId : public QString
	{
		inline BuildId() :
			QString()
		{}
		inline BuildId(const QString &other) :
			QString(other)
		{}
		inline BuildId(const QUuid &other) :
			QString(other.toString())
		{}
	};

	PluginRegistry *registry();
	QSettings *settings();

	void srcLock(const qpmx::PackageInfo &package);
	void srcLock(const QpmxDependency &dep);
	void srcUnlock(const qpmx::PackageInfo &package);
	void srcUnlock(const QpmxDependency &dep);

	void buildLock(const BuildId &kitId, const qpmx::PackageInfo &package);
	void buildLock(const BuildId &kitId, const QpmxDependency &dep);
	void buildUnlock(const BuildId &kitId, const qpmx::PackageInfo &package);
	void buildUnlock(const BuildId &kitId, const QpmxDependency &dep);

	QList<qpmx::PackageInfo> readCliPackages(const QStringList &arguments, bool fullPkgOnly = false) const;
	static QList<QpmxDependency> depList(const QList<qpmx::PackageInfo> &pkgList);

	QUuid findKit(const QString &qmake) const;
	void cleanCaches(const qpmx::PackageInfo &package);

	void printTable(const QStringList &headers, const QList<int> &minimals, const QList<QStringList> &rows);

	static QDir srcDir();
	static QDir srcDir(const qpmx::PackageInfo &package, bool mkDir = true);
	static QDir srcDir(const QpmxDependency &dep, bool mkDir = true);
	static QDir srcDir(const QString &provider, const QString &package, const QVersionNumber &version = {}, bool mkDir = true);

	static QDir buildDir();
	static QDir buildDir(const BuildId &kitId);
	static QDir buildDir(const BuildId &kitId, const qpmx::PackageInfo &package, bool mkDir = true);
	static QDir buildDir(const BuildId &kitId, const QpmxDependency &dep, bool mkDir = true);
	static QDir buildDir(const BuildId &kitId, const QString &provider, const QString &package, const QVersionNumber &version = {}, bool mkDir = true);

	static QDir tmpDir();

private:
	PluginRegistry *_registry;
	QSettings *_settings;
	QHash<QPair<bool, QString>, QSystemSemaphore*> _locks;

	static QDir subDir(QDir dir, const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir);
	void lock(bool isSource, const QString &key);
	void unlock(bool isSource, const QString &key);
};

#define xDebug(...) qDebug(__VA_ARGS__).noquote()
#define xInfo(...) qInfo(__VA_ARGS__).noquote()
#define xWarning(...) qWarning(__VA_ARGS__).noquote()
#define xCritical(...) qCritical(__VA_ARGS__).noquote()

#endif // COMMAND_H
