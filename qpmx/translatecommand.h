#ifndef TRANSLATECOMMAND_H
#define TRANSLATECOMMAND_H

#include "command.h"

#include <QProcess>
#include <functional>

class TranslateCommand : public Command
{
	Q_OBJECT

public:
	explicit TranslateCommand(QObject *parent = nullptr);

public slots:
	void initialize(QCliParser &parser) override;

private slots:
	void errorOccurred(QProcess::ProcessError error);
	void finished(int exitCode, QProcess::ExitStatus exitStatus);

private:
	QString _lconvert;
	QString _qmake;
	QStringList _lrelease;
	QpmxFormat _format;
	QStringList _tsFiles;

	QHash<QProcess*, std::function<void()>> _activeProcs;

	void binTranslate();
	void binCombine(const QString &tmpQmFile);

	void addTask(QStringList command, std::function<void()> emitter = {});
	QString localeString(const QString &fileName);
};

#endif // TRANSLATECOMMAND_H
