#include "command.h"
#include "qpmxformat.h"

#include "listcommand.h"
#include "installcommand.h"
#include "compilecommand.h"
#include "generatecommand.h"

#include <QCoreApplication>
#include <QException>
#include <QTimer>
#include <QJsonSerializer>
#include <qcliparser.h>

#include <QStandardPaths>
#include <iostream>

static bool colored = false;
static QSet<QtMsgType> logLevel{QtCriticalMsg, QtFatalMsg};

static void setupParser(QCliParser &parser);
static void qpmxMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral(TARGET));
	QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
	QCoreApplication::setOrganizationName(QStringLiteral(COMPANY));
	QCoreApplication::setOrganizationDomain(QStringLiteral(BUNDLE));

	QJsonSerializer::registerAllConverters<QpmxDependency>();
	qRegisterMetaType<QVersionNumber>();
	qRegisterMetaType<QList<QVersionNumber>>();
	qRegisterMetaTypeStreamOperators<QVersionNumber>();

	QCliParser parser;
	setupParser(parser);
	parser.process(a, true);

	if(parser.isSet(QStringLiteral("dir"))){
		if(!QDir::setCurrent(parser.value(QStringLiteral("dir")))) {
			xCritical() << QCoreApplication::translate("parser", "Failed to enter working directory \"%1\"")
						   .arg(parser.value(QStringLiteral("dir")));
			return EXIT_FAILURE;
		}
	}

	//setup logging
#ifndef Q_OS_WIN
	colored = !parser.isSet(QStringLiteral("no-color"));
	if(colored) {
		qSetMessagePattern(QCoreApplication::translate("parser", "%{if-warning}\033[33mWarning: %{endif}"
																 "%{if-critical}\033[31mError: %{endif}"
																 "%{if-fatal}\033[35mFatal Error: %{endif}"
																 "%{if-category}%{category}: %{endif}%{message}"
																 "%{if-warning}\033[0m%{endif}"
																 "%{if-critical}\033[0m%{endif}"
																 "%{if-fatal}\033[0m%{endif}"));
	} else
#endif
	{
		qSetMessagePattern(QCoreApplication::translate("parser", "%{if-warning}Warning: %{endif}"
																 "%{if-critical}Error: %{endif}"
																 "%{if-fatal}Fatal Error: %{endif}"
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

	Command *cmd = nullptr;
	if(parser.enterContext(QStringLiteral("list")))
		cmd = new ListCommand(qApp);
	else if(parser.enterContext(QStringLiteral("install")))
		cmd = new InstallCommand(qApp);
	else if(parser.enterContext(QStringLiteral("compile")))
		cmd = new CompileCommand(qApp);
	else if(parser.enterContext(QStringLiteral("generate")))
		cmd = new GenerateCommand(qApp);
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

static void setupParser(QCliParser &parser)
{
	parser.setApplicationDescription(QCoreApplication::translate("parser", "A qpm frontend"));//TODO better description
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

	//list
	auto listNode = parser.addContextNode(QStringLiteral("list"),
										  QCoreApplication::translate("parser", "List things like providers, qmake versions and other components of qpmx."));
	listNode->addOption({
							QStringLiteral("short"),
							QStringLiteral("Only list provider/qmake names, no other details.")
						});
	listNode->addLeafNode(QStringLiteral("providers"),
						  QCoreApplication::translate("parser", "List the package provider backends that are available for qpmx."));
	listNode->addLeafNode(QStringLiteral("kits"),
						  QCoreApplication::translate("parser", "List all currently known Qt kits and the corresponding qmake executable."));

	//install
	auto installNode = parser.addLeafNode(QStringLiteral("install"),
										  QCoreApplication::translate("parser", "Install a qpmx package for the current project."));
	installNode->addOption({
							   {QStringLiteral("r"), QStringLiteral("renew")},
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
																	 "Instead, build all ever cached packages (Ignored if packages are specified as arguments)."),
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
	compileNode->addOption({
							   {QStringLiteral("r"), QStringLiteral("recompile")},
							   QCoreApplication::translate("parser", "By default, compilation is skipped for unchanged packages. By specifying "
																	 "this flags, all packages are recompiled."),
						   });

	//generate
	auto generateNode = parser.addLeafNode(QStringLiteral("generate"),
										   QCoreApplication::translate("parser", "Generate the qpmx_generated.pri, internally used to include compiled packages."));
	generateNode->addPositionalArgument(QStringLiteral("outdir"),
										QCoreApplication::translate("parser", "The directory to generate the file in."));
	generateNode->addOption({
								{QStringLiteral("m"), QStringLiteral("qmake")},
								QCoreApplication::translate("parser", "The <qmake> version to include compiled binaries for. If not specified "
																	  "the qmake from path is be used."),
								QCoreApplication::translate("parser", "qmake"),
								QStandardPaths::findExecutable(QStringLiteral("qmake"))
							});
	generateNode->addOption({
							   {QStringLiteral("r"), QStringLiteral("recreate")},
							   QCoreApplication::translate("parser", "Always delete and recreate the file if it exists, not only when the configuration changed."),
						   });
}

static void qpmxMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	if(!logLevel.contains(type))
		return;

	auto message = qFormatLogMessage(type, context, msg);
	if(colored) {
		message.replace(QStringLiteral("%{pkg}"), QStringLiteral("\033[36m"));
		switch (type) {
		case QtDebugMsg:
		case QtInfoMsg:
			message.replace(QStringLiteral("%{endpkg}"), QStringLiteral("\033[0m"));
			break;
		case QtWarningMsg:
			message.replace(QStringLiteral("%{endpkg}"), QStringLiteral("\033[33m"));
			break;
		case QtCriticalMsg:
			message.replace(QStringLiteral("%{endpkg}"), QStringLiteral("\033[31m"));
			break;
		case QtFatalMsg:
			message.replace(QStringLiteral("%{endpkg}"), QStringLiteral("\033[35m"));
			break;
		default:
			Q_UNREACHABLE();
			break;
		}
	} else {
		message.replace(QStringLiteral("%{pkg}"), QStringLiteral("\""));
		message.replace(QStringLiteral("%{endpkg}"), QStringLiteral("\""));
	}

	if(type == QtDebugMsg || type == QtInfoMsg)
		std::cout << message.toStdString() << std::endl;
	else
		std::cerr << message.toStdString() << std::endl;

	if(type == QtCriticalMsg)
		qApp->exit(EXIT_FAILURE);
}
