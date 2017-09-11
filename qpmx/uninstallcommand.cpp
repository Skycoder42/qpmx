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
		QList<PackageInfo> pkgList;
		auto regex = PackageInfo::packageRegexp();
		foreach(auto arg, parser.positionalArguments()) {//TODO move to command
			auto match = regex.match(arg);
			if(!match.hasMatch())
				throw tr("Malformed package: \"%1\"").arg(arg);

			PackageInfo info(match.captured(1),
							 match.captured(2),
							 QVersionNumber::fromString(match.captured(3)));
			pkgList.append(info);
			xDebug() << tr("Parsed package: \"%1\" at version %2 (Provider: %3)")
						.arg(info.package())
						.arg(info.version().toString())
						.arg(info.provider());
		}

		if(pkgList.isEmpty())
			throw tr("You must specify at least one package!");

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
		//TODO move to command
		auto sDir = srcDir(package, false);
		if(!sDir.removeRecursively())
			throw tr("Failed to remove source cache for %1").arg(package.toString());
		auto bDir = buildDir();
		foreach(auto cmpDir, bDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)) {
			auto rDir = buildDir(cmpDir, package, false);
			if(!rDir.removeRecursively())
				throw tr("Failed to remove compilation cache for %1").arg(package.toString());
		}
		xDebug() << tr("Removed cached sources and binaries for %1").arg(package.toString());
	}
}
