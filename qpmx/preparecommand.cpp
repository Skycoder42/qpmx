#include "preparecommand.h"
using namespace qpmx;

PrepareCommand::PrepareCommand(QObject *parent) :
	Command(parent)
{}

QString PrepareCommand::commandName() const
{
	return QStringLiteral("prepare");
}

QString PrepareCommand::commandDescription() const
{
	return tr("Prepare a qpmx package for publishing with the given provider.");
}

QSharedPointer<QCliNode> PrepareCommand::createCliNode() const
{
	auto prepareNode = QSharedPointer<QCliLeaf>::create();
	prepareNode->addPositionalArgument(QStringLiteral("provider"),
									   tr("The provider to create the publisher information for."));
	return prepareNode;
}

void PrepareCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.positionalArguments().size() != 1)
			throw tr("You must specify exactly one provider to prepare publishing for");

		auto provider = parser.positionalArguments().value(0);
		auto plugin = registry()->sourcePlugin(provider);
		if(!plugin->canPublish(provider))
			throw tr("Provider %{bld}%1%{end} cannot publish packages via qpmx").arg(provider);

		auto format = QpmxFormat::readDefault();
		format.publishers.insert(provider, plugin->createPublisherInfo(provider));
		QpmxFormat::writeDefault(format);
		qApp->quit();
	} catch(QString &s) {
		xCritical() << s;
	}
}
