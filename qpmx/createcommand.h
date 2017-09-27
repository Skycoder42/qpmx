#ifndef CREATECOMMAND_H
#define CREATECOMMAND_H

#include "command.h"

class CreateCommand : public Command
{
	Q_OBJECT

public:
	explicit CreateCommand(QObject *parent = nullptr);

	QString commandName() override;
	QString commandDescription() override;
	QSharedPointer<QCliNode> createCliNode() override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	void runBaseInit();
	void runPrepare(QStringList baseArgs, const QString &provider);

	bool readBool(QTextStream &stream, bool defaultValue, bool &ok);
};

#endif // CREATECOMMAND_H
