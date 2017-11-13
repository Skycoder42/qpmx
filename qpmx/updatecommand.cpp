#include "updatecommand.h"

UpdateCommand::UpdateCommand(QObject *parent) :
	Command(parent),
	_install(false),
	_skipYes(false),
	_pkgList(),
	_updateList(),
	_currentLoad(0),
	_actionCache(),
	_connectCache()
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
		_currentLoad = 1;//to trigger the loop
		checkNext();
	} catch(QString &s) {
		xCritical() << s;
	}
}

void UpdateCommand::versionResult(int requestId, QVersionNumber version)
{
	if(!_actionCache.contains(requestId))
	   return;
	auto dep = _actionCache.take(requestId);

	if(dep.version != version)
		_updateList.append({dep, version});

	checkNext();
}

void UpdateCommand::sourceError(int requestId, const QString &error)
{
	if(!_actionCache.contains(requestId))
	   return;
	auto dep = _actionCache.take(requestId);

	xCritical() << tr("Failed to fetch latest version of %1 with error:\n%2")
				   .arg(dep.toString())
				   .arg(error);
	checkNext();
}

void UpdateCommand::checkNext()
{
	if(--_currentLoad == 0 && _pkgList.isEmpty()) {
		if(!_updateList.isEmpty())
			completeUpdate();
		else
			xInfo() << tr("All packages are up to date");
		qApp->quit();
		return;
	}

	while(_currentLoad < LoadLimit && !_pkgList.isEmpty()) {
		auto nextDep = _pkgList.takeFirst();
		auto plugin = registry()->sourcePlugin(nextDep.provider);
		if(!plugin->packageValid(nextDep.pkg())) {
			throw tr("The package name %1 is not valid for provider %{bld}%2%{end}")
					.arg(nextDep.package)
					.arg(nextDep.provider);
		}

		xDebug() << tr("Searching for latest version of %1").arg(nextDep.toString());
		//use the latest version -> query for it
		auto id = randId(_actionCache);
		_actionCache.insert(id, nextDep);
		connectPlg(plugin);
		plugin->findPackageVersion(id, nextDep.pkg());
		_currentLoad++;
	}
}

void UpdateCommand::completeUpdate()
{
	QString pkgStr;
	foreach(auto p, _updateList) {
		pkgStr.append(tr("\n * %1 -> %{bld}%2%{end}")
					  .arg(p.first.toString())
					  .arg(p.second.toString()));
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
	foreach(auto p, _updateList) {
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

void UpdateCommand::connectPlg(qpmx::SourcePlugin *plugin)
{
	if(_connectCache.contains(plugin))
	   return;

	auto plgobj = dynamic_cast<QObject*>(plugin);
	connect(plgobj, SIGNAL(versionResult(int,QVersionNumber)),
			this, SLOT(versionResult(int,QVersionNumber)),
			Qt::QueuedConnection);
	connect(plgobj, SIGNAL(sourceError(int,QString)),
			this, SLOT(sourceError(int,QString)),
			Qt::QueuedConnection);
	_connectCache.insert(plugin);
}
