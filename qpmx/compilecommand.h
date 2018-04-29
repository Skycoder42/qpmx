#ifndef COMPILECOMMAND_H
#define COMPILECOMMAND_H

#include "command.h"

#include <QUuid>
#include <QTemporaryDir>
#include <QProcess>

class QtKitInfo
{
public:
	QtKitInfo(QString path = {});

	static QUuid findKitId(const QDir &buildDir, const QString &qmake);
	static QList<QtKitInfo> readFromSettings(const QDir &buildDir);
	static void writeToSettings(const QDir &buildDir, const QList<QtKitInfo> &kitInfos);

	operator bool() const;
	bool operator ==(const QtKitInfo &other) const;

	QUuid id;
	QString path;

	QVersionNumber qmakeVer;
	QVersionNumber qtVer;
	QString spec;
	QString xspec;
	QString hostPrefix;
	QString installPrefix;
	QString sysRoot;
};

class BuildDir
{
public:
	BuildDir();
	BuildDir(const QDir &buildDir);

	bool isValid() const;
	void setAutoRemove(bool b);
	QString path() const;
	QString filePath(const QString &fileName) const;

private:
	QTemporaryDir _tDir;
	QDir _pDir;
};

class CompileCommand : public Command
{
	Q_OBJECT

public:
	enum Stage {
		None,
		QMake,
		Make,
		Source,
		PriGen
	};
	Q_ENUM(Stage)

	explicit CompileCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;
	void finalize() override;

private slots:
	void finished(int exitCode, QProcess::ExitStatus exitStatus);
	void errorOccurred(QProcess::ProcessError error);

private:
	bool _recompile;
	bool _fwdStderr;
	bool _clean;

	QList<QpmxDevDependency> _pkgList;
	QList<QpmxDevDependency> _explicitPkg;
	QList<QpmxDevAlias> _aliases;
	QList<QtKitInfo> _qtKits;
#ifndef QPMX_NO_MAKEBUG
	QProcessEnvironment _procEnv;
#endif

	QpmxDevDependency _current;
	int _kitIndex;
	QtKitInfo _kit;
	CacheLock _buildLock;
	QScopedPointer<BuildDir> _compileDir;
	QpmxFormat _format;
	Stage _stage;
	QProcess *_process;
	bool _hasBinary;

	void compileNext();
	void makeStep();
	void qmake();
	void make();
	void install();
	void priGen();

	QString stage();
	void depCollect();
	QString findMake();
	QStringList readMultiVar(const QString &dirName, bool recursive = false);
	QStringList readVar(const QString &fileName);
	void initProcess();
#ifndef QPMX_NO_MAKEBUG
	void setupEnv();
#endif

	void initKits(const QStringList &qmakes);
	QtKitInfo createKit(const QString &qmakePath);
	QtKitInfo updateKit(QtKitInfo oldKit, bool mustWork);
};

#endif // COMPILECOMMAND_H
