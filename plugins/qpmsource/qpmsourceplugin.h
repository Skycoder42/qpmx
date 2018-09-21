#ifndef QPMSOURCEPLUGIN_H
#define QPMSOURCEPLUGIN_H

#include "../../qpmx/sourceplugin.h"

#include <QProcess>
#include <QHash>
#include <QSet>

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
	~QpmSourcePlugin() override;

	bool canSearch(const QString &provider) const override;
	bool canPublish(const QString &provider) const override;
	QString packageSyntax(const QString &provider) const override;
	bool packageValid(const qpmx::PackageInfo &package) const override;

	QJsonObject createPublisherInfo(const QString &provider) override;

	QStringList searchPackage(const QString &provider, const QString &query) override;
	QVersionNumber findPackageVersion(const qpmx::PackageInfo &package) override;
	void getPackageSource(const qpmx::PackageInfo &package, const QDir &targetDir) override;
	void publishPackage(const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo) override;

	void cancelAll(int timeout) override;

private:
	QSet<QProcess*> _processCache;
	QHash<qpmx::PackageInfo, QString> _cachedDownloads;

	QProcess *createProcess(const QStringList &arguments, bool keepStdout = false, bool timeout = true);
	QString formatProcError(const QString &type, QProcess *proc);

	bool completeCopyInstall(const qpmx::PackageInfo &package, QDir targetDir, QDir sourceDir);

	void cleanCache(const qpmx::PackageInfo &package);
	void cleanCaches();
	void qpmTransform(const QDir &tDir);
};

#endif // QPMSOURCEPLUGIN_H
