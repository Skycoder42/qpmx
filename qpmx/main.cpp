#include "command.h"
#include "installcommand.h"
#include "listcommand.h"
#include "qpmxformat.h"

#include <QCoreApplication>
#include <QException>
#include <QTimer>
#include <QJsonSerializer>
#include <qcliparser.h>

#include <iostream>

static QSet<QtMsgType> logLevel{QtCriticalMsg, QtFatalMsg};
static void qpmxMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral(TARGET));
	QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
	QCoreApplication::setOrganizationName(QStringLiteral(COMPANY));
	QCoreApplication::setOrganizationDomain(QStringLiteral(BUNDLE));

	QJsonSerializer::registerAllConverters<QpmxDependency>();

	QCliParser parser;
	parser.setApplicationDescription(QCoreApplication::translate("parser", "Qt package manager X."));//TODO ...
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

	//providers
	auto listNode = parser.addContextNode(QStringLiteral("list"),
										  QCoreApplication::translate("parser", "List things like providers, qmake versions and other components of qpmx."));
	auto listProvidersNode = listNode->addLeafNode(QStringLiteral("providers"),
												   QCoreApplication::translate("parser", "List the package provider backends that are available for qpmx."));
	listProvidersNode->addOption({
									 QStringLiteral("short"),
									 QStringLiteral("Only list provider names, no syntax details")
								 });

	//install
	auto installNode = parser.addLeafNode(QStringLiteral("install"),
										  QCoreApplication::translate("parser", "Install a qpmx package for the current project."));
	installNode->addOption({
							   {QStringLiteral("s"), QStringLiteral("source")},
							   QCoreApplication::translate("parser", "Install the package as sources, not as precompiled static library. This will increase "
																	 "build times, but may help if a package does not work properly. Check the package "
																	 "authors website for hints."),
						   });
	installNode->addOption({
							   {QStringLiteral("n"), QStringLiteral("renew")},
							   QCoreApplication::translate("parser", "Force a reinstallation. If the package was already downloaded, "
																	 "the existing sources and other artifacts will be deleted before downloading."),
						   });
	installNode->addOption({
							   {QStringLiteral("c"), QStringLiteral("cache")},
							   QCoreApplication::translate("parser", "Only download and cache the sources. Do not add the package to a qpmx.json."),
						   });
	installNode->addPositionalArgument(QStringLiteral("packages"),
									   QCoreApplication::translate("parser", "The packages to be installed. The provider determines which backend to use for the download. "
																			 "If left empty, all providers are searched for the package. If the version is left out, "
																			 "the latest version is installed."),
									   QStringLiteral("[<provider>::]<package>[@<version>] ..."));

	//compile
	auto compileNode = parser.addLeafNode(QStringLiteral("compile"),
										  QCoreApplication::translate("parser", "Compile or recompile source packages to generate the precompiled libraries "
																				"for faster building explicitly."));
	compileNode->addOption({
							   {QStringLiteral("m"), QStringLiteral("qmake")},
							   QCoreApplication::translate("parser", "The different <qmake> versions to generate binaries for. Can be specified "
																	 "multiple times. Binaries are precompiled for every qmake in the list. If "
																	 "not specified, binaries are generated for all known qmakes."),
							   QCoreApplication::translate("parser", "qmake")
						   });
	compileNode->addOption({
							   {QStringLiteral("g"), QStringLiteral("global")},
							   QCoreApplication::translate("parser", "Don't limit the packages to rebuild to only the ones specified in the qpmx.json. "
																	 "Instead, build all ever cached packages (limited by the packages argument)."),
						   });
	compileNode->addOption({
							   {QStringLiteral("r"), QStringLiteral("recompile")},
							   QCoreApplication::translate("parser", "By default, compilation is skipped for unchanged packages. By specifying "
																	 "this flags, all packages are recompiled."),
						   });
	compileNode->addPositionalArgument(QStringLiteral("packages"),
									   QCoreApplication::translate("parser", "The packages to compile binaries for. Installed packages are "
																			 "matched against those, and binaries compiled for all of them. If no "
																			 "packages are specified, ALL INSTALLED packages will be compiled."),
									   QStringLiteral("[[<provider>::]<package>[@<version>] ...]"));

	parser.process(a, true);

	//setup logging
#ifndef Q_OS_WIN
	if(!parser.isSet(QStringLiteral("no-color"))) {
		qSetMessagePattern(QStringLiteral("%{if-warning}\033[33m%{endif}"
										  "%{if-critical}\033[31m%{endif}"
										  "%{if-fatal}\033[35m%{endif}"
										  "%{if-category}%{category}: %{endif}%{message}"
										  "%{if-warning}\033[0m%{endif}"
										  "%{if-critical}\033[0m%{endif}"
										  "%{if-fatal}\033[0m%{endif}"));
	}
#endif
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

	Command *cmd = nullptr;
	if(parser.enterContext(QStringLiteral("install")))
		cmd = new InstallCommand(qApp);
	else if(parser.enterContext(QStringLiteral("list")))
		cmd = new ListCommand(qApp);
	else {
		Q_UNREACHABLE();
		return EXIT_FAILURE;
	}

	QObject::connect(qApp, &QCoreApplication::aboutToQuit,
					 cmd, &Command::finalize);
	QTimer::singleShot(0, qApp, [&parser, cmd](){
		cmd->initialize(parser);
		parser.leaveContext();
	});
	return a.exec();
}

static void qpmxMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	if(!logLevel.contains(type))
		return;

	auto message = qFormatLogMessage(type, context, msg);
	if(type == QtDebugMsg || type == QtInfoMsg)
		std::cout << message.toStdString() << std::endl;
	else
		std::cerr << message.toStdString() << std::endl;

	if(type == QtCriticalMsg)
		qApp->exit(EXIT_FAILURE);
}
