#include "uninstallcommand.h"
using namespace qpmx;

UninstallCommand::UninstallCommand(QObject *parent) :
	Command(parent),
	_cached(false),
	_format()
{}

void UninstallCommand::initialize(QCliParser &parser)
{
	try {
		_cached = parser.isSet(QStringLiteral("cached"));

		if(parser.positionalArguments().isEmpty())
			throw tr("You must specify at least 1 package to remove.");

		xDebug() << tr("Uninstalling %n package(s) from the command line", "", parser.positionalArguments().size());
		auto pkgList = readCliPackages(parser.positionalArguments());

		_format = QpmxFormat::readDefault();
		foreach(auto pkg, pkgList)
			removePkg(pkg);
		QpmxFormat::writeDefault(_format);

		xDebug() << tr("Package uninstallation completed");
		qApp->quit();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void UninstallCommand::removePkg(PackageInfo package)
{
	auto found = false;
	if(package.provider().isNull()) {
		foreach(auto dep, _format.dependencies) {
			if(dep.package == package.package() &&
			   (package.version().isNull() ||
				package.version() == dep.version)) {
				package = dep.pkg();
				found = true;
			}
		}

		if(!found)
			throw tr("Failed to find a full package in qpmx.json that matches %1").arg(package.toString());
	} else if(package.version().isNull()) {
		foreach(auto dep, _format.dependencies) {
			if(dep.package == package.package() &&
			   dep.provider == package.provider()) {
				package = dep.pkg();
				found = true;
			}
		}

		if(!found)
			throw tr("Failed to find a full package in qpmx.json that matches %1").arg(package.toString());
	}

	if(!_format.dependencies.removeOne(package))
		xWarning() << tr("Package %1 not found in qpmx.json").arg(package.toString());
	else
		xDebug() << tr("Removed package %1 from qpmx.json").arg(package.toString());

	if(_cached) {
		srcLock(package);
		cleanCaches(package);
		srcUnlock(package);
	}
}
