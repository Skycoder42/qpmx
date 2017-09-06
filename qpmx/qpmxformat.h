#ifndef QPMXFORMAT_H
#define QPMXFORMAT_H

#include <QObject>
#include <QVersionNumber>
#include <QCoreApplication>
#include <QJsonTypeConverter>

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
	Q_PROPERTY(bool source MEMBER source)

public:
	QpmxDependency();
	QpmxDependency(const qpmx::PackageInfo &package, bool source);

	bool operator==(const QpmxDependency &other) const;
	operator QString() const;
	qpmx::PackageInfo pkg(const QString &provider = {}) const;

	QString provider;
	QString package;
	QVersionNumber version;
	bool source;
};

class QpmxFormat
{
	Q_GADGET
	Q_DECLARE_TR_FUNCTIONS(QpmxFormat)

	Q_PROPERTY(QList<QpmxDependency> dependencies MEMBER dependencies)

public:
	QpmxFormat();

	static QpmxFormat readDefault(bool mustExist = false);
	static void writeDefault(const QpmxFormat &data);

	QList<QpmxDependency> dependencies;
};

Q_DECLARE_METATYPE(QpmxDependency)
Q_DECLARE_METATYPE(QpmxFormat)

#endif // QPMXFORMAT_H
