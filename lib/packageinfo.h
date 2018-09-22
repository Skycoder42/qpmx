#ifndef PACKAGEINFO_H
#define PACKAGEINFO_H

#include <QtCore/QVersionNumber>
#include <QtCore/QSharedData>
#include <QtCore/QRegularExpression>

#include "libqpmx_global.h"

namespace qpmx {

class PackageInfoData;
class LIBQPMX_EXPORT PackageInfo
{
	Q_GADGET

	Q_PROPERTY(QString provider READ provider WRITE setProvider)
	Q_PROPERTY(QString package READ package WRITE setPackage)
	Q_PROPERTY(QVersionNumber version READ version WRITE setVersion)

public:
	static QRegularExpression packageRegexp();

	PackageInfo(QString provider = {},
				const QString &package = {},
				QVersionNumber version = {});
	PackageInfo(const PackageInfo &other);
	PackageInfo(PackageInfo &&other) noexcept;
	PackageInfo &operator=(const PackageInfo &other);
	PackageInfo &operator=(PackageInfo &&other) noexcept;
	~PackageInfo();

	QString provider() const;
	QString package() const;
	QVersionNumber version() const;

	bool isComplete() const;
	QString toString(bool scoped = true) const;

	bool operator==(const PackageInfo &other) const;
	bool operator!=(const PackageInfo &other) const;

	void setProvider(QString provider);
	void setPackage(const QString &package);
	void setVersion(QVersionNumber version);

private:
	QSharedDataPointer<PackageInfoData> d;
};

LIBQPMX_EXPORT uint qHash(const PackageInfo &t, uint seed);

}

Q_DECLARE_METATYPE(qpmx::PackageInfo)

#endif // PACKAGEINFO_H
