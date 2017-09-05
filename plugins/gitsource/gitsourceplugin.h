#ifndef GITSOURCEPLUGIN_H
#define GITSOURCEPLUGIN_H

#include "../../qpmx/sourceplugin.h"

#include <QProcess>
#include <QHash>

class GitSourcePlugin : public QObject, public qpmx::SourcePlugin
{
	Q_OBJECT
	Q_INTERFACES(qpmx::SourcePlugin)
	Q_PLUGIN_METADATA(IID SourcePlugin_iid FILE "gitsource.json")

public:
	GitSourcePlugin(QObject *parent = nullptr);

	QString packageSyntax() const override;
	bool packageValid(const qpmx::PackageInfo &package) const override;

public slots:
	void searchPackage(int requestId, const QString &provider, const QString &query) override;
	void getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir, const QVariantHash &extraParameters) override;

signals:
	void searchResult(int requestId, const QStringList &package) final;
	void sourceFetched(int requestId) final;
	void sourceError(int requestId, const QString &error) final;

private slots:
	void finished(int exitCode, QProcess::ExitStatus exitStatus);
	void errorOccurred(QProcess::ProcessError error);


private:
	QHash<QProcess*, int> _processCache;
};

#endif // GITSOURCEPLUGIN_H
