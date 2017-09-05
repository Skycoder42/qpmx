#ifndef INSTALLCOMMAND_H
#define INSTALLCOMMAND_H

#include "command.h"
#include <QTemporaryDir>

class InstallCommand : public Command
{
	Q_OBJECT

public:
	explicit InstallCommand(QObject *parent = nullptr);

public slots:
	void initialize(const QCliParser &parser) override;

private slots:
	void sourceFetched(int requestId);
	void sourceError(int requestId, const QString &error);

private:
	QList<qpmx::PackageInfo> _pkgList;
	qpmx::PackageInfo _current;

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

	void getNext();
	void getSource(QString provider, qpmx::SourcePlugin *plugin, bool mustWork);
	void completeSource();
};

#endif // INSTALLCOMMAND_H
