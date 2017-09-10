#ifndef SEARCHCOMMAND_H
#define SEARCHCOMMAND_H

#include "command.h"

class SearchCommand : public Command
{
	Q_OBJECT

public:
	explicit SearchCommand(QObject *parent = nullptr);

public slots:
	void initialize(QCliParser &parser) override;

private slots:
	void searchResult(int requestId, const QStringList &packageNames);
	void sourceError(int requestId, const QString &error);

private:
	bool _short;
	QHash<int, QString> _providerCache;
	QList<QPair<QString, QStringList>> _searchResults;

	void printResult();
};

#endif // SEARCHCOMMAND_H
