#ifndef QPMX_SOURCEPLUGIN_H
#define QPMX_SOURCEPLUGIN_H

#include "packageinfo.h"

#include <QtCore/QObject>
#include <QtCore/QDir>
#include <QtCore/QVariantHash>
#include <QtCore/QJsonObject>
#include <QtCore/QException>

namespace qpmx { //qpmx public namespace

class LIBQPMX_EXPORT SourcePlugin
{
	Q_DISABLE_COPY(SourcePlugin)
public:
	SourcePlugin();
	virtual ~SourcePlugin();

	virtual bool canSearch(const QString &provider) const = 0;
	virtual bool canPublish(const QString &provider) const = 0;
	virtual QString packageSyntax(const QString &provider) const = 0;
	virtual bool packageValid(const qpmx::PackageInfo &package) const = 0;

	virtual QJsonObject createPublisherInfo(const QString &provider) = 0;
	virtual QStringList searchPackage(const QString &provider, const QString &query) = 0;
	virtual QVersionNumber findPackageVersion(const qpmx::PackageInfo &package) = 0;
	virtual void getPackageSource(const qpmx::PackageInfo &package, const QDir &targetDir) = 0;
	virtual void publishPackage(const QString &provider, const QDir &qpmxDir, const QVersionNumber &version, const QJsonObject &publisherInfo) = 0;

	virtual void cancelAll(int timeout) = 0;
};

class LIBQPMX_EXPORT SourcePluginException : public QException
{
public:
	SourcePluginException(QByteArray errorMessage);
	SourcePluginException(const QString &errorMessage);
	SourcePluginException(const char *errorMessage);

	const char *what() const noexcept override;
	QString qWhat() const;

	void raise() const override;
	QException *clone() const override;

protected:
	const QByteArray _message;
};

}

#define SourcePlugin_iid "de.skycoder42.qpmx.SourcePlugin"
Q_DECLARE_INTERFACE(qpmx::SourcePlugin, SourcePlugin_iid)

#endif // QPMX_SOURCEPLUGIN_H
