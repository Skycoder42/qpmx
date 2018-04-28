#include "publishcommand.h"
using namespace qpmx;

PublishCommand::PublishCommand(QObject *parent) :
	Command(parent),
	_version(),
	_providers(),
	_format(),
	_connectCache(),
	_providerCache()
{}

QString PublishCommand::commandName() const
{
	return QStringLiteral("publish");
}

QString PublishCommand::commandDescription() const
{
	return tr("Publish a new version of a qpmx package for one or more providers.");
}

QSharedPointer<QCliNode> PublishCommand::createCliNode() const
{
	auto publishNode = QSharedPointer<QCliLeaf>::create();
	publishNode->addOption({
							   {QStringLiteral("p"), QStringLiteral("provider")},
							   tr("Pass the <provider> to push to. Can be specified multiple times. If not specified, "
								  "the package is published for all providers prepared for the package."),
							   tr("provider")
						   });
	publishNode->addPositionalArgument(QStringLiteral("version"),
									   tr("The new package version to publish."));
	return publishNode;
}

void PublishCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.positionalArguments().size() != 1)
			throw tr("You must specify the version to publish as a single parameter");

		_version = QVersionNumber::fromString(parser.positionalArguments().value(0));
		if(_version.isNull())
			throw tr("Invalid version! Please pass a valid version name");
		_format = QpmxFormat::readDefault(true);

		xInfo() << tr("Publishing package for version %1").arg(_version.toString());

		_providers.append(parser.values(QStringLiteral("provider")));
		if(_providers.isEmpty()) {
			_providers.append(_format.publishers.keys());
			if(_providers.isEmpty()) {
				throw tr("Unable to publish package without any providers. "
						 "Run qpmx prepare <provider> to prepare a package for publishing");
			}
		}

		publishNext();
	} catch (QString &s) {
		xCritical() << s;
	}
}

void PublishCommand::packagePublished(int requestId)
{
	auto provider = _providerCache.take(requestId);
	if(provider.isNull())
		return;

	xDebug() << tr("Successfully published package");
	publishNext();
}

void PublishCommand::sourceError(int requestId, const QString &error)
{
	auto provider = _providerCache.take(requestId);
	if(provider.isNull())
		return;

	xCritical() << tr("Failed to publish package for provider %{bld}%1%{end} with error:\n%2")
				   .arg(provider, error);
}

void PublishCommand::publishNext()
{
	if(_providers.isEmpty()) {
		xDebug() << tr("Package publishing completed");
		qApp->quit();
		return;
	}

	auto provider = _providers.dequeue();
	auto plugin = registry()->sourcePlugin(provider);

	if(!plugin->canPublish(provider))
		throw tr("Provider %{bld}%1%{end} cannot publish packages via qpmx!").arg(provider);
	if(!_format.publishers.contains(provider)) {
		throw tr("Package has not been prepare for provider %{bld}%1%{end}. "
				 "Run the following command to prepare it: qpmx prepare %1")
				.arg(provider);
	}

	connectPlg(plugin);
	xInfo() << tr("Publishing for provider %{bld}%1%{end}").arg(provider);

	auto id = randId(_providerCache);
	_providerCache.insert(id, provider);
	plugin->publishPackage(id, provider, QDir::current(), _version, _format.publishers.value(provider));
}

void PublishCommand::connectPlg(SourcePlugin *plugin)
{
	if(_connectCache.contains(plugin))
	   return;

	auto plgobj = dynamic_cast<QObject*>(plugin);
	connect(plgobj, SIGNAL(packagePublished(int)),
			this, SLOT(packagePublished(int)),
			Qt::QueuedConnection);
	connect(plgobj, SIGNAL(sourceError(int,QString)),
			this, SLOT(sourceError(int,QString)),
			Qt::QueuedConnection);
	_connectCache.insert(plugin);
}
