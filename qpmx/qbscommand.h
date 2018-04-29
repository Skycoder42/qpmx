#ifndef QBSCOMMAND_H
#define QBSCOMMAND_H

#include "command.h"

class QbsCommand : public Command
{
	Q_OBJECT
public:
	explicit QbsCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;
};

#endif // QBSCOMMAND_H
