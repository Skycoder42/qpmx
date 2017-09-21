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
			return ser.deserializeFrom<QpmxFormat>(&qpmxFile);
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

QpmxDevData::QpmxDevData() :
	active(false),
	devDeps()
{}

bool QpmxDevData::operator!=(const QpmxDevData &other) const
{
	return active != other.active &&
		devDeps != other.devDeps;
}

QpmxUserFormat::QpmxUserFormat() :
	QpmxFormat(),
	dev()
{}

QpmxUserFormat::QpmxUserFormat(const QpmxUserFormat &userFormat, const QpmxFormat &format) :
	QpmxFormat(format),
	dev(userFormat.dev)
{
	if(dev.active)
		source = true;
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
			return ser.deserializeFrom<QpmxUserFormat>(&qpmxUserFile);
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
		//ser.serializeTo(&qpmxFile, data);
		auto json = ser.serialize(data);
		qpmxUserFile.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
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
