#ifndef QPMXFORMAT_H
#define QPMXFORMAT_H

#include <QObject>
#include <QVersionNumber>
#include <QCoreApplication>
#include <QJsonTypeConverter>
#include <QDir>

#include "packageinfo.h"

class VersionConverter : public QJsonTypeConverter
{
public:
	bool canConvert(int metaTypeId) const override;
	QList<QJsonValue::Type> jsonTypes() const override;
	QJsonValue serialize(int propertyType, const QVariant &value, const SerializationHelper *helper) const override;
	QVariant deserialize(int propertyType, const QJsonValue &value, QObject *parent, const SerializationHelper *helper) const override;
};

class QpmxDependency
{
	Q_GADGET

	Q_PROPERTY(QString provider MEMBER provider)
	Q_PROPERTY(QString package MEMBER package)
	Q_PROPERTY(QVersionNumber version MEMBER version)

public:
	QpmxDependency();
	QpmxDependency(const qpmx::PackageInfo &package);

	bool operator==(const QpmxDependency &other) const;
	QString toString(bool scoped = true) const;
	qpmx::PackageInfo pkg(const QString &provider = {}) const;

	QString provider;
	QString package;
	QVersionNumber version;
};

class QpmxFormat
{
	Q_GADGET
	Q_DECLARE_TR_FUNCTIONS(QpmxFormat)

	Q_PROPERTY(QString priFile MEMBER priFile)
	Q_PROPERTY(QString prcFile MEMBER prcFile)
	Q_PROPERTY(bool source MEMBER source)

	Q_PROPERTY(QList<QpmxDependency> dependencies MEMBER dependencies)

	Q_PROPERTY(QStringList qmakeExtraFlags MEMBER qmakeExtraFlags)
	Q_PROPERTY(QStringList localIncludes MEMBER localIncludes)

public:
	QpmxFormat();

	static QpmxFormat readFile(const QDir &dir, bool mustExist = false);
	static QpmxFormat readFile(const QDir &dir, const QString &fileName, bool mustExist = false);
	static QpmxFormat readDefault(bool mustExist = false);
	static void writeDefault(const QpmxFormat &data);

	QString priFile;
	QString prcFile;
	bool source;
	QList<QpmxDependency> dependencies;
	QStringList qmakeExtraFlags;
	QStringList localIncludes;
};

class QpmxDevData
{
	Q_GADGET

	Q_PROPERTY(bool active MEMBER active)
	Q_PROPERTY(QList<QpmxDependency> devDeps MEMBER devDeps)

public:
	QpmxDevData();

	bool operator!=(const QpmxDevData &other) const;

	bool active;
	QList<QpmxDependency> devDeps;
};

class QpmxUserFormat : public QpmxFormat
{
	Q_GADGET

	Q_PROPERTY(QpmxDevData dev MEMBER dev)

public:
	QpmxUserFormat();
	QpmxUserFormat(const QpmxUserFormat &userFormat, const QpmxFormat &format);

	static QpmxUserFormat readDefault(bool mustExist = false);
	static QpmxUserFormat readCached(const QDir &dir, bool mustExist = false);
	static QpmxUserFormat readFile(const QDir &dir, const QString &fileName, bool mustExist = false);

	static bool writeCached(const QDir &dir, const QpmxUserFormat &data);

	QpmxDevData dev;
};

Q_DECLARE_METATYPE(QpmxDependency)
Q_DECLARE_METATYPE(QpmxFormat)
Q_DECLARE_METATYPE(QpmxDevData)
Q_DECLARE_METATYPE(QpmxUserFormat)

#endif // QPMXFORMAT_H
