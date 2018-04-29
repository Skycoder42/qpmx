#include "devcommand.h"
using namespace qpmx;

DevCommand::DevCommand(QObject *parent) :
	Command(parent)
{}

QString DevCommand::commandName() const
{
	return QStringLiteral("dev");
}

QString DevCommand::commandDescription() const
{
	return tr("Switch packages from or to developer mode.");
}

QSharedPointer<QCliNode> DevCommand::createCliNode() const
{
	/*MAJOR further group commands:
	 * dev
	 *  dep
	 *   add
	 *   remove
	 *   commit
	 *	alias
	 *   add
	 *   remove
	 */
	auto devNode = QSharedPointer<QCliContext>::create();
	auto devAddNode = devNode->addLeafNode(QStringLiteral("add"),
										   tr("Add a packages as \"dev package\" to the qpmx.json.user file."));
	devAddNode->addPositionalArgument(QStringLiteral("package"),
									  tr("The package(s) to add as dev package"),
									  QStringLiteral("[<provider>::<package>@<version>"));
	devAddNode->addPositionalArgument(QStringLiteral("pri-path"),
									  tr("The local path to the pri file to be used to replace the preceeding package with."),
									  QStringLiteral("<pri-path> ...]"));

	auto devRemoveNode = devNode->addLeafNode(QStringLiteral("remove"),
											  tr("Remove a \"dev package\" (or alias) from the qpmx.json.user file."));
	devRemoveNode->addOption({
								 QStringLiteral("alias"),
								 tr("Remove an alias instead of a dev dependency")
							 });
	devRemoveNode->addPositionalArgument(QStringLiteral("packages"),
										 tr("The packages to remove from the dev packages"),
										 QStringLiteral("<provider>::<package>@<version> ..."));

	auto devCommitNode = devNode->addLeafNode(QStringLiteral("commit"),
											  tr("Publish a dev package using \"qpmx publish\" and then "
												 "remove it from beeing a dev package."));
	devCommitNode->addPositionalArgument(QStringLiteral("package"),
										 tr("The package(s) to publish and remove from the dev packages."),
										 QStringLiteral("<provider>::<package>@<version>"));
	devCommitNode->addPositionalArgument(QStringLiteral("version"),
										 tr("The new version for the preceeding package to publish."),
										 QStringLiteral("<version> ..."));
	devCommitNode->addOption({
								 QStringLiteral("no-add"),
								 tr("Don't update/add the commited package in the user.json file. By default thats the case "
									"for comitting")
							 });
	devCommitNode->addOption({
								 {QStringLiteral("p"), QStringLiteral("provider")},
								 tr("Pass the <provider> to push to. Can be specified multiple times. If not specified, "
									"the package is published for all providers prepared for the package."),
								 tr("provider")
							 });

	auto devAliasNode = devNode->addLeafNode(QStringLiteral("alias"),
											 tr("Create an alias for a package to replace it in builds."));
	devAliasNode->addPositionalArgument(QStringLiteral("original package"),
										tr("The package(s) to be replaced by an alias"),
										QStringLiteral("[<provider>::<original-package>@<version>"));
	devAliasNode->addPositionalArgument(QStringLiteral("alias package"),
										tr("The package(s) to use as the replacement"),
										QStringLiteral("<provider>::<alias-package>@<version> ...]"));

	return devNode;
}

void DevCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.enterContext(QStringLiteral("add")))
			addDev(parser);
		else if(parser.enterContext(QStringLiteral("alias")))
			addAlias(parser);
		else if(parser.enterContext(QStringLiteral("remove"))) {
			if(parser.isSet(QStringLiteral("alias")))
				removeAlias(parser);
			else
				removeDev(parser);
		} else if(parser.enterContext(QStringLiteral("commit")))
			commitDev(parser);
		else
			Q_UNREACHABLE();

		//clean local caches
		setDevMode(true);
		QDir devCache = buildDir();
		setDevMode(false);
		if(devCache.exists() && !devCache.removeRecursively()) {
			xWarning() << tr("Failed to remove dev caches! This can corrupt your builds. "
							 "Please remove the folder yourself: %{bld}%1%{end}")
						  .arg(devCache.absolutePath());
		}

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
		throw tr("You must specify pairs of package and a local path");

	auto userFormat = QpmxUserFormat::readDefault();
	for(auto i = 0; i < parser.positionalArguments().size(); i += 2) {
		auto pkgList = readCliPackages(parser.positionalArguments().mid(i, 1), true);
		if(pkgList.size() != 1)
			throw tr("You must specify a package and a local path");
		auto pkg = pkgList.first();

		auto path = parser.positionalArguments().value(i + 1);
		if(!QFile::exists(path))
			throw tr("The given local path \"%1\" is not valid").arg(path);

		QpmxDevDependency devDep(pkg, path);
		auto dIdx = userFormat.devDependencies.indexOf(devDep);
		if(dIdx != -1) {
			xWarning() << tr("Package %1 is already a dev dependency. Replacing with the given version and path").arg(devDep.toString());
			userFormat.devDependencies[dIdx] = devDep;
		} else
			userFormat.devDependencies.append(devDep);

		xDebug() << tr("Added package %1 as dev dependency with path \"%2\"")
					.arg(devDep.toString(), path);
	}

	QpmxUserFormat::writeUser(userFormat);
}

void DevCommand::removeDev(const QCliParser &parser)
{
	auto packages = readCliPackages(parser.positionalArguments(), true);

	auto userFormat = QpmxUserFormat::readDefault();
	for(const auto &package : packages){
		userFormat.devDependencies.removeOne(QpmxDevDependency(package));
		xDebug() << tr("Removed package %1 from the dev dependencies")
					.arg(package.toString());
	}

	QpmxUserFormat::writeUser(userFormat);
}

void DevCommand::commitDev(const QCliParser &parser)
{
	if(parser.positionalArguments().isEmpty())
		throw tr("You must specify at least one package and path");
	if((parser.positionalArguments().size() %2) != 0)
		throw tr("You must specify pairs of package and version");

	auto noAdd = parser.isSet(QStringLiteral("no-add"));

	auto userFormat = QpmxUserFormat::readDefault();
	for(auto i = 0; i < parser.positionalArguments().size(); i += 2) {
		auto pkgList = readCliPackages(parser.positionalArguments().mid(i, 1), true);
		if(pkgList.size() != 1)
			throw tr("You must specify a package and a version");
		auto pkg = pkgList.takeFirst();

		QpmxDevDependency dep(pkg);
		auto depIndex = userFormat.devDependencies.indexOf(dep);
		if(depIndex == -1)
			throw tr("Package %1 is not a dev dependency and cannot be commited").arg(pkg.toString());
		dep = userFormat.devDependencies.value(depIndex);

		auto version = QVersionNumber::fromString(parser.positionalArguments().value(i + 1));
		if(version.isNull())
			throw tr("The given version \"%1\" is not valid").arg(version.toString());

		//run qpmx publish
		runPublish(parser.values(QStringLiteral("provider")), dep, version);

		//remove the dev dep
		userFormat.devDependencies.removeOne(dep);
		xDebug() << tr("Removed package %1 from the dev dependencies")
					.arg(pkg.toString());

		//add it to qpmx.json
		if(!noAdd) {
			dep.version = version;//update version and update in normal format
			userFormat.putDependency(dep);
		}
	}

	QpmxUserFormat::writeUser(userFormat);
	if(!noAdd)
		QpmxFormat::writeDefault(userFormat);
}

void DevCommand::addAlias(const QCliParser &parser)
{
	if(parser.positionalArguments().isEmpty())
		throw tr("You must specify at least one package and path");
	if((parser.positionalArguments().size() %2) != 0)
		throw tr("You must specify pairs of original and alias packages");

	auto userFormat = QpmxUserFormat::readDefault();
	for(auto i = 0; i < parser.positionalArguments().size(); i += 2) {
		auto pkgList = readCliPackages(parser.positionalArguments().mid(i, 2), true);
		if(pkgList.size() != 2)
			throw tr("You must specify an original and an alias package");
		auto original = pkgList.first();
		auto alias = pkgList.last();

		QpmxDevAlias devAlias(original, alias);
		auto dIdx = userFormat.devAliases.indexOf(devAlias);
		if(dIdx == -1)
			userFormat.devAliases.append(devAlias);
		else {
			xWarning() << tr("Package %1 already has an alias. Replacing with the given version and path").arg(devAlias.original.toString());
			userFormat.devAliases[dIdx] = devAlias;
		}

		xDebug() << tr("Added package %1 as alias for %2")
					.arg(devAlias.alias.toString(), devAlias.original.toString());
	}

	QpmxUserFormat::writeUser(userFormat);
}

void DevCommand::removeAlias(const QCliParser &parser)
{
	auto packages = readCliPackages(parser.positionalArguments(), true);

	auto userFormat = QpmxUserFormat::readDefault();
	for(const auto &package : packages){
		userFormat.devAliases.removeOne(QpmxDevAlias(package));
		xDebug() << tr("Removed package %1 from the aliases")
					.arg(package.toString());
	}

	QpmxUserFormat::writeUser(userFormat);
}

void DevCommand::runPublish(const QStringList &providers, const QpmxDevDependency &dep, const QVersionNumber &version)
{
	QStringList args = {
		QStringLiteral("publish"),
		version.toString()
	};
	args.reserve(providers.size() + args.size());
	for(const auto &provider : providers)
		args.append({QStringLiteral("--provider"), provider});

	xInfo() << tr("\nPublishing package %1").arg(dep.toString());
	subCall(args, QDir::current().absoluteFilePath(dep.path));
	xDebug() << tr("Successfully published package %1").arg(dep.toString());
}

