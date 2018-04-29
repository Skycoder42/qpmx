#include "compilecommand.h"
#include "generatecommand.h"

#include <QStandardPaths>
using namespace qpmx;

GenerateCommand::GenerateCommand(QObject *parent) :
	Command(parent),
	_genFile(nullptr),
	_qmake()
{}

QString GenerateCommand::commandName() const
{
	return QStringLiteral("generate");
}

QString GenerateCommand::commandDescription() const
{
	return tr("Generate the qpmx_generated.pri, internally used to include compiled packages.");
}

QSharedPointer<QCliNode> GenerateCommand::createCliNode() const
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

		QDir tDir(parser.positionalArguments().value(0));
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
				auto cacheFormat = QpmxCacheFormat::readCached(tDir);
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
		if(mainFormat.hasDevOptions())
			setDevMode(true);
		createPriFile(mainFormat);
		if(!QpmxCacheFormat::writeCached(tDir, cachedFormat(mainFormat)))
			xWarning() << tr("Failed to cache qpmx.json file. This means generate will always recreate the qpmx_generated.pri");

		xDebug() << tr("Pri-File generation completed");
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}

Command::BuildId GenerateCommand::kitId(const QpmxUserFormat &format) const
{
	if(format.source)
		return QStringLiteral("src");
	else
		return QtKitInfo::findKitId(buildDir(), _qmake);
}

QpmxCacheFormat GenerateCommand::cachedFormat(const QpmxUserFormat &format) const
{
	return {format, kitId(format)};
}

bool GenerateCommand::hasChanged(const QpmxUserFormat &currentUser, const QpmxCacheFormat &cache)
{
	auto current = cachedFormat(currentUser);

	if(current.buildKit != cache.buildKit ||
	   current.source != cache.source ||
	   current.prcFile != cache.prcFile ||
	   current.priIncludes != cache.priIncludes ||
	   current.dependencies.size() != cache.dependencies.size())
		return true;

	auto cCache = cache.dependencies;
	for(const auto &dep : qAsConst(current.dependencies)) {
		auto cIdx = cCache.indexOf(dep);
		if(cIdx == -1)
			return true;
		if(dep.version != cCache.takeAt(cIdx).version)
			return true;
	}
	if(!cCache.isEmpty())
		return true;

	auto dCache = cache.devDependencies;
	for(const auto &dep : qAsConst(current.devDependencies)) {
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
	stream << "!qpmx_sub_pri {\n"
		   << "\tQPMX_TMP_TS = $$TRANSLATIONS\n"
		   << "\tTRANSLATIONS = \n"
		   << "\tQPMX_BIN = \"" << QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) << "\"\n";
	if(current.source)
		stream << "\tCONFIG += qpmx_src_build\n";
	else {
		stream << "\tQPMX_APP_LIBS = $$LIBS\n"
			   << "\tLIBS =\n"
			   << "\tgcc:!mac: LIBS += -Wl,--start-group\n";
	}
	stream << "}\n\n";

	//add possible includes
	stream << "#local qpmx pri includes\n";
	if(current.source) {//not supported
		if(!current.priIncludes.isEmpty()) {
			stream << "warning(pri includes for sources builds will NOT load startup hooks or resources. "
				   << "Make shure to link the referenced library with ALL defined symbols!)\n"
				   << "win32: warning(See https://docs.microsoft.com/de-de/cpp/build/reference/wholearchive-include-all-library-object-files)\n"
				   << "else: warning(See https://stackoverflow.com/a/14116513)\n";
			for(const auto &inc : current.priIncludes) {
				stream << "INCLUDEPATH += $$fromfile(" << inc << "/qpmx_generated.pri, INCLUDEPATH)\n"
					   << "QPMX_INCLUDE_GUARDS += $$fromfile(" << inc << "/qpmx_generated.pri, QPMX_INCLUDE_GUARDS)\n";
			}
		}
	} else {
		stream << "!qpmx_sub_pri {\n"
			   << "\tCONFIG += qpmx_sub_pri\n";
		for(const auto &inc : current.priIncludes)
			stream << "\tinclude(" << inc << "/qpmx_generated.pri)\n";
		stream << "\tCONFIG -= qpmx_sub_pri\n"
			   << "} else {\n";
		for(const auto &inc : current.priIncludes)
			stream << "\tinclude(" << inc << "/qpmx_generated.pri)\n";
		stream << "}\n";
	}

	//add dependencies
	stream << "\n#dependencies\n"
		   << "QPMX_TS_DIRS = \n"; //clean for only use local deps
	for(const auto &dep : current.allDeps()) {
		auto dir = buildDir(kitId(current), dep.pkg());
		stream << "include(" << dir.absoluteFilePath(QStringLiteral("include.pri")) << ")\n";
	}

	//top-level pri only
	stream << "\n!qpmx_sub_pri {\n";

	if(!current.devDependencies.isEmpty()) {
		//add "dummy" dev deps, for easier access
		stream << "\tCONFIG -= qpmx_never_ever_ever_true\n"
			   << "\tqpmx_never_ever_ever_true { #trick to make QtCreator show dev dependencies in the build tree\n";
		for(const auto &dep : current.devDependencies) {
			auto depDir = QDir::current();
			if(!depDir.cd(dep.path)) {
				xWarning() << tr("Unabled to find directory of dev dependency %1").arg(dep.toString());
				continue;
			}

			auto format = QpmxFormat::readFile(depDir);
			if(!format.priFile.isEmpty())
				stream << "\t\tinclude(" << depDir.absoluteFilePath(format.priFile) << ")\n";
		}
		stream << "\t}\n\n";
	}

	//add fixed part
	QFile tCmp(QStringLiteral(":/build/qpmx_generated_base.pri"));
	if(!tCmp.open(QIODevice::ReadOnly | QIODevice::Text))
		throw tr("Failed to load translation compiler cached code with error: %1").arg(tCmp.errorString());
	while(!tCmp.atEnd())
		stream << "\t" << tCmp.readLine();
	tCmp.close();

	//final
	if(!current.source) {
		stream << "\n\tgcc:!mac: LIBS += -Wl,--end-group\n"
			   << "\tqpmx_as_private_lib: LIBS_PRIVATE += $$LIBS\n"
			   << "\telse: QPMX_APP_LIBS += $$LIBS\n"
			   << "\tLIBS = $$QPMX_APP_LIBS\n";
	}
	stream << "}\n";
	stream.flush();
	_genFile->close();
}
