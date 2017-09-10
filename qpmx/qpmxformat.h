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
	operator QString() const;//TODO use toString instead
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
};

Q_DECLARE_METATYPE(QpmxDependency)
Q_DECLARE_METATYPE(QpmxFormat)

#endif // QPMXFORMAT_H
