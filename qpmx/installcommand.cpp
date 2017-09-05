#include "installcommand.h"
#include <QDebug>
using namespace qpmx;

InstallCommand::InstallCommand(QObject *parent) :
	Command(parent)
{}

void InstallCommand::initialize(const QCliParser &parser)
{
	QList<PackageInfo> packageList;

	auto regex = PackageInfo::packageRegexp();
	foreach(auto arg, parser.positionalArguments()) {
		auto match = regex.match(arg);
		if(!match.hasMatch()) {
			qCritical() << qUtf8Printable(tr("Malformed package:"))
						<< arg;
			qApp->exit(EXIT_FAILURE);
			return;
		}

		PackageInfo info(match.captured(1),
						 match.captured(2),
						 QVersionNumber::fromString(match.captured(3)));
		packageList.append(info);
		qDebug().noquote() << tr("Parsed package: \"%1\" at version %2 (Provider: %3)")
							  .arg(info.package())
							  .arg(info.version().toString())
							  .arg(info.provider());
	}

	if(packageList.isEmpty()) {
		qCritical().noquote() << tr("You must specify at least one package!");
		qApp->exit(EXIT_FAILURE);
		return;
	}

	qApp->quit();
}
