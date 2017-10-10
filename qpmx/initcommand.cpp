#include "initcommand.h"
using namespace qpmx;

InitCommand::InitCommand(QObject *parent) :
	Command(parent)
{}

QString InitCommand::commandName()
{
	return QStringLiteral("init");
}

QString InitCommand::commandDescription()
{
	return tr("Initialize a qpmx based project by downloading and compiling sources, "
			  "as well as generation the required includes. Call\n"
			  "qpmx init --prepare <path_to_profile>\n"
			  "to prepare a pro file to automatically initialize with qmake.");
}

QSharedPointer<QCliNode> InitCommand::createCliNode()
{
	auto initNode = QSharedPointer<QCliLeaf>::create();
	initNode->addOption({
							QStringLiteral("r"),
							tr("Pass the -r option to the install, compile and generate commands."),
					   });
	initNode->addOption({
							{QStringLiteral("e"), QStringLiteral("stderr")},
							tr("Pass the --stderr cli flag to to compile step. This will forward stderr of "
							   "subprocesses instead of logging to a file."),
						});
	initNode->addOption({
							{QStringLiteral("c"), QStringLiteral("clean")},
							tr("Pass the --clean cli flag to the compile step. This will generate clean dev "
							   "builds instead of caching them for speeding builds up."),
						});
	initNode->addOption({
							QStringLiteral("prepare"),
							tr("Prepare the given <pro-file> by adding the qpmx initializations lines. By using this "
							   "option, no initialization is performed."),
							tr("pro-file")
					   });
	initNode->addPositionalArgument(QStringLiteral("qmake-path"),
									tr("The path to the qmake to use for compilation of the qpmx dependencies."));
	initNode->addPositionalArgument(QStringLiteral("outdir"),
									tr("The directory to generate the files in. Passed to the generate step."));
	return initNode;

}

void InitCommand::prepare(const QString &proFile, bool info)
{
	QFile file(proFile);
	if(!file.exists())
		throw tr("Target file \"%1\" does not exist").arg(proFile);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		throw tr("Failed to open pro file \"%1\" with error: %2").arg(proFile).arg(file.errorString());

	QTextStream stream(&file);
	stream << "\n!ReleaseBuild:!DebugBuild:!system(qpmx -d $$shell_quote($$_PRO_FILE_PWD_) --qmake-run init $$QPMX_EXTRA_OPTIONS $$shell_quote($$QMAKE_QMAKE) $$shell_quote($$OUT_PWD)): "
		   << "error(" << tr("qpmx initialization failed. Check the compilation log for details.") << ")\n"
		   << "else: include($$OUT_PWD/qpmx_generated.pri)\n";
	stream.flush();
	file.close();

	auto str = tr("Successfully added qpmx init code to pro file \"%1\"").arg(proFile);
	if(info)
		xInfo() << str;
	else
		xDebug() << str;
}

void InitCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.isSet(QStringLiteral("prepare"))) {
			prepare(parser.value(QStringLiteral("prepare")));
			qApp->quit();
			return;
		}

		auto reRun = parser.isSet(QStringLiteral("r"));

		if(parser.positionalArguments().size() != 2) {
			throw tr("Invalid arguments! You must specify the qmake path to use for compilation "
					 "and the target directory to place the generated files in");
		}
		auto qmake = parser.positionalArguments()[0];
		auto outdir = parser.positionalArguments()[1];

		//run install
		QStringList runArgs {
			QStringLiteral("install")
		};
		if(reRun)
			runArgs.append(QStringLiteral("--renew"));
		subCall(runArgs);
		xDebug() << tr("Successfully ran install step");

		//run compile
		runArgs = QStringList {
			QStringLiteral("compile"),
			QStringLiteral("--qmake"),
			qmake
		};
		if(reRun)
			runArgs.append(QStringLiteral("--recompile"));
		if(parser.isSet(QStringLiteral("stderr")))
			runArgs.append(QStringLiteral("--stderr"));
		if(parser.isSet(QStringLiteral("clean")))
			runArgs.append(QStringLiteral("--clean"));
		subCall(runArgs);
		xDebug() << tr("Successfully ran compile step");

		//run generate
		runArgs = QStringList {
			QStringLiteral("generate"),
			QStringLiteral("--qmake"),
			qmake,
			outdir
		};
		if(reRun)
			runArgs.append(QStringLiteral("--recreate"));
		subCall(runArgs);
		xDebug() << tr("Successfully ran generate step");

		xDebug() << tr("Completed qpmx initialization");
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}
