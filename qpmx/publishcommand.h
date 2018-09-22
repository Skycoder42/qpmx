#ifndef PUBLISHCOMMAND_H
#define PUBLISHCOMMAND_H

#include "command.h"

#include <QQueue>

class PublishCommand : public Command
{
	Q_OBJECT

public:
	explicit PublishCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	QVersionNumber _version;
	QpmxFormat _format;

	void publishPackages(const QStringList &providers);
};

#endif // PUBLISHCOMMAND_H
