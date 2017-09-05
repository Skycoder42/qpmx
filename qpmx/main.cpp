#include "command.h"
#include "installcommand.h"
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

	//install
	auto installNode = parser.addLeafNode(QStringLiteral("install"), QCoreApplication::translate("parser", "Install a qpmx package for the current project."));
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
	installNode->addPositionalArgument(QStringLiteral("packages"),
									   QCoreApplication::translate("parser", "The packages to be installed. The provider determines which backend to use for the download. "
																			 "If left empty, all providers are searched for the package. If the version is left out, "
																			 "the latest version is installed."),
									   QStringLiteral("[<provider>::]<package>[@<version>] ..."));

	parser.process(a);

	//setup logging
#ifndef Q_OS_WIN
	if(!parser.isSet(QStringLiteral("no-color"))) {
		qSetMessagePattern(QStringLiteral("%{if-category}%{category}: %{endif}"
										  "%{if-debug}\033[32m%{message}\033[0m%{endif}"
										  "%{if-info}\033[36m%{message}\033[0m%{endif}"
										  "%{if-warning}\033[33m%{message}\033[0m%{endif}"
										  "%{if-critical}\033[31m%{message}\033[0m%{endif}"
										  "%{if-fatal}\033[35m%{message}\033[0m%{endif}"));
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
	else {
		Q_UNREACHABLE();
		return EXIT_FAILURE;
	}

	QObject::connect(qApp, &QCoreApplication::aboutToQuit,
					 cmd, &Command::finalize);
	QTimer::singleShot(0, qApp, [&parser, cmd](){
		cmd->initialize(parser);
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
