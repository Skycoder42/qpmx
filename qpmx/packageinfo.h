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

	inline PackageInfo(const QString &provider = {},const QString &package = {}, const QVersionNumber &version = {}) :
		d(new Data(provider, package, version))
	{}
	inline PackageInfo(const PackageInfo &other) = default;
	inline PackageInfo &operator=(const PackageInfo &other) = default;

	inline QString provider() const {
		return d->provider;
	}
	inline QString package() const {
		return d->package;
	}
	inline QVersionNumber version() const {
		return d->version;
	}

	inline bool isComplete() const {
		return !d->provider.isEmpty() &&
				!d->package.isEmpty() &&
				!d->version.isNull();
	}
	inline QString toString(bool scoped = true) const {
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

	inline bool operator==(const PackageInfo &other) const {
		return d == other.d ||
				*d == *other.d;
	}

	inline bool operator!=(const PackageInfo &other) const {
		return !operator ==(other);
	}

private:
	struct Data : public QSharedData {
		QString provider;
		QString package;
		QVersionNumber version;

		inline Data(QString provider, const QString &package, QVersionNumber version) :
			QSharedData(),
			provider(std::move(provider)),
			package(package.toLower()),
			version(std::move(version))
		{}
		inline Data(const Data &other) = default;

		inline bool operator==(const Data &other) const {
			return provider == other.provider &&
					package == other.package &&
					version == other.version;
		}
	};

	QSharedDataPointer<Data> d;
};

inline uint qHash(const PackageInfo &t, uint seed) {
	return qHash(t.provider(), seed) ^
			qHash(t.package(), seed) ^
			qHash(t.version(), seed);
}

}

Q_DECLARE_METATYPE(qpmx::PackageInfo)

#endif // PACKAGEINFO_H
