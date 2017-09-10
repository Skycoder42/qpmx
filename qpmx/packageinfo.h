#ifndef PACKAGEINFO_H
#define PACKAGEINFO_H

#include <QtCore/QVersionNumber>
#include <QtCore/QSharedData>
#include <QtCore/QRegularExpression>

namespace qpmx {

class PackageInfo
{
public:
	inline static QRegularExpression packageRegexp() {
		return QRegularExpression{
			QStringLiteral(R"__(^(?:([^:]*)::)?(.*?)(?:@([\w\.-]*))?$)__"),
			QRegularExpression::CaseInsensitiveOption
		};
	}

	inline PackageInfo(QString provider = {}, QString package = {}, QVersionNumber version = {}) :
		d(new Data(provider, package, version))
	{}
	inline PackageInfo(const PackageInfo &other) :
		d(other.d)
	{}

	inline PackageInfo &operator=(const PackageInfo &other) {
		d = other.d;
		return *this;
	}

	inline QString provider() const {
		return d->provider;
	}
	inline QString package() const {
		return d->package;
	}
	inline QVersionNumber version() const {
		return d->version;
	}
	inline QString toString() const {
		auto res = d->package;
		if(!d->provider.isNull())
			res.prepend(d->provider + QStringLiteral("::"));
		if(!d->version.isNull())
			res.append(QLatin1Char('@') + d->version.toString());
		return QStringLiteral("%{pkg}") + res + QStringLiteral("%{endpkg}");
	}

	inline bool operator==(const PackageInfo &other) const {
		return d == other.d ||
				*d == *other.d;
	}

private:
	struct Data : public QSharedData {
		QString provider;
		QString package;
		QVersionNumber version;

		inline Data(QString provider, QString package, QVersionNumber version) :
			QSharedData(),
			provider(provider),
			package(package.toLower()),
			version(version)
		{}
		inline Data(const Data &other) :
			QSharedData(other),
			provider(other.provider),
			package(other.package),
			version(other.version)
		{}

		inline bool operator==(const Data &other) const {
			return provider == other.provider &&
					package == other.package &&
					version == other.version;
		}
	};

	QSharedDataPointer<Data> d;
};

}

#endif // PACKAGEINFO_H
