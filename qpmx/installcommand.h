#ifndef INSTALLCOMMAND_H
#define INSTALLCOMMAND_H

#include "command.h"

class InstallCommand : public Command
{
	Q_OBJECT

public:
	explicit InstallCommand(QObject *parent = nullptr);

public slots:
	void init(const QCliParser &parser) override;
};

#endif // INSTALLCOMMAND_H
