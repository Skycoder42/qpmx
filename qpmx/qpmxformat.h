#ifndef QPMXFORMAT_H
#define QPMXFORMAT_H

#include <QObject>
#include <QVersionNumber>
#include <QCoreApplication>
#include <QJsonTypeConverter>
#include <QDir>
#include <QMap>
#include <QJsonObject>
#include <QUuid>

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
	bool operator!=(const QpmxDependency &other) const;

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
	Q_PROPERTY(QString qbsFile MEMBER qbsFile)
	Q_PROPERTY(bool source MEMBER source)

	Q_PROPERTY(QList<QpmxDependency> dependencies MEMBER dependencies)
	Q_PROPERTY(QStringList priIncludes MEMBER priIncludes)

	Q_PROPERTY(QpmxFormatLicense license MEMBER license)
	Q_PROPERTY(QMap<QString, QJsonObject> publishers MEMBER publishers);

public:
	virtual ~QpmxFormat();

	static QpmxFormat readFile(const QDir &dir, bool mustExist = false);
	static QpmxFormat readFile(const QDir &dir, const QString &fileName, bool mustExist = false);
	static QpmxFormat readDefault(bool mustExist = false);
	static void writeDefault(const QpmxFormat &data);

	QString priFile;
	QString prcFile;
	QString qbsFile;
	bool source = false;
	QList<QpmxDependency> dependencies;
	QStringList priIncludes;
	QpmxFormatLicense license;
	QMap<QString, QJsonObject> publishers;

	void putDependency(const QpmxDependency &dep);

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
	QpmxDevDependency(const QpmxDependency &dep, QString localPath = {});

	bool isDev() const;

	bool operator==(const QpmxDependency &other) const;

	QString path;
};

class QpmxDevAlias
{
	Q_GADGET

	Q_PROPERTY(QpmxDependency original MEMBER original)
	Q_PROPERTY(QpmxDependency alias MEMBER alias)

public:
	QpmxDevAlias();
	QpmxDevAlias(const QpmxDependency &original, const QpmxDependency &alias = {});

	bool operator==(const QpmxDependency &original) const;
	bool operator==(const QpmxDevAlias &other) const;

	QpmxDependency original;
	QpmxDependency alias;
};

class QpmxUserFormat : public QpmxFormat
{
	Q_GADGET
	Q_DECLARE_TR_FUNCTIONS(QpmxUserFormat)

	Q_PROPERTY(QList<QpmxDevDependency> devDependencies MEMBER devDependencies)
	Q_PROPERTY(QList<QpmxDevAlias> devAliases MEMBER devAliases)

	//MAJOR keep for compability. Will not be written, but can be read
	Q_PROPERTY(QList<QpmxDevDependency> devmode WRITE setDevmodeSafe READ readDummy)

public:
	QpmxUserFormat();
	QpmxUserFormat(const QpmxUserFormat &userFormat, const QpmxFormat &format);

	QList<QpmxDevDependency> allDeps() const;
	bool hasDevOptions() const;

	static QpmxUserFormat readDefault(bool mustExist = false);
	static QpmxUserFormat readFile(const QDir &dir, const QString &fileName, bool mustExist = false);

	static void writeUser(const QpmxUserFormat &data);

	QList<QpmxDevDependency> devDependencies;
	QList<QpmxDevAlias> devAliases;

protected:
	void checkDuplicates() override;

private:
	void setDevmodeSafe(const QList<QpmxDevDependency> &data);
	QList<QpmxDevDependency> readDummy() const;
};

class QpmxCacheFormat : public QpmxUserFormat
{
	Q_GADGET
	Q_DECLARE_TR_FUNCTIONS(QpmxCacheFormat)

	Q_PROPERTY(QString buildKit MEMBER buildKit)

public:
	QpmxCacheFormat();
	QpmxCacheFormat(const QpmxUserFormat &userFormat, QString kitId);

	static QpmxCacheFormat readCached(const QDir &dir);
	static bool writeCached(const QDir &dir, const QpmxCacheFormat &data);

	QString buildKit;
};

Q_DECLARE_METATYPE(QpmxDependency)
Q_DECLARE_METATYPE(QpmxFormatLicense)
Q_DECLARE_METATYPE(QpmxFormat)
Q_DECLARE_METATYPE(QpmxDevDependency)
Q_DECLARE_METATYPE(QpmxDevAlias)
Q_DECLARE_METATYPE(QpmxUserFormat)
Q_DECLARE_METATYPE(QpmxCacheFormat)

#endif // QPMXFORMAT_H
