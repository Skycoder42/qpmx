#include "initcommand.h"

#include <QProcess>
using namespace qpmx;

InitCommand::InitCommand(QObject *parent) :
	Command(parent)
{}

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
						   outdir
					   });
		exec(QStringLiteral("generate"), runArgs);

		xDebug() << tr("Completed qpmx initialization");
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}

QString InitCommand::dashed(QString option)
{
	if(option.size() == 1)
		return QLatin1Char('-') + option;
	else
		return QStringLiteral("--") + option;
}

void InitCommand::prepare(const QString &proFile)
{
	QFile file(proFile);
	if(!file.exists())
		throw tr("Target file \"%1\" does not exist").arg(proFile);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		throw tr("Failed to open pro file \"%1\" with error: %2").arg(proFile).arg(file.errorString());

	QTextStream stream(&file);
	stream << "\nsystem(qpmx -d $$shell_quote($$_PRO_FILE_PWD_) init $$QPMX_EXTRA_OPTIONS $$shell_quote($$QMAKE_QMAKE) $$shell_quote($$OUT_PWD)):include($$OUT_PWD/qpmx_generated.pri)\n"
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
