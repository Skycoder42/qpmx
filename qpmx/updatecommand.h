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

private:
	static const int LoadLimit = 5;
	bool _install = false;
	bool _skipYes = false;

	QList<QpmxDependency> _pkgList;
	QList<QPair<QpmxDependency, QVersionNumber>> _updateList;

	void checkPackages();
	void checkNext();
	void completeUpdate();
};

#endif // UPDATECOMMAND_H
