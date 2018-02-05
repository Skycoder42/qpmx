#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <QDebug>
#include <QDir>
#include <qcliparser.h>
#include <QUuid>
#include <QSettings>
#include <QLockFile>

#include "packageinfo.h"
#include "pluginregistry.h"
#include "qpmxformat.h"

class Command : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool devMode READ devMode WRITE setDevMode)

public:
	explicit Command(QObject *parent = nullptr);

	virtual QString commandName() const = 0;
	virtual QString commandDescription() const = 0;
	virtual QSharedPointer<QCliNode> createCliNode() const = 0;

	static void setupParser(QCliParser &parser, const QHash<QString, Command*> &commands);

	void init(QCliParser &parser);
	void fin();

	static int exitCode();
	static QDir subDir(QDir dir, const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir);

protected slots:
	virtual void initialize(QCliParser &parser) = 0;
	virtual void finalize();

protected:
	static int _ExitCode;

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
		{
			*this = mid(1, size() - 2);
		}
	};

	class CacheLock
	{
		friend class Command;
		friend class SharedCacheLock;
		Q_DISABLE_COPY(CacheLock)

	public:
		CacheLock();
		CacheLock(CacheLock &&mv);
		CacheLock &operator=(CacheLock &&mv);
		~CacheLock();

		bool isLocked() const;
		void free();

	private:
		CacheLock(const QString &path, int timeout);
		QString _path;
		QScopedPointer<QLockFile> _lock;
	};

	class SharedCacheLock : public QSharedPointer<CacheLock>
	{
	public:
		SharedCacheLock();
		SharedCacheLock(const SharedCacheLock &other) = default;
		SharedCacheLock &operator=(const CacheLock &other);
		SharedCacheLock(CacheLock &&mv);
		SharedCacheLock &operator=(CacheLock &&mv);

		const CacheLock &lockRef() const;
	};

	PluginRegistry *registry() const;
	QSettings *settings() const;

	void setDevMode(bool devModeActive);
	bool devMode() const;

	Q_REQUIRED_RESULT CacheLock pkgLock(const qpmx::PackageInfo &package) const;
	Q_REQUIRED_RESULT CacheLock pkgLock(const QpmxDependency &dep) const;
	Q_REQUIRED_RESULT CacheLock pkgLock(const QpmxDevDependency &dep) const;
	Q_REQUIRED_RESULT CacheLock kitLock() const;

	QList<qpmx::PackageInfo> readCliPackages(const QStringList &arguments, bool fullPkgOnly = false) const;
	static QList<QpmxDependency> depList(const QList<qpmx::PackageInfo> &pkgList);
	static QList<QpmxDevDependency> devDepList(const QList<qpmx::PackageInfo> &pkgList);
	template <typename T>
	int randId(QHash<int, T> &cache) const;

	void cleanCaches(const qpmx::PackageInfo &package, const SharedCacheLock &sharedSrcLockRef) const;
	void cleanCaches(const qpmx::PackageInfo &package, const CacheLock &srcLockRef) const;

	bool readBool(const QString &message, QTextStream &stream, bool defaultValue) const;
	void printTable(const QStringList &headers, const QList<int> &minimals, const QList<QStringList> &rows) const;
	void subCall(QStringList arguments, const QString &workingDir = {}) const;

	QDir srcDir() const;
	QDir srcDir(const qpmx::PackageInfo &package, bool mkDir = false) const;
	QDir srcDir(const QpmxDependency &dep, bool mkDir = false) const;
	QDir srcDir(const QpmxDevDependency &dep, bool mkDir = false) const;
	QDir srcDir(const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir = false) const;

	QDir buildDir() const;
	QDir buildDir(const BuildId &kitId) const;
	QDir buildDir(const BuildId &kitId, const qpmx::PackageInfo &package, bool mkDir = false) const;
	QDir buildDir(const BuildId &kitId, const QpmxDependency &dep, bool mkDir = false) const;
	QDir buildDir(const BuildId &kitId, const QString &provider, const QString &package, const QVersionNumber &version, bool mkDir = false) const;

	QDir tmpDir() const;
	QDir lockDir(bool asDev) const;

	static QString pkgEncode(const QString &name);
	static QString pkgDecode(QString name);
	static QString dashed(QString option);

private:
	PluginRegistry *_registry;
	QSettings *_settings;
	bool _devMode;

	bool _verbose;
	bool _quiet;
#ifndef Q_OS_WIN
	bool _noColor;
#endif
	bool _qmakeRun;
	QString _cacheDir;

	QDir cacheDir() const;
	Q_REQUIRED_RESULT CacheLock lock(const QString &name, bool asDev = false) const;
};

template <typename T>
int Command::randId(QHash<int, T> &cache) const
{
	int id;
	do {
		id = qrand();
	} while(cache.contains(id));
	return id;
}

#define xDebug(...) qDebug(__VA_ARGS__).noquote()
#define xInfo(...) qInfo(__VA_ARGS__).noquote()
#define xWarning(...) qWarning(__VA_ARGS__).noquote()
#define xCritical(...) qCritical(__VA_ARGS__).noquote()

#endif // COMMAND_H
