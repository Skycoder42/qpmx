#ifndef TRANSLATECOMMAND_H
#define TRANSLATECOMMAND_H

#include "command.h"

class TranslateCommand : public Command
{
	Q_OBJECT

public:
	explicit TranslateCommand(QObject *parent = nullptr);

public slots:
	void initialize(QCliParser &parser) override;

private:
	QString _lconvert;
	QStringList _lrelease;
	QString _qpmxFile;
	QStringList _tsFiles;
};

#endif // TRANSLATECOMMAND_H
