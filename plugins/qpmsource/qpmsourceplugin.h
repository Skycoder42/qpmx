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
		Install,
		Publish
	};
	Q_ENUM(Mode)

	QpmSourcePlugin(QObject *parent = nullptr);

	bool canSearch(const QString &provider) const override;
	bool canPublish(const QString &provider) const override;
	QString packageSyntax(const QString &provider) const override;
	bool packageValid(const qpmx::PackageInfo &package) const override;

	QJsonObject createPublisherInfo(const QString &provider) const override;

	void cancelAll(int timeout) override;

public slots:
	void searchPackage(int requestId, const QString &provider, const QString &query) override;
	void findPackageVersion(int requestId, const qpmx::PackageInfo &package) override;
	void getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir) override;
	void publishPackage(int requestId, const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo) override;

signals:
	void searchResult(int requestId, const QStringList &packageNames) final;
	void versionResult(int requestId, const QVersionNumber &version) final;
	void sourceFetched(int requestId) final;
	void packagePublished(int requestId) final;
	void sourceError(int requestId, const QString &error) final;

private slots:
	void finished(int exitCode, QProcess::ExitStatus exitStatus);
	void errorOccurred(QProcess::ProcessError error);

private:
	typedef std::tuple<int, Mode, QVariantHash> tpl;
	QHash<QProcess*, tpl> _processCache;

	QProcess *createProcess(const QStringList &arguments, bool keepStdout = false, bool timeout = true);
	QString formatProcError(const QString &type, QProcess *proc);

	void completeSearch(int id, QProcess *proc);
	void completeVersion(int id, QProcess *proc);
	void completeInstall(int id, QProcess *proc, const QVariantHash &params);
	void completePublish(int id, QProcess *proc);
};

#endif // QPMSOURCEPLUGIN_H
