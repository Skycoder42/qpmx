#ifndef CLEARCACHESCOMMAND_H
#define CLEARCACHESCOMMAND_H

#include "command.h"

class ClearCachesCommand : public Command
{
	Q_OBJECT

public:
	explicit ClearCachesCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;
};

#endif // CLEARCACHESCOMMAND_H
