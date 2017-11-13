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

private slots:
	void packagePublished(int requestId);
	void sourceError(int requestId, const QString &error);

private:
	QVersionNumber _version;
	QQueue<QString> _providers;
	QpmxFormat _format;

	QSet<qpmx::SourcePlugin*> _connectCache;
	QHash<int, QString> _providerCache;

	void publishNext();

	void connectPlg(qpmx::SourcePlugin *plugin);
};

#endif // PUBLISHCOMMAND_H
