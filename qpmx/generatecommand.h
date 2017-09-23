#ifndef GENERATECOMMAND_H
#define GENERATECOMMAND_H

#include "command.h"

class GenerateCommand : public Command
{
	Q_OBJECT

public:
	explicit GenerateCommand(QObject *parent = nullptr);

public slots:
	void initialize(QCliParser &parser) override;

private:
	QFile *_genFile;
	QString _qmake;

	bool hasChanged(const QpmxUserFormat &current, const QpmxUserFormat &cache);
	void createPriFile(const QpmxUserFormat &current);
};

#endif // GENERATECOMMAND_H
