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

class QpmxDependency
{
	Q_GADGET

	Q_PROPERTY(QString provider MEMBER provider)
	Q_PROPERTY(QString package MEMBER package)
	Q_PROPERTY(QVersionNumber version MEMBER version)

public:
	QpmxDependency();
	QpmxDependency(const qpmx::PackageInfo &package);
	virtual ~QpmxDependency();

	bool operator==(const QpmxDependency &other) const;

	QString toString(bool scoped = true) const;
	bool isComplete() const;
	qpmx::PackageInfo pkg(const QString &provider = {}) const;

	QString provider;
	QString package;
	QVersionNumber version;
};

class QpmxFormatLicense
{
	Q_GADGET

	Q_PROPERTY(QString name MEMBER name)
	Q_PROPERTY(QString file MEMBER file)

public:
	QString name;
	QString file;

	bool operator!=(const QpmxFormatLicense &other) const;

};

class QpmxFormat
{
	Q_GADGET
	Q_DECLARE_TR_FUNCTIONS(QpmxFormat)

	Q_PROPERTY(QString priFile MEMBER priFile)
	Q_PROPERTY(QString prcFile MEMBER prcFile)
	Q_PROPERTY(bool source MEMBER source)

	Q_PROPERTY(QList<QpmxDependency> dependencies MEMBER dependencies)
	Q_PROPERTY(QStringList priIncludes MEMBER priIncludes)

	Q_PROPERTY(QpmxFormatLicense license MEMBER license)
	Q_PROPERTY(QMap<QString, QJsonObject> publishers MEMBER publishers)

public:
	QpmxFormat();
	virtual ~QpmxFormat();

	static QpmxFormat readFile(const QDir &dir, bool mustExist = false);
	static QpmxFormat readFile(const QDir &dir, const QString &fileName, bool mustExist = false);
	static QpmxFormat readDefault(bool mustExist = false);
	static void writeDefault(const QpmxFormat &data);

	QString priFile;
	QString prcFile;
	bool source;
	QList<QpmxDependency> dependencies;
	QStringList priIncludes;
	QpmxFormatLicense license;
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

	bool isDev() const;

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

	QList<QpmxDevDependency> allDeps() const;

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
Q_DECLARE_METATYPE(QpmxFormatLicense)
Q_DECLARE_METATYPE(QpmxFormat)
Q_DECLARE_METATYPE(QpmxDevDependency)
Q_DECLARE_METATYPE(QpmxUserFormat)

#endif // QPMXFORMAT_H
