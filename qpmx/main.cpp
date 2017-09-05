#include <QCoreApplication>
#include <qcliparser.h>

int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral(TARGET));
	QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
	QCoreApplication::setOrganizationName(QStringLiteral(COMPANY));
	QCoreApplication::setOrganizationDomain(QStringLiteral(BUNDLE));

	QCliParser parser;
	parser.setApplicationDescription(QCoreApplication::translate("parser", "Qt package manager X"));//TODO ...
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

	//install
	auto installNode = parser.addLeafNode(QStringLiteral("install"), QCoreApplication::translate("parser", "Install a qpmx module for the current project."));
	installNode->addOption({
							   QStringLiteral("source"),
							   QCoreApplication::translate("parser", "Install the package as sources, not as precompiled static library. This will increase "
																	 "build times, but may help if a package does not work properly. Check the package "
																	 "authors website for hints."),
						   });
	installNode->addPositionalArgument(QStringLiteral("package"),
									   QCoreApplication::translate("parser", "The package to be installed. The provider determines which backend to use for the download. "
																			 "If left empty, all providers are searched for the package. If the version is left out, "
																			 "the latest version is installed."),
									   QStringLiteral("[<provider>:]<package>[@<version>]"));

	parser.process(a);
	return a.exec();
}