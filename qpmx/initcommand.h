#ifndef INITCOMMAND_H
#define INITCOMMAND_H

#include "command.h"

class InitCommand : public Command
{
	Q_OBJECT

public:
	explicit InitCommand(QObject *parent = nullptr);

	QString commandName() override;
	QString commandDescription() override;
	QSharedPointer<QCliNode> createCliNode() override;

public slots:
	void initialize(QCliParser &parser) override;

private:
	static QString dashed(QString option);

	void prepare(const QString &proFile);
	void exec(const QString &step, const QStringList &arguments);
};

#endif // INITCOMMAND_H
