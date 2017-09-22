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
					.arg(fileName)
					.arg(qpmxFile.errorString());
		}

		try {
			QJsonSerializer ser;
			ser.addJsonTypeConverter(new VersionConverter());
			auto format = ser.deserializeFrom<QpmxFormat>(&qpmxFile);
			format.checkDuplicates();
			return format;
		} catch(QJsonSerializerException &e) {
			qDebug() << e.what();
			throw tr("%1 contains invalid data").arg(fileName);
		}
	} else if(mustExist)
		throw tr("%1 file does not exist").arg(fileName);
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
	if(!qpmxFile.open(QIODevice::WriteOnly | QIODevice::Text))
		throw tr("Failed to open qpmx.json with error: %1").arg(qpmxFile.errorString());

	try {
		QJsonSerializer ser;
		ser.addJsonTypeConverter(new VersionConverter());
		//ser.serializeTo(&qpmxFile, data);
		auto json = ser.serialize(data);
		qpmxFile.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
	} catch(QJsonSerializerException &e) {
		qDebug() << e.what();
		throw tr("Failed to write qpmx.json");
	}

	if(!qpmxFile.commit())
		throw tr("Failed to save qpmx.json with error: %1").arg(qpmxFile.errorString());
}

void QpmxFormat::checkDuplicates()
{
	checkDuplicatesImpl(dependencies);
}

template<typename T>
void QpmxFormat::checkDuplicatesImpl(const QList<T> &data)
{
	static_assert(std::is_base_of<QpmxDependency, T>::value, "checkDuplicates is only available for QpmxDependency classes");
	for(auto i = 0; i < data.size() - 1; i++) {
		if(data.indexOf(data[i], i + 1) != -1)
			throw tr("Duplicated dependency found: %1").arg(data[i].toString());
	}
}


QpmxDevDependency::QpmxDevDependency() :
	QpmxDependency(),
	path()
{}

QpmxDevDependency::QpmxDevDependency(const QpmxDependency &dep, const QString &localPath) :
	QpmxDependency(dep),
	path(localPath)
{}

bool QpmxDevDependency::operator==(const QpmxDependency &other) const
{
	return QpmxDependency::operator ==(other);
}


QpmxUserFormat::QpmxUserFormat() :
	QpmxFormat(),
	devmode()
{}

QpmxUserFormat::QpmxUserFormat(const QpmxUserFormat &userFormat, const QpmxFormat &format) :
	QpmxFormat(format),
	devmode(userFormat.devmode)
{
	if(!devmode.isEmpty())
		source = true;
	foreach(auto dep, devmode)
		dependencies.removeOne(dep);
}

QpmxUserFormat QpmxUserFormat::readDefault(bool mustExist)
{
	auto baseFormat = QpmxFormat::readDefault(mustExist);
	auto userFormat = readFile(QDir::current(), QStringLiteral("qpmx.json.user"), false);
	return {userFormat, baseFormat};
}

QpmxUserFormat QpmxUserFormat::readCached(const QDir &dir, bool mustExist)
{
	return readFile(dir, QStringLiteral(".qpmx.cache"), mustExist);
}

QpmxUserFormat QpmxUserFormat::readFile(const QDir &dir, const QString &fileName, bool mustExist)
{
	QFile qpmxUserFile(dir.absoluteFilePath(fileName));
	if(qpmxUserFile.exists()) {
		if(!qpmxUserFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			throw tr("Failed to open %1 with error: %2")
					.arg(fileName)
					.arg(qpmxUserFile.errorString());
		}

		try {
			QJsonSerializer ser;
			ser.addJsonTypeConverter(new VersionConverter());
			auto format = ser.deserializeFrom<QpmxUserFormat>(&qpmxUserFile);
			format.checkDuplicates();
			return format;
		} catch(QJsonSerializerException &e) {
			qDebug() << e.what();
			throw tr("%1 contains invalid data").arg(fileName);
		}
	} else if(mustExist)
		throw tr("%1 file does not exist").arg(fileName);
	else
		return {};
}

bool QpmxUserFormat::writeCached(const QDir &dir, const QpmxUserFormat &data)
{
	QSaveFile qpmxUserFile(dir.absoluteFilePath(QStringLiteral(".qpmx.cache")));
	if(!qpmxUserFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qWarning().noquote() << tr("Failed to open .qpmx.cache with error: %1").arg(qpmxUserFile.errorString());
		return false;
	}

	try {
		QJsonSerializer ser;
		ser.addJsonTypeConverter(new VersionConverter());
		ser.serializeTo(&qpmxUserFile, data);
	} catch(QJsonSerializerException &e) {
		qDebug() << e.what();
		qWarning().noquote() << tr("Failed to write .qpmx.cache");
		return false;
	}

	if(!qpmxUserFile.commit()) {
		qWarning().noquote() << tr("Failed to save .qpmx.cache with error: %1").arg(qpmxUserFile.errorString());
		return false;
	} else
		return true;
}

void QpmxUserFormat::checkDuplicates()
{
	QpmxFormat::checkDuplicates();
	checkDuplicatesImpl(devmode);
}




bool VersionConverter::canConvert(int metaTypeId) const
{
	return metaTypeId == qMetaTypeId<QVersionNumber>();
}

QList<QJsonValue::Type> VersionConverter::jsonTypes() const
{
	return {QJsonValue::String};
}

QJsonValue VersionConverter::serialize(int propertyType, const QVariant &value, const SerializationHelper *helper) const
{
	Q_ASSERT(propertyType == qMetaTypeId<QVersionNumber>());
	Q_UNUSED(helper)
	return value.value<QVersionNumber>().toString();
}

QVariant VersionConverter::deserialize(int propertyType, const QJsonValue &value, QObject *parent, const SerializationHelper *helper) const
{
	Q_ASSERT(propertyType == qMetaTypeId<QVersionNumber>());
	Q_UNUSED(helper)
	Q_UNUSED(parent)
	return QVariant::fromValue(QVersionNumber::fromString(value.toString()));
}
