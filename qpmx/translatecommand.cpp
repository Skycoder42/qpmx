#include "translatecommand.h"

TranslateCommand::TranslateCommand(QObject *parent) :
	Command(parent),
	_lconvert(),
	_lrelease(),
	_qpmxFile(),
	_tsFiles()
{}

void TranslateCommand::initialize(QCliParser &parser)
{
	try {
		//verify all args
		if(parser.isSet(QStringLiteral("lconvert")))
			_lconvert = parser.value(QStringLiteral("lconvert"));

		auto pArgs = parser.positionalArguments();
		if(pArgs.isEmpty())
			throw tr("You must specify the qpmx file to combine translations based on as first argument");
		_qpmxFile = pArgs.takeFirst();

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

		qApp->quit();
	} catch(QString &s) {
		xCritical() << s;
	}
}
