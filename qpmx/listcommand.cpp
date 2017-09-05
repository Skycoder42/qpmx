#include "listcommand.h"
#include <iostream>

#define print(x) std::cout << QString(x).toStdString() << std::endl

ListCommand::ListCommand(QObject *parent) :
	Command(parent)
{}

void ListCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.enterContext(QStringLiteral("providers")))
			listProviders(parser);
		else
			Q_UNREACHABLE();
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
	parser.leaveContext();
}

void ListCommand::listProviders(const QCliParser &parser)
{
	if(parser.isSet(QStringLiteral("short")))
		print(registry()->providerNames().join(QLatin1Char(' ')));
	else {
		print(QStringLiteral("%1| Package syntax").arg(QStringLiteral(" Provider"), -30));
		print(QStringLiteral("-").repeated(30) + QLatin1Char('|') + QStringLiteral("-").repeated(49));
		foreach(auto provider, registry()->providerNames()) {
			auto plugin = registry()->sourcePlugin(provider);
			print(QStringLiteral(" %1| %2")
				  .arg(provider, -29)
				  .arg(plugin->packageSyntax()));
		}
	}
}
