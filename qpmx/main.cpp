#include "command.h"
#include "qpmxformat.h"

#include "listcommand.h"
#include "searchcommand.h"
#include "installcommand.h"
#include "uninstallcommand.h"
#include "updatecommand.h"
#include "compilecommand.h"
#include "generatecommand.h"
#include "createcommand.h"
#include "preparecommand.h"
#include "publishcommand.h"
#include "devcommand.h"
#include "initcommand.h"
#include "clearcachescommand.h"
#include "translatecommand.h"
#include "hookcommand.h"
#include "qbscommand.h"

#include <QCoreApplication>
#include <QException>
#include <QTimer>
#include <QJsonSerializer>
#include <qcliparser.h>

#include <QStandardPaths>
#include <iostream>

#include <qtcoroutine.h>
using namespace qpmx;

static bool colored = false;
static QSet<QtMsgType> logLevel{QtCriticalMsg, QtFatalMsg};

template <typename T>
static void addCommand(QHash<QString, Command*> &commands);
static void qpmxMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral(TARGET));
	QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
	QCoreApplication::setOrganizationName(QStringLiteral(COMPANY));
	QCoreApplication::setOrganizationDomain(QStringLiteral(BUNDLE));

	QJsonSerializer::registerAllConverters<QpmxDependency>();
	QJsonSerializer::registerAllConverters<QpmxDevDependency>();
	QJsonSerializer::registerAllConverters<QpmxDevAlias>();
	qRegisterMetaTypeStreamOperators<QVersionNumber>();

	QHash<QString, Command*> commands;
	addCommand<ListCommand>(commands);
	addCommand<SearchCommand>(commands);
	addCommand<InstallCommand>(commands);
	addCommand<UninstallCommand>(commands);
	addCommand<UpdateCommand>(commands);
	addCommand<CompileCommand>(commands);
	addCommand<GenerateCommand>(commands);
	addCommand<CreateCommand>(commands);
	addCommand<PrepareCommand>(commands);
	addCommand<PublishCommand>(commands);
	addCommand<DevCommand>(commands);
	addCommand<InitCommand>(commands);
	addCommand<ClearCachesCommand>(commands);
	addCommand<TranslateCommand>(commands);
	addCommand<HookCommand>(commands);
	addCommand<QbsCommand>(commands);

	QCliParser parser;
	Command::setupParser(parser, commands);
	parser.process(a, true);

	//setup logging
	QString prefix;
	if(parser.isSet(QStringLiteral("qmake-run")))
		prefix = QStringLiteral("qpmx.json:1: ");
#ifndef Q_OS_WIN
	colored = !parser.isSet(QStringLiteral("no-color"));
	if(colored) {
		qSetMessagePattern(QCoreApplication::translate("parser", "%{if-warning}\033[33m%1Warning: %{endif}"
																 "%{if-critical}\033[31m%1Error: %{endif}"
																 "%{if-fatal}\033[35m%1Fatal Error: %{endif}"
																 "%{if-category}%{category}: %{endif}%{message}"
																 "%{if-warning}\033[0m%{endif}"
																 "%{if-critical}\033[0m%{endif}"
																 "%{if-fatal}\033[0m%{endif}")
						   .arg(prefix));
	} else
#endif
	{
		qSetMessagePattern(QCoreApplication::translate("parser", "%{if-warning}%1Warning: %{endif}"
																 "%{if-critical}%1Error: %{endif}"
																 "%{if-fatal}%1Fatal Error: %{endif}"
																 "%{if-category}%{category}: %{endif}%{message}")
						   .arg(prefix));
	}

	if(!parser.isSet(QStringLiteral("quiet"))) {
		logLevel.insert(QtWarningMsg);
		logLevel.insert(QtInfoMsg);
		if(parser.isSet(QStringLiteral("verbose")))
			logLevel.insert(QtDebugMsg);
	} else {
		if(parser.isSet(QStringLiteral("verbose")))
			logLevel.insert(QtWarningMsg);
	}
	qInstallMessageHandler(qpmxMessageHandler);

	//perform cd
	if(parser.isSet(QStringLiteral("dir"))){
		if(!QDir::setCurrent(parser.value(QStringLiteral("dir")))) {
			xCritical() << QCoreApplication::translate("parser", "Failed to enter working directory \"%1\"")
						   .arg(parser.value(QStringLiteral("dir")));
			return EXIT_FAILURE;
		}
	}

	Command *cmd = nullptr;
	for(auto it = commands.begin(); it != commands.end(); it++) {
		if(parser.enterContext(it.key())) {
			cmd = it.value();
			break;
		}
	}
	if(!cmd) {
		Q_UNREACHABLE();
		return EXIT_FAILURE;
	}

	QObject::connect(qApp, &QCoreApplication::aboutToQuit,
					 cmd, &Command::fin);
	QTimer::singleShot(0, qApp, [&parser, cmd](){
		QtCoroutine::createAndRun([&parser, cmd](){
			cmd->init(parser);
			parser.leaveContext();
		});
	});
	return a.exec();
}

template <typename T>
static void addCommand(QHash<QString, Command*> &commands)
{
	auto cmd = new T(qApp);
	commands.insert(cmd->commandName(), cmd);
}

static void qpmxMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	if(!logLevel.contains(type))
		return;

	auto message = qFormatLogMessage(type, context, msg);
	if(colored) {
		message.replace(QStringLiteral("%{pkg}"), QStringLiteral("\033[36m"));
		message.replace(QStringLiteral("%{bld}"), QStringLiteral("\033[32m"));
		switch (type) {
		case QtDebugMsg:
		case QtInfoMsg:
			message.replace(QStringLiteral("%{end}"), QStringLiteral("\033[0m"));
			break;
		case QtWarningMsg:
			message.replace(QStringLiteral("%{end}"), QStringLiteral("\033[33m"));
			break;
		case QtCriticalMsg:
			message.replace(QStringLiteral("%{end}"), QStringLiteral("\033[31m"));
			break;
		case QtFatalMsg:
			message.replace(QStringLiteral("%{end}"), QStringLiteral("\033[35m"));
			break;
		default:
			Q_UNREACHABLE();
			break;
		}
	} else {
		message.replace(QStringLiteral("%{pkg}"), QStringLiteral("\""));
		message.replace(QStringLiteral("%{bld}"), QStringLiteral("\""));
		message.replace(QStringLiteral("%{end}"), QStringLiteral("\""));
	}

	if(type == QtDebugMsg || type == QtInfoMsg)
		std::cout << message.toStdString() << std::endl;
	else
		std::cerr << message.toStdString() << std::endl;

	if(type == QtCriticalMsg)
		qApp->exit(Command::exitCode());
}
