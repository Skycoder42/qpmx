#include "clearcachescommand.h"
using namespace qpmx;

ClearCachesCommand::ClearCachesCommand(QObject *parent) :
	Command(parent)
{}

QString ClearCachesCommand::commandName() const
{
	return QStringLiteral("clean-caches");
}

QString ClearCachesCommand::commandDescription() const
{
	return tr("Removes cached sources, binaries and temporary files.");
}

QSharedPointer<QCliNode> ClearCachesCommand::createCliNode() const
{
	auto cleanNode = QSharedPointer<QCliLeaf>::create();
	cleanNode->addOption({
							 QStringLiteral("no-src"),
							 tr("Keep cached sources, do not delete them, only build files.")
						 });
	cleanNode->addOption({
							 {QStringLiteral("y"), QStringLiteral("yes")},
							 tr("Skip the prompt for running qpmx instances.")
						 });
	return cleanNode;
}

void ClearCachesCommand::initialize(QCliParser &parser)
{
	try {
		if(!parser.isSet(QStringLiteral("yes"))) {
			QFile console;
			if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
				throw tr("Failed to access console with error: %1").arg(console.errorString());
			xWarning() << tr("Make shure no other qpmx instances are running as cleaning caches would corrupt them!");
			console.readLine();
		}

		//clean temporary files
		if(!tmpDir().removeRecursively())
			xWarning() << tr("Failed to completly remove temporary files");
		else
			xDebug() << tr("Removed temporary files");
		if(!buildDir().removeRecursively())
			xWarning() << tr("Failed to completly remove cached build files");
		else
			xDebug() << tr("Removed cached build files");
		if(!parser.isSet(QStringLiteral("no-src"))) {
			if(!srcDir().removeRecursively())
				xWarning() << tr("Failed to completly remove cached sources files");
			else
				xDebug() << tr("Removed cached source files");
		}
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}
