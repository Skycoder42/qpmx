#ifndef QPMX_SOURCEPLUGIN_H
#define QPMX_SOURCEPLUGIN_H

#include "packageinfo.h"

#include <QtCore/QObject>
#include <QtCore/QDir>
#include <QtCore/QVariantHash>

namespace qpmx { //qpmx public namespace

class SourcePlugin
{
public:
	virtual inline ~SourcePlugin() = default;

	virtual QString packageSyntax(const QString &provider) const = 0;
	virtual bool packageValid(const qpmx::PackageInfo &package) const = 0;

public:
	virtual void searchPackage(int requestId, const QString &provider, const QString &query) = 0;
	virtual void listPackageVersions(int requestId, const qpmx::PackageInfo &package) = 0;
	virtual void getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir, const QVariantHash &extraParameters = {}) = 0;

public:
	virtual void searchResult(int requestId, const QStringList &package) = 0;
	virtual void versionResult(int requestId, const QList<QVersionNumber> &versions) = 0;
	virtual void sourceFetched(int requestId) = 0;
	virtual void sourceError(int requestId, const QString &error) = 0;
};

}

Q_DECLARE_METATYPE(qpmx::PackageInfo)

#define SourcePlugin_iid "de.skycoder42.qpmx.SourcePlugin"
Q_DECLARE_INTERFACE(qpmx::SourcePlugin, SourcePlugin_iid)

#endif // QPMX_SOURCEPLUGIN_H
