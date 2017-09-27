#include "initcommand.h"

#include <QProcess>
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

void InitCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.isSet(QStringLiteral("prepare"))) {
			prepare(parser.value(QStringLiteral("prepare")));
			qApp->quit();
			return;
		}

		if(parser.positionalArguments().size() != 2) {
			throw tr("Invalid arguments! You must specify the qmake path to use for compilation "
					 "and the target directory to place the generated files in");
		}
		auto qmake = parser.positionalArguments()[0];
		auto outdir = parser.positionalArguments()[1];

		//collect base arguments by copying all options
		QStringList baseArguments;
		foreach(auto opt, QSet<QString>::fromList(parser.optionNames())) {
			if(opt == QStringLiteral("e") || opt == QStringLiteral("stderr"))
				continue;
			auto values = parser.values(opt);
			if(values.isEmpty())
				baseArguments.append(dashed(opt));
			else {
				foreach(auto value, values)
					baseArguments.append({dashed(opt), value});
			}
		}

		//run install
		auto runArgs = baseArguments;
		runArgs.append(QStringLiteral("install"));
		exec(QStringLiteral("install"), runArgs);

		//run compile
		runArgs = baseArguments;
		if(parser.isSet(QStringLiteral("stderr")))
			runArgs.append(QStringLiteral("--stderr"));
		runArgs.append({
						   QStringLiteral("compile"),
						   QStringLiteral("--qmake"),
						   qmake
					   });
		exec(QStringLiteral("compile"), runArgs);

		//run generate
		runArgs = baseArguments;
		runArgs.append({
						   QStringLiteral("generate"),
						   QStringLiteral("--qmake"),
						   qmake,
						   outdir
					   });
		exec(QStringLiteral("generate"), runArgs);

		xDebug() << tr("Completed qpmx initialization");
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}

void InitCommand::prepare(const QString &proFile)
{
	QFile file(proFile);
	if(!file.exists())
		throw tr("Target file \"%1\" does not exist").arg(proFile);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		throw tr("Failed to open pro file \"%1\" with error: %2").arg(proFile).arg(file.errorString());

	QTextStream stream(&file);
	stream << "\nsystem(qpmx -d $$shell_quote($$_PRO_FILE_PWD_) --qmake-run init $$QPMX_EXTRA_OPTIONS $$shell_quote($$QMAKE_QMAKE) $$shell_quote($$OUT_PWD)):include($$OUT_PWD/qpmx_generated.pri)\n"
		   << "else: error(" << tr("qpmx initialization failed. Check the compilation log for details.") << ")\n";
	stream.flush();
	file.close();
	xDebug() << tr("Successfully added qpmx init code to pro file \"%1\"").arg(proFile);
}

void InitCommand::exec(const QString &step, const QStringList &arguments)
{
	auto res = QProcess::execute(QCoreApplication::applicationFilePath(), arguments);
	switch (res) {
	case -2://not started
		throw tr("Failed to start qpmx to perform the %1 step").arg(step);
		break;
	case -1://crashed
		throw tr("Failed to run %1 step - qpmx crashed").arg(step);
		break;
	case 0://success
		xDebug() << tr("Successfully ran %1 step").arg(step);
		break;
	default:
		throw tr("Running %1 step failed").arg(step);
		break;
	}
}
