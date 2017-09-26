#include "command.h"
#include "qpmxformat.h"

#include "listcommand.h"
#include "searchcommand.h"
#include "installcommand.h"
#include "uninstallcommand.h"
#include "compilecommand.h"
#include "generatecommand.h"
#include "preparecommand.h"
#include "publishcommand.h"
#include "devcommand.h"
#include "initcommand.h"
#include "translatecommand.h"

#include <QCoreApplication>
#include <QException>
#include <QTimer>
#include <QJsonSerializer>
#include <qcliparser.h>

#include <QStandardPaths>
#include <iostream>
using namespace qpmx;

static bool colored = false;
static QSet<QtMsgType> logLevel{QtCriticalMsg, QtFatalMsg};

template <typename T>
static void addCommand(QHash<QString, Command*> &commands);
static void setupParser(QCliParser &parser, const QHash<QString, Command *> &commands);
static void qpmxMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral(TARGET));
	QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
	QCoreApplication::setOrganizationName(QStringLiteral(COMPANY));
	QCoreApplication::setOrganizationDomain(QStringLiteral(BUNDLE));

	QJsonSerializer::registerAllConverters<QJsonObject>();
	QJsonSerializer::registerAllConverters<QpmxDependency>();
	QJsonSerializer::registerAllConverters<QpmxDevDependency>();
	qRegisterMetaType<QVersionNumber>();
	qRegisterMetaType<QList<QVersionNumber>>();
	qRegisterMetaTypeStreamOperators<QVersionNumber>();

	QHash<QString, Command*> commands;
	addCommand<ListCommand>(commands);
	addCommand<SearchCommand>(commands);
	addCommand<InstallCommand>(commands);
	addCommand<UninstallCommand>(commands);
	addCommand<CompileCommand>(commands);
	addCommand<GenerateCommand>(commands);
	addCommand<PrepareCommand>(commands);
	addCommand<PublishCommand>(commands);
	addCommand<DevCommand>(commands);
	addCommand<InitCommand>(commands);
	addCommand<TranslateCommand>(commands);

	QCliParser parser;
	setupParser(parser, commands);
	parser.process(a, true);

	//setup logging
#ifndef Q_OS_WIN
	colored = !parser.isSet(QStringLiteral("no-color"));
	if(colored) {
		qSetMessagePattern(QCoreApplication::translate("parser", "%{if-warning}\033[33mwarning: %{endif}"
																 "%{if-critical}\033[31merror: %{endif}"
																 "%{if-fatal}\033[35mfatal error: %{endif}"
																 "%{if-category}%{category}: %{endif}%{message}"
																 "%{if-warning}\033[0m%{endif}"
																 "%{if-critical}\033[0m%{endif}"
																 "%{if-fatal}\033[0m%{endif}"));
	} else
#endif
	{
		qSetMessagePattern(QCoreApplication::translate("parser", "%{if-warning}warning: %{endif}"
																 "%{if-critical}error: %{endif}"
																 "%{if-fatal}fatal error: %{endif}"
																 "%{if-category}%{category}: %{endif}%{message}"));
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
		cmd->init(parser);
		parser.leaveContext();
	});
	return a.exec();
}

template <typename T>
static void addCommand(QHash<QString, Command*> &commands)
{
	auto cmd = new T(qApp);
	commands.insert(cmd->commandName(), cmd);
}

static void setupParser(QCliParser &parser, const QHash<QString, Command*> &commands)
{
	parser.setApplicationDescription(QCoreApplication::translate("parser", "A frontend for qpm, to provide source and build caching."));
	parser.addHelpOption();
	parser.addVersionOption();

	//global
	parser.addOption({
						 QStringLiteral("verbose"),
						 QCoreApplication::translate("parser", "Enable verbose output.")
					 });
	parser.addOption({
						 {QStringLiteral("q"), QStringLiteral("quiet")},
						 QCoreApplication::translate("parser", "Limit output to error messages only.")
					 });
#ifndef Q_OS_WIN
	parser.addOption({
						 QStringLiteral("no-color"),
						 QCoreApplication::translate("parser", "Do not use colors to highlight output.")
					 });
#endif
	parser.addOption({
						 {QStringLiteral("d"), QStringLiteral("dir")},
						 QCoreApplication::translate("parser", "Set the working <directory>, i.e. the directory to check for the qpmx.json file."),
						 QCoreApplication::translate("parser", "directory"),
						 QDir::currentPath()
					 });

	foreach(auto cmd, commands) {
		parser.addCliNode(cmd->commandName(),
						  cmd->commandDescription(),
						  cmd->createCliNode());
	}
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
		qApp->exit(EXIT_FAILURE);
}
