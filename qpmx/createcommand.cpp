#include "createcommand.h"

#include <iostream>
using namespace qpmx;

#define print(x) do { \
	std::cout << QString(x).toStdString(); \
	std::cout.flush(); \
} while(false)

CreateCommand::CreateCommand(QObject *parent) :
	Command{parent}
{}

QString CreateCommand::commandName() const
{
	return QStringLiteral("create");
}

QString CreateCommand::commandDescription() const
{
	return tr("Create a new qpmx package by creating the initial qpmx.json "
			  "(or setting up an exiting one).");
}

QSharedPointer<QCliNode> CreateCommand::createCliNode() const
{
	auto createNode = QSharedPointer<QCliLeaf>::create();
	createNode->addOption({
							  {QStringLiteral("p"), QStringLiteral("prepare")},
							  tr("After creating the qpmx.json, prepare it for the given <provider>. "
								 "Can be specified multiple times. Preparing is needed for later publishing via qpmx."),
							  tr("provider")
						  });
	return createNode;
}

void CreateCommand::initialize(QCliParser &parser)
{
	try {
		if(!parser.positionalArguments().isEmpty())
			throw tr("No additional arguments allowed");

		runBaseInit();

		auto providers = parser.values(QStringLiteral("prepare"));
		if(!providers.isEmpty()) {
			for(const auto &provider : providers)
				runPrepare(provider);
		}

		xDebug() << tr("Finish qpmx.json creation");
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}

void CreateCommand::runBaseInit()
{
	QFile console;
	if(!console.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
		throw tr("Failed to access console with error: %1").arg(console.errorString());
	QTextStream stream(&console);

	auto format = QpmxFormat::readDefault();

	if(format.priFile.isEmpty())
		format.priFile = QStringLiteral("qpmx.pri");
	print(tr("Enter the name of the build pri-file [%1]: ").arg(format.priFile));
	auto read = stream.readLine().trimmed();
	if(!read.isEmpty())
		format.priFile = read;

	format.source = readBool(tr("Source builds only? (%1) "), stream, format.source);

	if(!format.source) {
		if(format.prcFile.isEmpty())
			print(tr("Enter the name of the compiled include pri-file (optional): "));
		else
			print(tr("Enter the name of the compiled include pri-file [%1]: ").arg(format.prcFile));
		read = stream.readLine().trimmed();
		if(!read.isEmpty())
			format.prcFile = read;
	}

	if(format.license.name.isEmpty())
		print(tr("Enter the type of license your are using (optional, e.g. \"MIT\"): "));
	else
		print(tr("Enter the type of license your are using [%1]: ").arg(format.license.name));
	read = stream.readLine().trimmed();
	if(!read.isEmpty())
		format.license.name = read;

	if(format.license.file.isEmpty()) {
		//try to find the default license file
		auto dir = QDir::current();
		dir.setFilter(QDir::Files | QDir::Readable);
		dir.setSorting(QDir::Name);
		dir.setNameFilters({QStringLiteral("*LICENSE*")});
		auto list = dir.entryList();
		if(!list.isEmpty())
			format.license.file = list.first();
		else
			format.license.file = QStringLiteral("LICENSE");
	}
	print(tr("Enter the file name of the license your are using [%1]: ").arg(format.license.file));
	read = stream.readLine().trimmed();
	if(!read.isEmpty())
		format.license.file = read;

	auto generate = readBool(tr("Generate templates for non existant files? (%1) "), stream, true);

	QpmxFormat::writeDefault(format);
	xDebug() << tr("Created qpmx.json file");

	if(generate) {
		if(!QFile::exists(format.priFile)) {
			if(!QFile::copy(QStringLiteral(":/build/default.pri"), format.priFile))
				throw tr("Failed to create %1 file").arg(format.priFile);

			QFile priFile(format.priFile);
			priFile.setPermissions(priFile.permissions() | QFile::WriteUser | QFile::WriteOwner);
			if(!format.prcFile.isEmpty()) {
				if(!priFile.open(QIODevice::Append | QIODevice::Text)) {
					throw tr("Failed to open %1 file with error: %2")
							.arg(priFile.fileName(), priFile.errorString());
				}

				QTextStream priStream(&priFile);
				priStream << "\ninclude($$PWD/" << format.prcFile << ")\n";
				priStream.flush();
				priFile.close();
				xDebug() << tr("Created default pri-file with prc include");
			} else
				xDebug() << tr("Created default pri-file");
		}

		if(!format.prcFile.isEmpty() && !QFile::exists(format.prcFile)) {
			QFile prcFile(format.prcFile);
			if(!prcFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
				throw tr("Failed to create %1 file with error: %2")
						.arg(prcFile.fileName(), prcFile.errorString());
			}
			prcFile.write("\n");
			prcFile.close();
			xDebug() << tr("Created empty prc-file");
		}
	}
}

void CreateCommand::runPrepare(const QString &provider)
{
	QStringList args {
		QStringLiteral("prepare"),
		provider
	};

	xInfo() << tr("\nPreparing qpmx.json for provider %{bld}%1%{end}").arg(provider);
	subCall(args);
	xDebug() << tr("Successfully prepare for provider %{bld}%1%{end}").arg(provider);
}
