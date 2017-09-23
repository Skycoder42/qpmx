#ifndef UNINSTALLCOMMAND_H
#define UNINSTALLCOMMAND_H

#include "command.h"

class UninstallCommand : public Command
{
	Q_OBJECT

public:
	explicit UninstallCommand(QObject *parent = nullptr);

	QString commandName() override;
	QString commandDescription() override;
	QSharedPointer<QCliNode> createCliNode() override;

public slots:
	void initialize(QCliParser &parser) override;

private:
	bool _cached;
	QpmxFormat _format;

	void removePkg(qpmx::PackageInfo package);
};

#endif // UNINSTALLCOMMAND_H
