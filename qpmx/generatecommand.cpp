#include "generatecommand.h"

GenerateCommand::GenerateCommand(QObject *parent) :
	Command(parent),
	_genFile(nullptr)
{}

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

		auto mainFormat = QpmxFormat::readDefault(true);
		if(_genFile->exists()) {
			if(!parser.isSet(QStringLiteral("recreate"))) {
				auto cacheFormat = QpmxFormat::readFile(tDir, QStringLiteral(".qpmx.cache"));
				if(!hasChanged(mainFormat, cacheFormat)) {
					xDebug() << tr("Unchanged configuration. Skipping generationg");
					qApp->quit();
					return;
				}
			}

			if(_genFile->remove())
				throw tr("Failed to remove qpmx_generated.pri with error: %1").arg(_genFile->errorString());
			if(!QFile::remove(cachePath))
				throw tr("Failed to remove qpmx cache file");
		}

		//create the file
		xInfo() << tr("Updating qpmx_generated.pri to apply changes");
		createPriFile(mainFormat);
		if(!QFile::copy(QStringLiteral("qpmx.json"), cachePath))
			xWarning() << tr("Failed to cache qpmx.json file. This means generate will always recreate the qpmx_generated.pri");

		xDebug() << tr("Pri-File generation completed");
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}

bool GenerateCommand::hasChanged(const QpmxFormat &current, const QpmxFormat &cache)
{
	if(current.source != cache.source)
		return true;

	if(current.dependencies.size() != cache.dependencies.size())
		return true;
	auto cCache = cache.dependencies;
	foreach(auto dep, current.dependencies) {
		auto cIdx = cCache.indexOf(dep);
		if(cIdx == -1)
			return true;
		if(dep.version != cCache.takeAt(cIdx).version)
			return true;
	}

	return !cCache.isEmpty();
}

void GenerateCommand::createPriFile(const QpmxFormat &current)
{

}
