#ifndef COMPILECOMMAND_H
#define COMPILECOMMAND_H

#include "command.h"

#include <QUuid>
#include <QTemporaryDir>
#include <QProcess>

class QtKitInfo
{
public:
	QtKitInfo(const QString &path = {});

	static QList<QtKitInfo> readFromSettings(QSettings *settings);
	static void writeToSettings(QSettings *settings, const QList<QtKitInfo> &kitInfos);

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

	QString commandName() override;
	QString commandDescription() override;
	QSharedPointer<QCliNode> createCliNode() override;

public slots:
	void initialize(QCliParser &parser) override;

private slots:
	void finished(int exitCode, QProcess::ExitStatus exitStatus);
	void errorOccurred(QProcess::ProcessError error);

private:
	bool _recompile;

	QList<qpmx::PackageInfo> _pkgList;
	QList<QtKitInfo> _qtKits;

	qpmx::PackageInfo _current;
	int _kitIndex;
	QtKitInfo _kit;
	QScopedPointer<QTemporaryDir> _compileDir;
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
	void initProcess();

	void initKits(const QStringList &qmakes);
	QtKitInfo createKit(const QString &qmakePath);
	QtKitInfo updateKit(QtKitInfo oldKit, bool mustWork);
};

#endif // COMPILECOMMAND_H
