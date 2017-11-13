#ifndef INITCOMMAND_H
#define INITCOMMAND_H

#include "command.h"

class InitCommand : public Command
{
	Q_OBJECT

public:
	explicit InitCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

	static void prepare(const QString &proFile, bool info = false);

protected slots:
	void initialize(QCliParser &parser) override;

private:
	void exec(const QString &step, const QStringList &arguments);
};

#endif // INITCOMMAND_H
