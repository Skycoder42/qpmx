#ifndef QPMSOURCEPLUGIN_H
#define QPMSOURCEPLUGIN_H

#include "../../qpmx/sourceplugin.h"

#include <QProcess>
#include <QHash>

class QpmSourcePlugin : public QObject, public qpmx::SourcePlugin
{
	Q_OBJECT
	Q_INTERFACES(qpmx::SourcePlugin)
	Q_PLUGIN_METADATA(IID SourcePlugin_iid FILE "qpmsource.json")

public:
	enum Mode {
		Search,
		Version,
		Install
	};
	Q_ENUM(Mode)

	QpmSourcePlugin(QObject *parent = nullptr);

	bool canSearch() const override;
	QString packageSyntax(const QString &provider) const override;
	bool packageValid(const qpmx::PackageInfo &package) const override;

	void cancelAll(int timeout) override;

public slots:
	void searchPackage(int requestId, const QString &provider, const QString &query) override;
	void findPackageVersion(int requestId, const qpmx::PackageInfo &package) override;
	void getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir) override;

signals:
	void searchResult(int requestId, const QStringList &packageNames) override;
	void versionResult(int requestId, const QVersionNumber &version) override;
	void sourceFetched(int requestId) override;
	void sourceError(int requestId, const QString &error) override;

private slots:
	void finished(int exitCode, QProcess::ExitStatus exitStatus);
	void errorOccurred(QProcess::ProcessError error);

private:
	typedef std::tuple<int, Mode, QDir, QString> tpl;
	QHash<QProcess*, tpl> _processCache;

	QDir createLogDir(const QString &action);
	QProcess *createProcess(const QString &type, const QStringList &arguments, bool stdLog = false);

	void completeSearch(int id, QProcess *proc);
	void completeVersion(int id, QProcess *proc);
	void completeInstall(int id, QProcess *proc, QDir tDir, QString package);
};

#endif // QPMSOURCEPLUGIN_H
