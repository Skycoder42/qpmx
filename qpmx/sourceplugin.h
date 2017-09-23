#ifndef QPMX_SOURCEPLUGIN_H
#define QPMX_SOURCEPLUGIN_H

#include "packageinfo.h"

#include <QtCore/QObject>
#include <QtCore/QDir>
#include <QtCore/QVariantHash>
#include <QtCore/QJsonObject>

namespace qpmx { //qpmx public namespace

class SourcePlugin
{
public:
	virtual inline ~SourcePlugin() = default;

	virtual bool canSearch(const QString &provider) const = 0;
	virtual bool canPublish(const QString &provider) const = 0;
	virtual QString packageSyntax(const QString &provider) const = 0;
	virtual bool packageValid(const qpmx::PackageInfo &package) const = 0;

	virtual QJsonObject createPublisherInfo(const QString &provider) const = 0;

	virtual void cancelAll(int timeout) = 0;

public Q_SLOTS:
	virtual void searchPackage(int requestId, const QString &provider, const QString &query) = 0;
	virtual void findPackageVersion(int requestId, const qpmx::PackageInfo &package) = 0;
	virtual void getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir) = 0;
	virtual void publishPackage(int requestId, const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo) = 0;

Q_SIGNALS:
	virtual void searchResult(int requestId, const QStringList &packageNames) = 0;
	virtual void versionResult(int requestId, const QVersionNumber &version) = 0;
	virtual void sourceFetched(int requestId) = 0;
	virtual void packagePublished(int requestId) = 0;
	virtual void sourceError(int requestId, const QString &error) = 0;
};

}

Q_DECLARE_METATYPE(qpmx::PackageInfo)

#define SourcePlugin_iid "de.skycoder42.qpmx.SourcePlugin"
Q_DECLARE_INTERFACE(qpmx::SourcePlugin, SourcePlugin_iid)

#endif // QPMX_SOURCEPLUGIN_H
