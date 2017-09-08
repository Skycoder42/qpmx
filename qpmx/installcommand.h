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

public slots:
	void initialize(QCliParser &parser) override;

private slots:
	void sourceFetched(int requestId);
	void versionResult(int requestId, QList<QVersionNumber> versions);
	void sourceError(int requestId, const QString &error);

private:
	bool _renew;
	bool _cacheOnly;

	QList<QpmxDependency> _pkgList;
	int _pkgIndex;
	QpmxDependency _current;

	struct SrcAction {
		QString provider;
		QSharedPointer<QTemporaryDir> tDir;
		bool mustWork;
		qpmx::SourcePlugin *plugin;

		SrcAction(QString provider = {}, QTemporaryDir *tDir = nullptr, bool mustWork = false, qpmx::SourcePlugin *plugin = nullptr);
		operator bool() const;
	};

	QHash<int, SrcAction> _actionCache;
	QList<SrcAction> _resCache;
	QSet<qpmx::SourcePlugin*> _connectCache;

	void getNext();
	int randId();
	void connectPlg(qpmx::SourcePlugin *plugin);
	bool getSource(QString provider, qpmx::SourcePlugin *plugin, bool mustWork);
	void completeSource();
	void completeInstall();
	void createSrcInclude(const QpmxFormat &format);
};

#endif // INSTALLCOMMAND_H
