#ifndef LISTCOMMAND_H
#define LISTCOMMAND_H

#include "command.h"

class ListCommand : public Command
{
	Q_OBJECT

public:
	explicit ListCommand(QObject *parent = nullptr);

	QString commandName() override;
	QString commandDescription() override;
	QSharedPointer<QCliNode> createCliNode() override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	void listProviders(const QCliParser &parser);
	void listKits(const QCliParser &parser);
};

#endif // LISTCOMMAND_H
