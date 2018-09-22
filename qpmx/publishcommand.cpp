#include "publishcommand.h"
using namespace qpmx;

PublishCommand::PublishCommand(QObject *parent) :
	Command{parent}
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

		QStringList providers;
		providers.append(parser.values(QStringLiteral("provider")));
		if(providers.isEmpty()) {
			providers.append(_format.publishers.keys());
			if(providers.isEmpty()) {
				throw tr("Unable to publish package without any providers. "
						 "Run qpmx prepare <provider> to prepare a package for publishing");
			}
		}

		publishPackages(providers);
	} catch (QString &s) {
		xCritical() << s;
	}
}

void PublishCommand::publishPackages(const QStringList &providers)
{
	for(const auto &provider : providers) {
		auto plugin = registry()->sourcePlugin(provider);

		if(!plugin->canPublish(provider))
			throw tr("Provider %{bld}%1%{end} cannot publish packages via qpmx!").arg(provider);
		if(!_format.publishers.contains(provider)) {
			throw tr("Package has not been prepare for provider %{bld}%1%{end}. "
					 "Run the following command to prepare it: qpmx prepare %1")
					.arg(provider);
		}

		try {
			xInfo() << tr("Publishing for provider %{bld}%1%{end}").arg(provider);
			plugin->publishPackage(provider, QDir::current(), _version, _format.publishers.value(provider));
		} catch (qpmx::SourcePluginException &e) {
			throw tr("Failed to publish package for provider %{bld}%1%{end} with error: %2")
					.arg(provider, e.qWhat());
		}
	}

	xDebug() << tr("Package publishing completed");
	qApp->quit();
}
