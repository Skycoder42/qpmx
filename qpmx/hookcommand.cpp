#include "hookcommand.h"
using namespace qpmx;

HookCommand::HookCommand(QObject *parent) :
	Command(parent)
{}

QString HookCommand::commandName()
{
	return QStringLiteral("hook");
}

QString HookCommand::commandDescription()
{
	return tr("Creates a source file for the given hook id");
}

QSharedPointer<QCliNode> HookCommand::createCliNode()
{
	auto hookNode = QSharedPointer<QCliLeaf>::create();
	hookNode->setHidden(true);
	hookNode->addOption({
							{QStringLiteral("o"), QStringLiteral("out")},
							tr("The <path> of the file to be generated (required!)."),
							tr("path")
						});
	hookNode->addOption({
							QStringLiteral("path"),
							tr("Expect the resources to be paths instead of basenames.")
						});
	hookNode->addPositionalArgument(QStringLiteral("hook_ids"),
									tr("The ids of the hooks to be added to the hookup. Typically defined by the "
									   "QPMX_STARTUP_HASHES qmake variable."),
									QStringLiteral("[<hook_id> ...]"));
	return hookNode;
}

void HookCommand::initialize(QCliParser &parser)
{
	try {
		auto outFile = parser.value(QStringLiteral("out"));
		if(outFile.isEmpty())
			throw tr("You must specify the name of the file to generate as --out option");

		QFile out(outFile);
		if(!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
			throw tr("Failed to create %1 file with error: %2")
					.arg(out.fileName())
					.arg(out.errorString());
		}

		bool sep = false;
		QRegularExpression replaceRegex(QStringLiteral(R"__([\.-])__"));
		QStringList hooks;
		QStringList resources;
		foreach(auto arg, parser.positionalArguments()) {
			if(sep) {
				QString res;
				if(parser.isSet(QStringLiteral("path")))
					res = QFileInfo(arg).completeBaseName();
				else
					res = arg;
				resources.append(res.replace(replaceRegex, QStringLiteral("_")));
			} else if(arg == QStringLiteral("%%"))
				sep = true;
			else
				hooks.append(arg);
		}

		xDebug() << tr("Creating hook file");
		QTextStream stream(&out);
		stream << "#include <QtCore/QCoreApplication>\n\n";
		foreach(auto hook, hooks)
			stream << "void " << hook << "();\n";
		stream << "\nstatic void __qpmx_root_hook() {\n";
		foreach(auto resource, resources)
			stream << "\tQ_INIT_RESOURCE(" << resource << ");\n";
		foreach(auto hook, hooks)
			stream << "\t" << hook << "();\n";
		stream << "}\n"
			   << "Q_COREAPP_STARTUP_FUNCTION(__qpmx_root_hook)\n";
		stream.flush();
		out.close();

		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}
