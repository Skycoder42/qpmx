#include "listcommand.h"
#include <iostream>

#include "compilecommand.h"
using namespace qpmx;

#define print(x) std::cout << QString(x).toStdString() << std::endl

ListCommand::ListCommand(QObject *parent) :
	Command(parent)
{}

QString ListCommand::commandName()
{
	return QStringLiteral("list");
}

QString ListCommand::commandDescription()
{
	return tr("List things like providers, qmake versions and other components of qpmx.");
}

QSharedPointer<QCliNode> ListCommand::createCliNode()
{
	auto listNode = QSharedPointer<QCliContext>::create();
	listNode->addOption({
							QStringLiteral("short"),
							QStringLiteral("Only list provider/qmake names, no other details.")
						});
	listNode->addLeafNode(QStringLiteral("providers"),
						  tr("List the package provider backends that are available for qpmx."));
	listNode->addLeafNode(QStringLiteral("kits"),
						  tr("List all currently known Qt kits and the corresponding qmake executable."));

	return listNode;
}

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
		print(tr(" %1 | %2 | %3 ")
			  .arg(tr("Provider"), -10)
			  .arg(tr("Can Search"), -12)
			  .arg(tr("Package Syntax")));
		print(tr("-")
			  .repeated(80)
			  .replace(12, 1, QLatin1Char('|'))
			  .replace(27, 1, QLatin1Char('|')));
		foreach(auto provider, registry()->providerNames()) {
			auto plugin = registry()->sourcePlugin(provider);
			print(tr(" %1 | %2 | %3 ")
				  .arg(provider, -10)
				  .arg(plugin->canSearch() ? tr("yes") : tr("no"), -12)
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
		print(tr(" %1 | %2 | %3 | %4 ")
			  .arg(tr("Qt"), -5)
			  .arg(tr("qmake"), -5)
			  .arg(tr("xspec"), -15)
			  .arg(tr("qmake path")));
		print(tr("-")
			  .repeated(80)
			  .replace(7, 1, QLatin1Char('|'))
			  .replace(15, 1, QLatin1Char('|'))
			  .replace(33, 1, QLatin1Char('|')));

		foreach(auto kit, kits) {
			print(tr(" %1 | %2 | %3 | %4 ")
				  .arg(kit.qtVer.toString(), -5)
				  .arg(kit.qmakeVer.toString(), -5)
				  .arg(kit.xspec, -15)
				  .arg(kit.path));
		}
	}
}
