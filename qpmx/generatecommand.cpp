#include "generatecommand.h"

#include <QStandardPaths>
using namespace qpmx;

GenerateCommand::GenerateCommand(QObject *parent) :
	Command(parent),
	_genFile(nullptr),
	_qmake()
{}

QString GenerateCommand::commandName()
{
	return QStringLiteral("generate");
}

QString GenerateCommand::commandDescription()
{
	return tr("Generate the qpmx_generated.pri, internally used to include compiled packages.");
}

QSharedPointer<QCliNode> GenerateCommand::createCliNode()
{
	auto generateNode = QSharedPointer<QCliLeaf>::create();
	generateNode->addPositionalArgument(QStringLiteral("outdir"),
										tr("The directory to generate the file in."));
	generateNode->addOption({
								{QStringLiteral("m"), QStringLiteral("qmake")},
								tr("The <qmake> version to include compiled binaries for. If not specified "
																	  "the qmake from path is be used."),
								tr("qmake"),
								QStandardPaths::findExecutable(QStringLiteral("qmake"))
							});
	generateNode->addOption({
								{QStringLiteral("r"), QStringLiteral("recreate")},
								tr("Always delete and recreate the file if it exists, not only when the configuration changed."),
						   });
	return generateNode;
}

void GenerateCommand::initialize(QCliParser &parser)
{
	try {
		if(parser.positionalArguments().size() != 1)
			throw tr("Invalid arguments! You must specify the target directory as a single parameter");

		QDir tDir(parser.positionalArguments().first());
		if(!tDir.mkpath(QStringLiteral(".")))
			throw tr("Failed to create target directory");
		_genFile = new QFile(tDir.absoluteFilePath(QStringLiteral("qpmx_generated.pri")), this);
		auto cachePath = tDir.absoluteFilePath(QStringLiteral(".qpmx.cache"));

		//qmake kit
		_qmake = parser.value(QStringLiteral("qmake"));
		if(!QFile::exists(_qmake))
			throw tr("Choosen qmake executable \"%1\" does not exist").arg(_qmake);

		auto mainFormat = QpmxUserFormat::readDefault(true);
		if(_genFile->exists()) {
			if(!parser.isSet(QStringLiteral("recreate"))) {
				auto cacheFormat = QpmxUserFormat::readCached(tDir, false);
				if(!hasChanged(mainFormat, cacheFormat)) {
					xDebug() << tr("Unchanged configuration. Skipping generation");
					qApp->quit();
					return;
				}
			}

			if(!_genFile->remove())
				throw tr("Failed to remove qpmx_generated.pri with error: %1").arg(_genFile->errorString());
			if(QFile::exists(cachePath) && !QFile::remove(cachePath))
				throw tr("Failed to remove qpmx cache file");
		}

		//create the file
		xInfo() << tr("Updating qpmx_generated.pri to apply changes");
		createPriFile(mainFormat);
		if(!QpmxUserFormat::writeCached(tDir, mainFormat))
			xWarning() << tr("Failed to cache qpmx.json file. This means generate will always recreate the qpmx_generated.pri");

		xDebug() << tr("Pri-File generation completed");
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}

bool GenerateCommand::hasChanged(const QpmxUserFormat &current, const QpmxUserFormat &cache)
{
	if(current.source != cache.source ||
	   current.prcFile != cache.prcFile ||
	   current.qmakeExtraFlags != cache.qmakeExtraFlags ||
	   current.localIncludes != cache.localIncludes ||
	   current.dependencies.size() != cache.dependencies.size())
		return true;

	auto cCache = cache.dependencies;
	foreach(auto dep, current.dependencies) {
		auto cIdx = cCache.indexOf(dep);
		if(cIdx == -1)
			return true;
		if(dep.version != cCache.takeAt(cIdx).version)
			return true;
	}
	if(!cCache.isEmpty())
		return true;

	auto dCache = cache.devmode;
	foreach(auto dep, current.devmode) {
		auto cIdx = dCache.indexOf(dep);
		if(cIdx == -1)
			return true;
		auto dDep = dCache.takeAt(cIdx);
		if(dep.version != dDep.version ||
		   dep.path != dDep.path)
			return true;
	}
	if(!dCache.isEmpty())
		return true;

	return false;
}

void GenerateCommand::createPriFile(const QpmxUserFormat &current)
{
	if(!_genFile->open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to open qpmx_generated.pri with error: %1").arg(_genFile->errorString());

	//create & prepare
	QTextStream stream(_genFile);
	stream << "gcc:!mac:!gcc_skip_group: LIBS += -Wl,--start-group\n";
	if(current.source)
		stream << "CONFIG += qpmx_src_build\n";

	//add possible includes
	stream << "\n#local includes\n";
	if(current.source) {//only add include paths
		foreach(auto inc, current.localIncludes)
			stream << "INCLUDEPATH += $$fromfile(" << inc << "/qpmx_generated.pri, INCLUDEPATH)";
	} else {
		stream << "gcc:!mac:!gcc_skip_group {\n"
			   << "\tCONFIG += gcc_skip_group\n";
		foreach(auto inc, current.localIncludes)
			stream << "\tinclude(" << inc << "/qpmx_generated.pri)\n";
		stream << "\tCONFIG -= gcc_skip_group\n"
			   << "} else {\n";
		foreach(auto inc, current.localIncludes)
			stream << "\tinclude(" << inc << "/qpmx_generated.pri)\n";
		stream << "}\n";
	}

	//add dependencies
	BuildId kit;
	if(current.source)
		kit = QStringLiteral("src");
	else
		kit = findKit(_qmake);
	stream << "\n#dependencies\n";
	foreach(auto dep, current.dependencies) {
		auto dir = buildDir(kit, dep.pkg(), false);
		stream << "include(" << dir.absoluteFilePath(QStringLiteral("include.pri")) << ")\n";
	}

	//add dev dependencies
	stream << "\n#dev dependencies\n";
	auto crDir = QDir::current();
	foreach(auto dep, current.devmode)
		stream << "include(" << QDir::cleanPath(crDir.absoluteFilePath(dep.path)) << ")\n";

	//add translations
	stream << "\n#translations\n";
	QFile tCmp(QStringLiteral(":/build/translation_compiler.pri"));
	if(!tCmp.open(QIODevice::ReadOnly | QIODevice::Text))
		throw tr("Failed to load translation compiler cached code with error: %1").arg(tCmp.errorString());
	while(!tCmp.atEnd())
		stream << tCmp.readLine();
	tCmp.close();

	//final
	stream << "\ngcc:!mac:!gcc_skip_group: LIBS += -Wl,--end-group\n";
	stream.flush();
	_genFile->close();
}
