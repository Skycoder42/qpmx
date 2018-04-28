#include "qpmxformat.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonSerializer>
#include <QSaveFile>
using namespace qpmx;

QpmxDependency::QpmxDependency() :
	provider(),
	package(),
	version()
{}

QpmxDependency::QpmxDependency(const PackageInfo &package) :
	provider(package.provider()),
	package(package.package()),
	version(package.version())
{}

QpmxDependency::~QpmxDependency() = default;

bool QpmxDependency::operator==(const QpmxDependency &other) const
{
	//only provider and package "identify" the dependency
	return provider == other.provider &&
			package == other.package;
}

QString QpmxDependency::toString(bool scoped) const
{
	return pkg().toString(scoped);
}

bool QpmxDependency::isComplete() const
{
	return !provider.isEmpty() &&
			!package.isEmpty() &&
			!version.isNull();
}

PackageInfo QpmxDependency::pkg(const QString &provider) const
{
	return {provider.isEmpty() ? this->provider : provider, package, version};
}

QpmxFormat::QpmxFormat() :
	priFile(),
	prcFile(),
	source(false),
	dependencies()
{}

QpmxFormat::~QpmxFormat() = default;

QpmxFormat QpmxFormat::readFile(const QDir &dir, bool mustExist)
{
	return readFile(dir, QStringLiteral("./qpmx.json"), mustExist);
}

QpmxFormat QpmxFormat::readFile(const QDir &dir, const QString &fileName, bool mustExist)
{
	QFile qpmxFile(dir.absoluteFilePath(fileName));
	if(qpmxFile.exists()) {
		if(!qpmxFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			throw tr("Failed to open %1 with error: %2")
					.arg(qpmxFile.fileName(), qpmxFile.errorString());
		}

		try {
			QJsonSerializer ser;
			auto format = ser.deserializeFrom<QpmxFormat>(&qpmxFile);
			format.checkDuplicates();
			return format;
		} catch(QJsonSerializerException &e) {
			qDebug() << e.what();
			throw tr("%1 contains invalid data").arg(qpmxFile.fileName());
		}
	} else if(mustExist)
		throw tr("%1 does not exist").arg(qpmxFile.fileName());
	else
		return {};
}

QpmxFormat QpmxFormat::readDefault(bool mustExist)
{
	return readFile(QDir::current(), mustExist);
}

void QpmxFormat::writeDefault(const QpmxFormat &data)
{
	QSaveFile qpmxFile(QStringLiteral("./qpmx.json"));
	if(!qpmxFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		throw tr("Failed to open %1 with error: %2")
				.arg(qpmxFile.fileName(), qpmxFile.errorString());
	}

	try {
		QJsonSerializer ser;
		ser.serializeTo(&qpmxFile, data, QJsonDocument::Indented);
	} catch(QJsonSerializerException &e) {
		qDebug() << e.what();
		throw tr("Failed to write %1").arg(qpmxFile.fileName());
	}

	if(!qpmxFile.commit()) {
		throw tr("Failed to save %1 with error: %2")
				.arg(qpmxFile.fileName(), qpmxFile.errorString());
	}
}

void QpmxFormat::putDependency(const QpmxDependency &dep)
{
	auto depIndex = dependencies.indexOf(dep);
	if(depIndex == -1) {
		qDebug().noquote() << tr("Added package %1 to qpmx.json").arg(dep.toString());
		dependencies.append(dep);
	} else {
		qWarning().noquote() << tr("Package %1 is already a dependency. Replacing with that version")
								.arg(dep.toString());
		dependencies[depIndex] = dep;
	}
}

void QpmxFormat::checkDuplicates()
{
	checkDuplicatesImpl(dependencies);
}

template<typename T>
void QpmxFormat::checkDuplicatesImpl(const QList<T> &data)
{
	static_assert(std::is_base_of<QpmxDependency, T>::value, "checkDuplicates is only available for QpmxDependency classes");
	for(auto i = 0; i < data.size(); i++) {
		if(data[i].provider.isEmpty())
			throw tr("Dependency does not have a provider: %1").arg(data[i].toString());
		if(data[i].package.isEmpty())
			throw tr("Dependency does not have a package: %1").arg(data[i].toString());
		if(data[i].version.isNull())
			throw tr("Dependency does not have a version: %1").arg(data[i].toString());

		if(i < data.size() - 1) {
			if(data.indexOf(data[i], i + 1) != -1)
				throw tr("Duplicated dependency found: %1").arg(data[i].toString());
		}
	}
}


QpmxDevDependency::QpmxDevDependency() :
	QpmxDependency(),
	path()
{}

QpmxDevDependency::QpmxDevDependency(const QpmxDependency &dep, QString localPath) :
	QpmxDependency(dep),
	path(std::move(localPath))
{}

bool QpmxDevDependency::isDev() const
{
	return !path.isNull();
}

bool QpmxDevDependency::operator==(const QpmxDependency &other) const
{
	return QpmxDependency::operator ==(other);
}



QpmxUserFormat::QpmxUserFormat() :
	QpmxFormat(),
	devDependencies()
{}

QpmxUserFormat::QpmxUserFormat(const QpmxUserFormat &userFormat, const QpmxFormat &format) :
	QpmxFormat(format),
	devDependencies(userFormat.devDependencies)
{
	for(const auto &dep : qAsConst(devDependencies))
		dependencies.removeOne(dep);
}

QList<QpmxDevDependency> QpmxUserFormat::allDeps() const
{
	auto res = devDependencies;
	res.reserve(res.size() + dependencies.size());
	for(const auto &dep : qAsConst(dependencies))
		res.append(dep);
	return res;
}

QpmxUserFormat QpmxUserFormat::readDefault(bool mustExist)
{
	auto baseFormat = QpmxFormat::readDefault(mustExist);
	auto userFormat = readFile(QDir::current(), QStringLiteral("qpmx.json.user"), false);
	return {userFormat, baseFormat};
}

QpmxUserFormat QpmxUserFormat::readFile(const QDir &dir, const QString &fileName, bool mustExist)
{
	QFile qpmxUserFile(dir.absoluteFilePath(fileName));
	if(qpmxUserFile.exists()) {
		if(!qpmxUserFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			throw tr("Failed to open %1 with error: %2")
					.arg(qpmxUserFile.fileName(), qpmxUserFile.errorString());
		}

		try {
			QJsonSerializer ser;
			auto format = ser.deserializeFrom<QpmxUserFormat>(&qpmxUserFile);
			format.checkDuplicates();
			return format;
		} catch(QJsonSerializerException &e) {
			qDebug() << e.what();
			throw tr("%1 contains invalid data").arg(qpmxUserFile.fileName());
		}
	} else if(mustExist)
		throw tr("%1 does not exist").arg(qpmxUserFile.fileName());
	else
		return {};
}

void QpmxUserFormat::writeUser(const QpmxUserFormat &data)
{
	QSaveFile qpmxUserFile(QDir::current().absoluteFilePath(QStringLiteral("qpmx.json.user")));
	if(!qpmxUserFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		throw tr("Failed to open %1 with error: %2")
				.arg(qpmxUserFile.fileName(), qpmxUserFile.errorString());
	}

	try {
		QJsonSerializer ser;
		auto json = ser.serialize(data);

		QJsonObject userReduced;
		for(auto i = staticMetaObject.propertyOffset(); i < staticMetaObject.propertyCount(); i++) {
			auto prop = staticMetaObject.property(i);
			auto name = QString::fromUtf8(prop.name());
			userReduced[name] = json[name];
		}

		qpmxUserFile.write(QJsonDocument(userReduced).toJson(QJsonDocument::Indented));
	} catch(QJsonSerializerException &e) {
		qDebug() << e.what();
		throw tr("Failed to write %1").arg(qpmxUserFile.fileName());
	}

	if(!qpmxUserFile.commit()) {
		throw tr("Failed to save %1 with error: %2")
				.arg(qpmxUserFile.fileName(), qpmxUserFile.errorString());
	}
}

void QpmxUserFormat::checkDuplicates()
{
	QpmxFormat::checkDuplicates();
	checkDuplicatesImpl(devDependencies);
}

void QpmxUserFormat::setDevmodeSafe(const QList<QpmxDevDependency> &data)
{
	if(data.isEmpty())
		return;
	else if(!devDependencies.isEmpty())
		qWarning().noquote() << tr("Both devDependencies and devmode are set! Ignoring devmode");
	else
		devDependencies = data;
}

QList<QpmxDevDependency> QpmxUserFormat::readDummy() const
{
	return {};
}



QpmxCacheFormat::QpmxCacheFormat() :
	QpmxUserFormat(),
	buildKit()
{}

QpmxCacheFormat::QpmxCacheFormat(const QpmxUserFormat &userFormat, QString kitId) :
	QpmxUserFormat(userFormat),
	buildKit(std::move(kitId))
{}

QpmxCacheFormat QpmxCacheFormat::readCached(const QDir &dir)
{
	QFile qpmxCacheFile(dir.absoluteFilePath(QStringLiteral(".qpmx.cache")));
	if(qpmxCacheFile.exists()) {
		if(!qpmxCacheFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			throw tr("Failed to open %1 with error: %2")
					.arg(qpmxCacheFile.fileName(), qpmxCacheFile.errorString());
		}

		try {
			QJsonSerializer ser;
			auto format = ser.deserializeFrom<QpmxCacheFormat>(&qpmxCacheFile);
			format.checkDuplicates();
			return format;
		} catch(QJsonSerializerException &e) {
			qDebug() << e.what();
			throw tr("%1 contains invalid data").arg(qpmxCacheFile.fileName());
		}
	} else
		return {};
}

bool QpmxCacheFormat::writeCached(const QDir &dir, const QpmxCacheFormat &data)
{
	QSaveFile qpmxCacheFile(dir.absoluteFilePath(QStringLiteral(".qpmx.cache")));
	if(!qpmxCacheFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qWarning().noquote() << tr("Failed to open %1 with error: %2")
								.arg(qpmxCacheFile.fileName(), qpmxCacheFile.errorString());
		return false;
	}

	try {
		QJsonSerializer ser;
		ser.serializeTo(&qpmxCacheFile, data, QJsonDocument::Compact);
	} catch(QJsonSerializerException &e) {
		qDebug() << e.what();
		qWarning().noquote() << tr("Failed to write %1").arg(qpmxCacheFile.fileName());
		return false;
	}

	if(!qpmxCacheFile.commit()) {
		qWarning().noquote() << tr("Failed to save %1 with error: %2")
								.arg(qpmxCacheFile.fileName(), qpmxCacheFile.errorString());
		return false;
	} else
		return true;
}



bool QpmxFormatLicense::operator!=(const QpmxFormatLicense &other) const
{
	return name != other.name ||
		file != other.file;
}
