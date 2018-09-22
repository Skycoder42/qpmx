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

namespace qpmx {
namespace priv {
class Bridge;
}
}

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
		inline BuildId() = default;
		inline BuildId(const QString &other) :
			QString{other}
		{}
		inline BuildId(QUuid other) :
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
			QString{other.toString(QUuid::WithoutBraces)}
		{}
#else
			QString(other.toString())
		{
			*this = mid(1, size() - 2);
		}
#endif
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
		void relock();

	private:
		CacheLock(const QString &path, int timeout);
		QString _path;
		QScopedPointer<QLockFile> _lock;

		void doLock();
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
	static void replaceAlias(QpmxDependency &original, const QList<QpmxDevAlias> &aliases);

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
	static QString dashed(const QString &option);

private:
	friend class qpmx::priv::Bridge;

	PluginRegistry *_registry;
	QSettings *_settings;
	bool _devMode = false;

	bool _verbose = false;
	bool _quiet = false;
#ifndef Q_OS_WIN
	bool _noColor = false;
#endif
	bool _qmakeRun = false;
	QString _cacheDir;

	QDir cacheDir() const;
	Q_REQUIRED_RESULT CacheLock lock(const QString &name, bool asDev = false) const;
};

#define xDebug(...) qDebug(__VA_ARGS__).noquote()
#define xInfo(...) qInfo(__VA_ARGS__).noquote()
#define xWarning(...) qWarning(__VA_ARGS__).noquote()
#define xCritical(...) qCritical(__VA_ARGS__).noquote()

#endif // COMMAND_H
