#ifndef UNINSTALLCOMMAND_H
#define UNINSTALLCOMMAND_H

#include "command.h"

class UninstallCommand : public Command
{
	Q_OBJECT

public:
	explicit UninstallCommand(QObject *parent = nullptr);

public slots:
	void initialize(QCliParser &parser) override;

private:
	bool _cached;
	QpmxFormat _format;

	void removePkg(const qpmx::PackageInfo &package);
};

#endif // UNINSTALLCOMMAND_H
