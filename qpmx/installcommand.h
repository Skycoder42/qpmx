#ifndef INSTALLCOMMAND_H
#define INSTALLCOMMAND_H

#include "command.h"
#include "qpmxformat.h"
#include <QTemporaryDir>

class InstallCommand : public Command
{
	Q_OBJECT

public:
	explicit InstallCommand(QObject *parent = nullptr);

	QString commandName() override;
	QString commandDescription() override;
	QSharedPointer<QCliNode> createCliNode() override;

protected slots:
	void initialize(QCliParser &parser) override;

private slots:
	void sourceFetched(int requestId);
	void versionResult(int requestId, QVersionNumber version);
	void sourceError(int requestId, const QString &error);

private:
	bool _renew;
	bool _cacheOnly;
	bool _noPrepare;

	QList<QpmxDependency> _pkgList;
	int _pkgIndex;
	QpmxDependency _current;

	struct SrcAction {
		enum ResType {
			Version,
			Install,
			Exists
		} type;
		QString provider;
		QSharedPointer<QTemporaryDir> tDir;
		bool mustWork;
		qpmx::SourcePlugin *plugin;

		SrcAction(ResType type = Install, QString provider = {}, QTemporaryDir *tDir = nullptr, bool mustWork = false, qpmx::SourcePlugin *plugin = nullptr);
		operator bool() const;
	};

	QHash<int, SrcAction> _actionCache;
	QList<SrcAction> _resCache;
	QSet<qpmx::SourcePlugin*> _connectCache;

	void getNext();
	void getSource(QString provider, qpmx::SourcePlugin *plugin, bool mustWork);
	void completeSource();
	void completeInstall();

	void connectPlg(qpmx::SourcePlugin *plugin);
	void createSrcInclude(const QpmxFormat &format);
};

#endif // INSTALLCOMMAND_H
