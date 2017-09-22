#ifndef DEVCOMMAND_H
#define DEVCOMMAND_H

#include "command.h"

class DevCommand : public Command
{
	Q_OBJECT

public:
	explicit DevCommand(QObject *parent = nullptr);

public slots:
	void initialize(QCliParser &parser) override;

private:
	void addDev(const QCliParser &parser);
	void removeDev(const QCliParser &parser);
};

#endif // DEVCOMMAND_H
