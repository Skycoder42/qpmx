#ifndef SEARCHCOMMAND_H
#define SEARCHCOMMAND_H

#include "command.h"

class SearchCommand : public Command
{
	Q_OBJECT

public:
	explicit SearchCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	bool _short = false;
	QList<QPair<QString, QStringList>> _searchResults;

	void performSearch(const QString &query, const QStringList &providers);
	void printResult();
};

#endif // SEARCHCOMMAND_H
