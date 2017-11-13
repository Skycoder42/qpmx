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

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;

private slots:
	void sourceFetched(int requestId);
	void versionResult(int requestId, QVersionNumber version);
	void existsResult(int requestId);
	void sourceError(int requestId, const QString &error);

private:
	bool _renew;
	bool _noPrepare;

	QList<QpmxDevDependency> _pkgList;
	int _addPkgCount;
	int _pkgIndex;
	QpmxDevDependency _current;

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
		SharedCacheLock lock;

		SrcAction(ResType type = Install,
				  QString provider = {},
				  QTemporaryDir *tDir = nullptr,
				  bool mustWork = false,
				  qpmx::SourcePlugin *plugin = nullptr,
				  SharedCacheLock lock = {});
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
	void detectDeps(const QpmxFormat &format);
};

#endif // INSTALLCOMMAND_H
