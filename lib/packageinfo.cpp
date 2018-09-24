#include "packageinfo.h"
using namespace qpmx;

namespace qpmx {

class PackageInfoData : public QSharedData
{
public:
	QString provider;
	QString package;
	QVersionNumber version;

	PackageInfoData(QString provider, const QString &package, QVersionNumber version) :
		QSharedData(),
		provider(std::move(provider)),
		package(package.toLower()),
		version(std::move(version))
	{}

	PackageInfoData(const PackageInfoData &other) = default;
	PackageInfoData(PackageInfoData &&other) = default;

	bool operator==(const PackageInfoData &other) const
	{
		return provider == other.provider &&
				package == other.package &&
				version == other.version;
	}
};

}

QRegularExpression PackageInfo::packageRegexp() {
	return QRegularExpression{
		QStringLiteral(R"__(^(?:([^:]*)::)?(.*?)(?:@([\w\.-]*))?$)__"),
				QRegularExpression::CaseInsensitiveOption
	};
}

PackageInfo::PackageInfo(QString provider, const QString &package, QVersionNumber version) :
	d{new PackageInfoData{std::move(provider), package, std::move(version)}}
{}

PackageInfo::~PackageInfo() = default;

PackageInfo::PackageInfo(const PackageInfo &other) = default;

PackageInfo::PackageInfo(PackageInfo &&other) noexcept = default;

PackageInfo &PackageInfo::operator=(const PackageInfo &other) = default;

PackageInfo &PackageInfo::operator=(PackageInfo &&other) noexcept = default;

QString PackageInfo::provider() const
{
	return d->provider;
}

QString PackageInfo::package() const
{
	return d->package;
}

QVersionNumber PackageInfo::version() const
{
	return d->version;
}

bool PackageInfo::isComplete() const
{
	return !d->provider.isEmpty() &&
			!d->package.isEmpty() &&
			!d->version.isNull();
}

QString PackageInfo::toString(bool scoped) const
{
	auto res = d->package;
	if(!d->provider.isNull())
		res.prepend(d->provider + QStringLiteral("::"));
	if(!d->version.isNull())
		res.append(QLatin1Char('@') + d->version.toString());
	if(scoped)
		return QStringLiteral("%{pkg}") + res + QStringLiteral("%{end}");
	else
		return res;
}

bool PackageInfo::operator==(const PackageInfo &other) const
{
	return d == other.d ||
			*d == *other.d;
}

bool PackageInfo::operator!=(const PackageInfo &other) const
{
	return !operator ==(other);
}

void PackageInfo::setProvider(QString provider)
{
	d->provider = std::move(provider);
}

void PackageInfo::setPackage(const QString &package)
{
	d->package = package.toLower();
}

void PackageInfo::setVersion(QVersionNumber version)
{
	d->version = std::move(version);
}

uint qpmx::qHash(const PackageInfo &t, uint seed)
{
	return qHash(t.provider(), seed) ^
			qHash(t.package(), seed) ^
			qHash(t.version(), seed);
}
