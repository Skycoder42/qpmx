#ifndef UPDATECOMMAND_H
#define UPDATECOMMAND_H

#include "command.h"

#include <QQueue>

class UpdateCommand : public Command
{
	Q_OBJECT

public:
	explicit UpdateCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;

private slots:
	void versionResult(int requestId, const QVersionNumber &version);
	void sourceError(int requestId, const QString &error);

private:
	static const int LoadLimit = 5;
	bool _install;
	bool _skipYes;

	QList<QpmxDependency> _pkgList;
	QList<QPair<QpmxDependency, QVersionNumber>> _updateList;
	quint32 _currentLoad;
	QHash<int, QpmxDependency> _actionCache;

	QSet<qpmx::SourcePlugin*> _connectCache;

	void checkNext();
	void completeUpdate();

	void connectPlg(qpmx::SourcePlugin *plugin);
};

#endif // UPDATECOMMAND_H
