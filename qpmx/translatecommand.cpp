#include "translatecommand.h"

#include <iostream>

TranslateCommand::TranslateCommand(QObject *parent) :
	Command(parent),
	_lconvert(),
	_qmake(),
	_lrelease(),
	_format(),
	_tsFiles(),
	_activeProcs()
{}

void TranslateCommand::initialize(QCliParser &parser)
{
	try {
		//verify all args
		if(parser.isSet(QStringLiteral("lconvert")))
			_lconvert = parser.value(QStringLiteral("lconvert"));

		//qmake kit
		_qmake = parser.value(QStringLiteral("qmake"));

		auto pArgs = parser.positionalArguments();
		if(pArgs.isEmpty())
			throw tr("You must specify the qpmx file to combine translations based on as first argument");
		QFileInfo qpmxFile = pArgs.takeFirst();

		if(pArgs.isEmpty())
			throw tr("You must specify the path to the lrelease binary after the qpmx file as argument, with possible additional arguments");
		forever {
			QString arg = pArgs.takeFirst();
			if(arg == QStringLiteral("%%"))
				break;
			_lrelease.append(arg.replace(QRegularExpression(QStringLiteral("^\\+")), QStringLiteral("-")));
			if(pArgs.isEmpty())
				throw tr("After the lrelease part, seperate via \"%%\" and then pass the ts files to translate");
		}
		xDebug() << tr("Extracted lrelease as: %1").arg(_lrelease.join(QStringLiteral(" ")));

		if(pArgs.isEmpty()) {
			//TODO qtcreator does not "find" warning -> maybe some special syntax is needed?
			xWarning() << tr("No ts-files specified! Make shure to set the TRANSLATIONS variable BEFORE including the qpmx pri");
			qApp->quit();
			return;
		} else
			_tsFiles = pArgs;
		xDebug() << tr("Extracted translation files as: %1").arg(_tsFiles.join(tr(", ")));

		//read the qpmx
		_format = QpmxFormat::readFile(qpmxFile.dir(), qpmxFile.fileName(), true);
		if(_format.source)
			;//TODO
		else
			binTranslate();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void TranslateCommand::errorOccurred(QProcess::ProcessError error)
{
	Q_UNUSED(error)
	auto proc = qobject_cast<QProcess*>(sender());
	if(!proc)
		return;
	if(_activeProcs.remove(proc) == 0)
		return;

	xCritical() << tr("Subcommand \"%1\" failed with error: %2")
				   .arg(proc->program() + QLatin1Char(' ') + proc->arguments().join(QLatin1Char(' ')))
				   .arg(proc->errorString());
	proc->deleteLater();
}

void TranslateCommand::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
	if(exitStatus == QProcess::CrashExit)
		errorOccurred(QProcess::Crashed);
	else {
		auto proc = qobject_cast<QProcess*>(sender());
		if(!proc)
			return;
		if(!_activeProcs.contains(proc))
			return;

		if(exitCode != 0) {
			xCritical() << tr("Subcommand \"%1\" failed with exit code: %2")
						   .arg(proc->program() + QLatin1Char(' ') + proc->arguments().join(QLatin1Char(' ')))
						   .arg(exitCode);
			proc->deleteLater();
			return;
		}

		//forward stdout/err
		std::cout << proc->readAllStandardOutput().toStdString();
		std::cerr << proc->readAllStandardError().toStdString();

		auto emitter = _activeProcs.take(proc);
		proc->deleteLater();

		if(emitter)
			emitter();
		if(_activeProcs.isEmpty()) {
			xDebug() << "Translation generation completed";
			qApp->quit();
		}
	}
}

void TranslateCommand::binTranslate()
{
	if(!QFile::exists(_qmake))
		throw tr("Choosen qmake executable \"%1\" does not exist").arg(_qmake);

	//first: translate all the ts files
	foreach(auto tsFile, _tsFiles) {
		QFileInfo tsInfo(tsFile);
		QString qmFile = tsInfo.completeBaseName() + QStringLiteral(".qm-base");

		auto args = _lrelease;
		args.append({tsFile, QStringLiteral("-qm"), qmFile});
		addTask(args, [this, qmFile](){
			binCombine(qmFile);
		});
	}
}

void TranslateCommand::binCombine(const QString &tmpQmFile)
{
	auto locale = localeString(tmpQmFile);
	if(locale.isNull())
		return;

	QStringList args(_lconvert);
	args.append({QStringLiteral("-if"), QStringLiteral("qm")});
	args.append({QStringLiteral("-i"), tmpQmFile});

	foreach(auto dep, _format.dependencies) {
		auto bDir = buildDir(findKit(_qmake), dep);
		if(!bDir.cd(QStringLiteral("translations")))
			continue;

		bDir.setFilter(QDir::Files | QDir::Readable);
		bDir.setNameFilters({QStringLiteral("*.qm")});
		foreach(auto qmFile, bDir.entryInfoList()) {
			auto baseName = qmFile.completeBaseName();
			if(baseName.endsWith(locale))
				args.append({QStringLiteral("-i"), qmFile.absoluteFilePath()});
		}
	}

	args.append({QStringLiteral("-of"), QStringLiteral("qm")});
	args.append({QStringLiteral("-o"), tmpQmFile.mid(0, tmpQmFile.size() - 5)});

	addTask(args);
}

void TranslateCommand::addTask(QStringList command, std::function<void ()> emitter)
{
	xDebug() << tr("Running subcommand: %1").arg(command.join(QLatin1Char(' ')));

	auto proc = new QProcess(this);
	proc->setProgram(command.takeFirst());
	proc->setArguments(command);

	connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this, &TranslateCommand::finished);
	connect(proc, &QProcess::errorOccurred,
			this, &TranslateCommand::errorOccurred);

	_activeProcs.insert(proc, emitter);
	proc->start(QIODevice::ReadOnly);
}

QString TranslateCommand::localeString(const QString &fileName)
{
	auto parts = QFileInfo(fileName).baseName().split(QLatin1Char('_'));
	do {
		auto nName = parts.join(QLatin1Char('_'));
		QLocale locale(nName);
		if(locale != QLocale::c())
			return QLatin1Char('_') + nName;
		parts.removeFirst();
	} while(!parts.isEmpty());

	xWarning() << tr("Unable to detect locale of file \"%1\". Translations are skipped")
				  .arg(fileName);
	return {};
}
