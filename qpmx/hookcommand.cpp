#include "hookcommand.h"

#include <QCryptographicHash>
using namespace qpmx;

HookCommand::HookCommand(QObject *parent) :
	Command(parent)
{}

QString HookCommand::commandName() const
{
	return QStringLiteral("hook");
}

QString HookCommand::commandDescription() const
{
	return tr("Creates a source file for the given hook id");
}

QSharedPointer<QCliNode> HookCommand::createCliNode() const
{
	auto hookNode = QSharedPointer<QCliLeaf>::create();
	hookNode->setHidden(true);
	hookNode->addOption({
							QStringLiteral("prepare"),
							tr("Generate the special sources for given <source> as outfile."),
							tr("source")
						});
	hookNode->addOption({
							{QStringLiteral("o"), QStringLiteral("out")},
							tr("The <path> of the file to be generated (required!)."),
							tr("path")
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
					.arg(out.fileName(), out.errorString());
		}

		if(parser.isSet(QStringLiteral("prepare")))
			createHookCompile(parser.value(QStringLiteral("prepare")), &out);
		else
			createHookSrc(parser.positionalArguments(), &out);

		out.close();
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}

void HookCommand::createHookSrc(const QStringList &args, QIODevice *out)
{
	bool sep = false;
	QRegularExpression replaceRegex(QStringLiteral(R"__([\.-])__"));
	QStringList hooks;
	QStringList resources;
	for(auto arg : args) {
		if(sep)
			resources.append(arg.replace(replaceRegex, QStringLiteral("_")));
		else if(arg == QStringLiteral("%%"))
			sep = true;
		else
			hooks.append(arg);
	}

	xDebug() << tr("Creating hook file");
	QTextStream stream(out);
	stream << "#include <QtCore/QCoreApplication>\n\n"
		   << "namespace __qpmx_startup_hooks {\n";
	for(const auto &hook : hooks)
		stream << "\tvoid hook_" << hook << "();\n";
	stream << "}\n\n";
	stream << "using namespace __qpmx_startup_hooks;\n"
		   << "static void __qpmx_root_hook() {\n";
	for(const auto &resource : resources)
		stream << "\tQ_INIT_RESOURCE(" << resource << ");\n";
	for(const auto &hook : hooks)
		stream << "\thook_" << hook << "();\n";
	stream << "}\n"
		   << "Q_CONSTRUCTOR_FUNCTION(__qpmx_root_hook)\n";
	stream.flush();
}

void HookCommand::createHookCompile(const QString &inFile, QIODevice *out)
{
	xDebug() << tr("Scanning %1 for startup hooks").arg(inFile);

	QStringList functions;
	QFile file(inFile);
	if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		throw tr("Failed to read source file %1 with error: %2")
				.arg(file.fileName(), file.errorString());
	}
	auto inData = QTextStream(&file).readAll();
	QRegularExpression fileRegex(QStringLiteral(R"__(^Q_COREAPP_STARTUP_FUNCTION\(([^\)]+)\)$)__"),
								 QRegularExpression::MultilineOption);
	auto iter = fileRegex.globalMatch(inData);
	while(iter.hasNext()) {
		auto match = iter.next();
		functions.append(match.captured(1));
	}
	xDebug() << tr("found startup hooks: %1").arg(functions.join(QStringLiteral(", ")));

	xDebug() << tr("Creating hook include");
	QTextStream stream(out);
	stream << "#define Q_CONSTRUCTOR_FUNCTION(x)\n" //define to nothing to prevent code generation
		   << "#include \"" << inFile << "\"\n";

	QDir hookDir(QStringLiteral(".qpmx_startup_hooks"));
	if(!hookDir.mkpath(QStringLiteral(".")))
		throw tr("Failed to create hook directory");
	QFile hookFile(hookDir.absoluteFilePath(QFileInfo(inFile).fileName()));
	if(!functions.isEmpty()) {
		if(!hookFile.open(QIODevice::WriteOnly | QIODevice::Text))
			throw tr("Failed to create qpmx hook cache with error: %1").arg(hookFile.errorString());
		QTextStream hookStream(&hookFile);

		stream << "\nnamespace __qpmx_startup_hooks {";
		for(const auto &fn : functions) {
			auto fnId = QCryptographicHash::hash(fn.toUtf8() + QByteArray::number(qrand()), QCryptographicHash::Sha3_256)
						.toHex();
			stream << "\n\tvoid hook_" << fnId << "() {\n"
				   << "\t\t" << fn << "_ctor_function();\n"
				   << "\t}\n";
			hookStream << fnId << "\n";
		}
		stream << "}\n";

		hookStream.flush();
		hookFile.close();
	} else
		hookFile.remove();

	stream.flush();
}
