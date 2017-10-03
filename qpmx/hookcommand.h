#ifndef HOOKCOMMAND_H
#define HOOKCOMMAND_H

#include "command.h"

class HookCommand : public Command
{
	Q_OBJECT

public:
	explicit HookCommand(QObject *parent = nullptr);

	QString commandName() override;
	QString commandDescription() override;
	QSharedPointer<QCliNode> createCliNode() override;

protected slots:
	void initialize(QCliParser &parser) override;
};

#endif // HOOKCOMMAND_H
