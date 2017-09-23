#ifndef QPMXFORMAT_H
#define QPMXFORMAT_H

#include <QObject>
#include <QVersionNumber>
#include <QCoreApplication>
#include <QJsonTypeConverter>
#include <QDir>
#include <QMap>
#include <QJsonObject>

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

	Q_PROPERTY(QMap<QString, QJsonObject> publishers MEMBER publishers)

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
	QMap<QString, QJsonObject> publishers;

protected:
	virtual void checkDuplicates();
	template <typename T>
	static void checkDuplicatesImpl(const QList<T> &data);
};

class QpmxDevDependency : public QpmxDependency
{
	Q_GADGET

	Q_PROPERTY(QString path MEMBER path)

public:
	QpmxDevDependency();
	QpmxDevDependency(const QpmxDependency &dep, const QString &localPath = {});

	bool operator==(const QpmxDependency &other) const;

	QString path;
};

class QpmxUserFormat : public QpmxFormat
{
	Q_GADGET
	Q_DECLARE_TR_FUNCTIONS(QpmxFormat)

	Q_PROPERTY(QList<QpmxDevDependency> devmode MEMBER devmode)

public:
	QpmxUserFormat();
	QpmxUserFormat(const QpmxUserFormat &userFormat, const QpmxFormat &format);

	static QpmxUserFormat readDefault(bool mustExist = false);
	static QpmxUserFormat readCached(const QDir &dir, bool mustExist = false);
	static QpmxUserFormat readFile(const QDir &dir, const QString &fileName, bool mustExist = false);

	static void writeUser(const QpmxUserFormat &data);
	static bool writeCached(const QDir &dir, const QpmxUserFormat &data);

	QList<QpmxDevDependency> devmode;

protected:
	void checkDuplicates() override;
};

Q_DECLARE_METATYPE(QpmxDependency)
Q_DECLARE_METATYPE(QpmxFormat)
Q_DECLARE_METATYPE(QpmxDevDependency)
Q_DECLARE_METATYPE(QpmxUserFormat)

#endif // QPMXFORMAT_H
