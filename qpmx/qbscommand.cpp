#include "qbscommand.h"

QbsCommand::QbsCommand(QObject *parent) :
	Command(parent)
{}

QString QbsCommand::commandName() const
{
	return QStringLiteral("qbs");
}

QString QbsCommand::commandDescription() const
{
	return tr("Commands required to use qpmx together with qbs");
}

QSharedPointer<QCliNode> QbsCommand::createCliNode() const
{
	auto qbsNode = QSharedPointer<QCliContext>::create();
	qbsNode->addOption({
						   {QStringLiteral("p"), QStringLiteral("path")},
						   tr("The path to the qbs executable to be used. "
						   "If not specified the system default is used.")
					   });
	qbsNode->addOption({
						   {QStringLiteral("s"), QStringLiteral("settings-dir")},
						   tr("Passed as --settings-dir <dir> to the qbs executable. "
						   "This way custom settings directories can be specified."),
						   QStringLiteral("dir")
					   });

	auto initNode = qbsNode->addLeafNode(QStringLiteral("init"),
										 tr("Initialize the current project by installing, compiling and preparing packages for qbs."));
	initNode->addOption({
							QStringLiteral("r"),
							tr("Pass the -r option to the install, compile and generate commands.")
						});
	initNode->addOption({
							{QStringLiteral("e"), QStringLiteral("stderr")},
							tr("Pass the --stderr cli flag to to compile step. This will forward stderr of "
							   "subprocesses instead of logging to a file.")
						});
	initNode->addOption({
							{QStringLiteral("c"), QStringLiteral("clean")},
							tr("Pass the --clean cli flag to the compile step. This will generate clean dev "
							   "builds instead of caching them for speeding builds up.")
						});

	auto generateNode = qbsNode->addLeafNode(QStringLiteral("generate"),
											 tr("Generate the qbs modules for all given packages."));
	generateNode->addOption({
								{QStringLiteral("r"), QStringLiteral("recreate")},
								tr("Always delete and recreate the files if they exist, not only when the configuration changed."),
						   });

	return qbsNode;
}

void QbsCommand::initialize(QCliParser &parser)
{
	throw QStringLiteral("UNIMPLEMENTED");
}
