#include "translatecommand.h"

#include <QStandardPaths>
#include <QProcess>
#include <iostream>
using namespace qpmx;

TranslateCommand::TranslateCommand(QObject *parent) :
	Command(parent),
	_outDir(),
	_qmake(),
	_lconvert(),
	_tsFile(),
	_lrelease(),
	_qpmxTsFiles()
{}

QString TranslateCommand::commandName() const
{
	return QStringLiteral("translate");
}

QString TranslateCommand::commandDescription() const
{
	return tr("Prepare translations by compiling the projects translations and combining "
			  "it with the translations of qpmx packages");
}

QSharedPointer<QCliNode> TranslateCommand::createCliNode() const
{
	auto translateNode = QSharedPointer<QCliLeaf>::create();
	translateNode->setHidden(true);
	translateNode->addOption({
								 QStringLiteral("src"),
								 tr("Assume a source build, and create translations for a source build instead of binary.")
							 });
	translateNode->addOption({
								 QStringLiteral("lconvert"),
								 tr("The <path> to the lconvert binary to be used (binary builds only)."),
								 tr("path")
							 });
	translateNode->addOption({
								 {QStringLiteral("m"), QStringLiteral("qmake")},
								 tr("The <qmake> version to use to find the corresponding translations. If not specified "
									"the qmake from path is used (binary builds only)."),
								 tr("qmake"),
								 QStandardPaths::findExecutable(QStringLiteral("qmake"))
							 });
	translateNode->addOption({
								 QStringLiteral("outdir"),
								 tr("The <directory> to generate the translation files in. If not passed, the current directoy is used."),
								 tr("directory")
							 });
	translateNode->addOption({
								 QStringLiteral("ts-file"),
								 tr("The ts <file> to translate and combine with the qpmx translations (required)."),
								 tr("file")
							 });
	translateNode->addPositionalArgument(QStringLiteral("lrelease"),
										 tr("The path to the lrelease binary, as well as additional arguments.\n"
											"IMPORTANT: Extra arguments like \"-nounfinished\" must NOT be specified with the "
											"leading \"-\"! Instead, use a \"+\". It is replaced internally. Example: \"+nounfinished\"."),
										 QStringLiteral("<lrelease> [<arguments> ...]"));
	translateNode->addPositionalArgument(QStringLiteral("qpmx-translations"),
										 tr("The ts-files to combine with the specified translations. "
											"Typically, the QPMX_TRANSLATIONS qmake variable is passed (source builds only)."),
										 QStringLiteral("[%% <ts-files> ...]"));
	translateNode->addPositionalArgument(QStringLiteral("qpmx-translations-dirs"),
										 tr("The directories containing qm-files to combine with the specified translations. "
											"Typically, the QPMX_TS_DIRS qmake variable is passed (binary builds only)."),
										 QStringLiteral("| [%% <ts-dirs> ...]"));
	return translateNode;
}

void TranslateCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.isSet(QStringLiteral("outdir")))
			_outDir = parser.value(QStringLiteral("outdir")) + QLatin1Char('/');
		else
			_outDir = QStringLiteral("./");
		_qmake = parser.value(QStringLiteral("qmake"));
		_lconvert = parser.value(QStringLiteral("lconvert"));
		_tsFile = parser.value(QStringLiteral("ts-file"));
		if(!QFile::exists(_tsFile))
			throw tr("You must specify the --ts-file option with a valid file!");

		auto pArgs = parser.positionalArguments();
		if(pArgs.isEmpty())
			throw tr("You must specify the path to the lrelease binary after the qpmx file as argument, with possible additional arguments");
		do {
			QString arg = pArgs.takeFirst();
			if(arg == QStringLiteral("%%")) {
				_qpmxTsFiles = pArgs;
				xDebug() << tr("Extracted qpmx translation files/dirs as: %1").arg(_qpmxTsFiles.join(tr(", ")));
				break;
			}
			_lrelease.append(arg.replace(QRegularExpression(QStringLiteral("^\\+")), QStringLiteral("-")));
		} while(!pArgs.isEmpty());
		xDebug() << tr("Extracted lrelease as: %1").arg(_lrelease.join(QStringLiteral(" ")));

		if(parser.isSet(QStringLiteral("src")))
			srcTranslate();
		else
			binTranslate();

		qApp->quit();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void TranslateCommand::binTranslate()
{
	if(!QFile::exists(_qmake))
		throw tr("Choosen qmake executable \"%1\" does not exist").arg(_qmake);

	//first: translate the ts file
	QFileInfo tsInfo(_tsFile);
	QString qmFile = _outDir + tsInfo.completeBaseName() + QStringLiteral(".qm");
	QString qmBaseFile = _outDir + tsInfo.completeBaseName() + QStringLiteral(".qm-base");

	auto args = _lrelease;
	args.append({_tsFile, QStringLiteral("-qm"), qmBaseFile});
	execute(args);

	//now combine them into one
	auto locale = localeString();
	if(locale.isNull()) {
		QFile::rename(qmBaseFile, qmFile);
		return;
	}

	args = QStringList{
		_lconvert,
		QStringLiteral("-if"), QStringLiteral("qm"),
		QStringLiteral("-i"), qmBaseFile
	};

	QList<QpmxDependency> allDeps;
	foreach(auto tsDir, _qpmxTsFiles) {
		QDir bDir(tsDir);
		if(!bDir.exists()) {
			xWarning() << tr("Translation directory does not exist: %1").arg(tsDir);
			continue;
		}

		bDir.setFilter(QDir::Files | QDir::Readable);
		bDir.setNameFilters({QStringLiteral("*.qm")});
		foreach(auto qpmxQmFile, bDir.entryInfoList()) {
			auto baseName = qpmxQmFile.completeBaseName();
			if(baseName.endsWith(locale))
				args.append({QStringLiteral("-i"), qpmxQmFile.absoluteFilePath()});
		}
	}

	args.append({QStringLiteral("-of"), QStringLiteral("qm")});
	args.append({QStringLiteral("-o"), qmFile});
	execute(args);
}

void TranslateCommand::srcTranslate()
{
	QFileInfo tsInfo(_tsFile);
	QString qmFile = _outDir + tsInfo.completeBaseName() + QStringLiteral(".qm");

	QStringList tsFiles(_tsFile);
	auto locale = localeString();
	if(!locale.isNull()) {
		//collect all possible qpmx ts files
		foreach(auto ts, _qpmxTsFiles) {
			auto baseName = QFileInfo(ts).completeBaseName();
			if(baseName.endsWith(locale))
				tsFiles.append(ts);
		}
	}

	auto args = _lrelease;
	args.append(tsFiles);
	args.append({QStringLiteral("-qm"), qmFile});
	execute(args);
}

void TranslateCommand::execute(QStringList command)
{
	xDebug() << tr("Running subcommand: %1").arg(command.join(QLatin1Char(' ')));

	auto pName = command.takeFirst();
	auto res = QProcess::execute(pName, command);
	switch (res) {
	case -2://not started
		throw tr("Failed to start \"%1\" to compile \"%2\"")
				.arg(pName)
				.arg(_tsFile);
		break;
	case -1://crashed
		throw tr("Failed to run \"%1\" to compile \"%2\" - it crashed")
				.arg(pName)
				.arg(_tsFile);
		break;
	case 0://success
		xDebug() << tr("Successfully ran \"%1\" to compile \"%2\"")
					.arg(pName)
					.arg(_tsFile);
		break;
	default:
		throw tr("Running \"%1\" to compile \"%2\" failed with exit code: %3")
				.arg(pName)
				.arg(_tsFile)
				.arg(res);
		break;
	}
}

QString TranslateCommand::localeString()
{
	auto parts = QFileInfo(_tsFile).baseName().split(QLatin1Char('_'));
	do {
		auto nName = parts.join(QLatin1Char('_'));
		QLocale locale(nName);
		if(locale != QLocale::c())
			return QLatin1Char('_') + nName;
		parts.removeFirst();
	} while(!parts.isEmpty());

	xWarning() << tr("Unable to detect locale of file \"%1\". Translation combination is skipped")
				  .arg(_tsFile);
	return {};
}
