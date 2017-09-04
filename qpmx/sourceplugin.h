#ifndef QPMX_SOURCEPLUGIN_H
#define QPMX_SOURCEPLUGIN_H

#include <QtCore/QObject>
#include <QtCore/QDir>
#include <QtCore/QVariantHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QVersionNumber>
#include <QtCore/QSharedData>

namespace qpmx { //qpmx public namespace

class PackageInfo
{
public:
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

private:
	struct Data : public QSharedData {
		QString provider;
		QString package;
		QVersionNumber version;

		inline Data(QString provider, QString package, QVersionNumber version) :
			QSharedData(),
			provider(provider),
			package(package),
			version(version)
		{}
		inline Data(const Data &other) :
			QSharedData(other),
			provider(other.provider),
			package(other.package),
			version(other.version)
		{}
	};

	QSharedDataPointer<Data> d;
};

class SourcePlugin
{
public:
	virtual inline ~SourcePlugin() = default;

	virtual QString packageSyntax() const = 0;
	virtual bool packageNameValid(const QString &packageName) const = 0;

public:
	virtual void searchPackage(int requestId, const QString &provider, const QString &query) = 0;
	virtual void getPackageSource(int requestId, const qpmx::PackageInfo &package, const QDir &targetDir, const QVariantHash &extraParameters = {}) = 0;

public:
	virtual void searchResult(int requestId, const QStringList &package) = 0;
	virtual void sourceFetched(int requestId) = 0;
	virtual void sourceError(int requestId, const QString &error) = 0;
};

}

Q_DECLARE_METATYPE(qpmx::PackageInfo)

#define SourcePlugin_iid "de.skycoder42.qpmx.SourcePlugin"
Q_DECLARE_INTERFACE(qpmx::SourcePlugin, SourcePlugin_iid)

#endif // QPMX_SOURCEPLUGIN_H
