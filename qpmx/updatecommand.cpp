#include "updatecommand.h"
#include <qtcoroutine.h>

UpdateCommand::UpdateCommand(QObject *parent) :
	Command{parent}
{}

QString UpdateCommand::commandName() const
{
	return QStringLiteral("update");
}

QString UpdateCommand::commandDescription() const
{
	return tr("Update all dependencies of a qpmx.json file to the newest available version.");
}

QSharedPointer<QCliNode> UpdateCommand::createCliNode() const
{
	auto updateNode = QSharedPointer<QCliLeaf>::create();
	updateNode->addOption({
							  {QStringLiteral("i"), QStringLiteral("install")},
							  tr("Don't just update the dependencies, also download the newer sources. "
								 "(This is done automatically on the next qmake, or by running qpmx install)"),
						  });
	updateNode->addOption({
							  {QStringLiteral("y"), QStringLiteral("yes")},
							  tr("Skip the prompt before updating all dependencies."),
						  });
	return updateNode;
}

void UpdateCommand::initialize(QCliParser &parser)
{
	try {
		_install = parser.isSet(QStringLiteral("install"));
		_skipYes = parser.isSet(QStringLiteral("yes"));

		auto format = QpmxFormat::readDefault(true); //no user format, as only real deps are updated
		_pkgList = format.dependencies;
		if(_pkgList.isEmpty()) {
			xWarning() << tr("No dependencies found in qpmx.json. Nothing will be done");
			qApp->quit();
			return;
		}

		xDebug() << tr("Searching for updates for %n package(s) from qpmx.json file", "", _pkgList.size());
		checkPackages();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void UpdateCommand::checkPackages()
{
	quint32 currentLoad = 0;
	QQueue<QtCoroutine::RoutineId> checkQueue;
	QtCoroutine::awaitEach(_pkgList, [this, &checkQueue, &currentLoad](const QpmxDependency &nextDep){
		if(currentLoad >= LoadLimit) {
			checkQueue.enqueue(QtCoroutine::current());
			QtCoroutine::yield();
		}

		auto plugin = registry()->sourcePlugin(nextDep.provider);
		if(!plugin->packageValid(nextDep.pkg())) {
			throw tr("The package name %1 is not valid for provider %{bld}%2%{end}")
					.arg(nextDep.package, nextDep.provider);
		}

		xDebug() << tr("Searching for latest version of %1").arg(nextDep.toString());
		//use the latest version -> query for it
		++currentLoad;
		try {
			auto version = plugin->findPackageVersion(nextDep.pkg());
			if(version > nextDep.version)
				_updateList.append({nextDep, version});
		} catch (qpmx::SourcePluginException &e) {
			xWarning() << tr("Failed to fetch latest version of %1 with error: %2").arg(nextDep.toString())
					   << e.what();
		}
		--currentLoad;

		if(!checkQueue.isEmpty())
			QtCoroutine::resume(checkQueue.dequeue());
	});

	if(!_updateList.isEmpty())
		completeUpdate();
	else
		xInfo() << tr("All packages are up to date");
	qApp->quit();
}

void UpdateCommand::completeUpdate()
{
	QString pkgStr;
	for(const auto &p : qAsConst(_updateList)) {
		pkgStr.append(tr("\n * %1 -> %{bld}%2%{end}")
					  .arg(p.first.toString(), p.second.toString()));
	}

	if(!_skipYes) {
		QFile console;
		if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
			throw tr("Failed to access console with error: %1").arg(console.errorString());
		QTextStream stream(&console);

		xInfo() << tr("Update are available for the packages:") << pkgStr;
		if(!readBool(tr("Do you want to update all packages to the newer versions? (%1) "), stream, true)) {
			xDebug() << tr("Update of qpmx.json was canceled");
			return;
		}
	} else
		xInfo() << tr("Updating the following packages:") << pkgStr;

	auto format = QpmxFormat::readDefault(true);
	for(const auto &p : qAsConst(_updateList)) {
		auto dIndex = format.dependencies.indexOf(p.first);
		if(dIndex != -1)
			format.dependencies[dIndex].version = p.second;
		else
			xWarning() << tr("Updated dependency %1 cannot be found in qpmx.json. Skipping update");
	}

	QpmxFormat::writeDefault(format);
	xDebug() << tr("Update all dependencies to their newest version");

	if(_install)
		subCall({QStringLiteral("install")});
}
