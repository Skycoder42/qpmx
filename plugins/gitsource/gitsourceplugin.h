#ifndef GITSOURCEPLUGIN_H
#define GITSOURCEPLUGIN_H

#include <sourceplugin.h>

#include <QProcess>
#include <QSet>
#include <tuple>

class GitSourcePlugin : public QObject, public qpmx::SourcePlugin
{
	Q_OBJECT
	Q_INTERFACES(qpmx::SourcePlugin)
	Q_PLUGIN_METADATA(IID SourcePlugin_iid FILE "gitsource.json")

public:
	enum ProcessMode {
		Invalid,
		LsRemote,
		Clone,
		Tag,
		Push
	};
	Q_ENUM(ProcessMode)

	GitSourcePlugin(QObject *parent = nullptr);

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
	static QRegularExpression _githubRegex;
	QSet<QProcess*> _processCache;

	QString pkgUrl(const qpmx::PackageInfo &package, QString *prefix = nullptr);
	QString pkgTag(const qpmx::PackageInfo &package);

	QProcess *createProcess(const QStringList &arguments, bool keepStdout = false);
	QString formatProcError(const QString &type, QProcess *proc);
};

#endif // GITSOURCEPLUGIN_H
