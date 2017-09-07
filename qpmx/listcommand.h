#ifndef LISTCOMMAND_H
#define LISTCOMMAND_H

#include "command.h"

class ListCommand : public Command
{
	Q_OBJECT

public:
	explicit ListCommand(QObject *parent = nullptr);

public slots:
	void initialize(QCliParser &parser) override;

private:
	void listProviders(const QCliParser &parser);
	void listKits(const QCliParser &parser);
};

#endif // LISTCOMMAND_H
