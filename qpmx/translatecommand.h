#ifndef TRANSLATECOMMAND_H
#define TRANSLATECOMMAND_H

#include "command.h"

class TranslateCommand : public Command
{
	Q_OBJECT

public:
	explicit TranslateCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	QString _outDir;
	QString _qmake;
	QString _lconvert;
	QString _tsFile;
	QStringList _lrelease;
	QStringList _qpmxTsFiles;

	void binTranslate();
	void srcTranslate();

	void execute(QStringList command);
	QString localeString();
};

#endif // TRANSLATECOMMAND_H
