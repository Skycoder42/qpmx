#ifndef HOOKCOMMAND_H
#define HOOKCOMMAND_H

#include "command.h"

class HookCommand : public Command
{
	Q_OBJECT

public:
	explicit HookCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	void createHookSrc(const QStringList &args, QIODevice *out);
	void createHookCompile(const QString &inFile, QIODevice *out);
};

#endif // HOOKCOMMAND_H
