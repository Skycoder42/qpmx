#ifndef QPMXFORMAT_H
#define QPMXFORMAT_H

#include <QObject>
#include <QVersionNumber>
#include <QCoreApplication>

#include "packageinfo.h"

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

	static QpmxFormat readDefault();
	static void writeDefault(const QpmxFormat &data);

	QList<QpmxDependency> dependencies;
};

Q_DECLARE_METATYPE(QpmxDependency)
Q_DECLARE_METATYPE(QpmxFormat)

#endif // QPMXFORMAT_H
