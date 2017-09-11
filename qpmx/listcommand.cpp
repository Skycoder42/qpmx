#include "listcommand.h"
#include <iostream>

#include "compilecommand.h"
using namespace qpmx;

#define print(x) std::cout << QString(x).toStdString() << std::endl

ListCommand::ListCommand(QObject *parent) :
	Command(parent)
{}

void ListCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.enterContext(QStringLiteral("providers")))
			listProviders(parser);
		else if(parser.enterContext(QStringLiteral("kits")))
			listKits(parser);
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
		print(QStringLiteral(" %1 | Package syntax").arg(QStringLiteral("Provider"), -28));
		print(QStringLiteral("-").repeated(30) + QLatin1Char('|') + QStringLiteral("-").repeated(49));
		foreach(auto provider, registry()->providerNames()) {
			auto plugin = registry()->sourcePlugin(provider);
			print(QStringLiteral(" %1 | %2")
				  .arg(provider, -28)
				  .arg(plugin->packageSyntax(provider)));
		}
	}
}

void ListCommand::listKits(const QCliParser &parser)
{
	auto kits = QtKitInfo::readFromSettings(settings());

	if(parser.isSet(QStringLiteral("short"))) {
		QStringList paths;
		foreach(auto kit, kits)
			paths.append(kit.path);
		print(paths.join(QLatin1Char(' ')));
	} else {
		print(QStringLiteral(" Qt    | qmake | xspec      | qmake path"));
		print(QString(QStringLiteral("-").repeated(7) + QLatin1Char('|')).repeated(2) +
			  QStringLiteral("-").repeated(12) + QLatin1Char('|') +
			  QStringLiteral("-").repeated(51));
		foreach(auto kit, kits) {
			print(QStringLiteral(" %1 | %2 | %3 | %4")
				  .arg(kit.qtVer.toString(), -5)
				  .arg(kit.qmakeVer.toString(), -5)
				  .arg(kit.xspec, -10)
				  .arg(kit.path));
		}
	}
}
