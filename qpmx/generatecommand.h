#ifndef GENERATECOMMAND_H
#define GENERATECOMMAND_H

#include "command.h"

class GenerateCommand : public Command
{
	Q_OBJECT

public:
	explicit GenerateCommand(QObject *parent = nullptr);

	QString commandName() override;
	QString commandDescription() override;
	QSharedPointer<QCliNode> createCliNode() override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	QFile *_genFile;
	QString _qmake;

	BuildId kitId(const QpmxUserFormat &format) const;
	QpmxCacheFormat cachedFormat(const QpmxUserFormat &format) const;

	bool hasChanged(const QpmxUserFormat &currentUser, const QpmxCacheFormat &cache);
	void createPriFile(const QpmxUserFormat &current);
};

#endif // GENERATECOMMAND_H
