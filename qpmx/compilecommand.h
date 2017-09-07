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
		QMake,
		Make,
		Install,
		PriGen
	};

	explicit CompileCommand(QObject *parent = nullptr);

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

	void compileNext();
	void makeStep();
	void qmake();
	void make();
	void install();
	void priGen();

	void initKits(const QStringList &qmakes);
	QtKitInfo createKit(const QString &qmakePath);
	QtKitInfo updateKit(QtKitInfo oldKit, bool mustWork);
	void initProcess();
};

#endif // COMPILECOMMAND_H
