#include "devcommand.h"

DevCommand::DevCommand(QObject *parent) :
	Command(parent)
{}

void DevCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.enterContext(QStringLiteral("add")))
			addDev(parser);
		else if(parser.enterContext(QStringLiteral("remove")))
			removeDev(parser);
		else
			Q_UNREACHABLE();
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
	parser.leaveContext();
}

void DevCommand::addDev(const QCliParser &parser)
{
	if(parser.positionalArguments().isEmpty())
		throw tr("You must specify at least one package and path");
	if((parser.positionalArguments().size() %2) != 0)
		throw tr("You must specify pairs of packages and a local paths");

	auto userFormat = QpmxUserFormat::readDefault();
	for(auto i = 0; i < parser.positionalArguments().size(); i += 2) {
		auto pkgList = readCliPackages(parser.positionalArguments().mid(i, 1), true);
		if(pkgList.size() != 1)
			throw tr("You must specify a package and a local path");
		auto pkg = pkgList.takeFirst();

		auto path = parser.positionalArguments()[i + 1];
		if(!QFile::exists(path))
			throw tr("The given local path \"%1\" is not valid").arg(path);

		QpmxDevDependency devDep(pkg, path);
		auto dIdx = userFormat.devmode.indexOf(devDep);
		if(dIdx != -1) {
			xWarning() << tr("Package %1 is already a dependency. Replacing with the given version and path").arg(devDep.toString());
			userFormat.devmode[dIdx] = devDep;
		} else
			userFormat.devmode.append(devDep);

		xDebug() << tr("Added package %1 as dev dependency with path \"%2\"")
					.arg(devDep.toString())
					.arg(path);
	}

	QpmxUserFormat::writeUser(userFormat);
}

void DevCommand::removeDev(const QCliParser &parser)
{
	auto packages = readCliPackages(parser.positionalArguments(), true);

	auto userFormat = QpmxUserFormat::readDefault();
	foreach(auto package, packages){
		userFormat.devmode.removeOne(QpmxDevDependency(package));
		xDebug() << tr("Removed package %1 from the dev dependencies")
					.arg(package.toString());
	}

	QpmxUserFormat::writeUser(userFormat);
}
