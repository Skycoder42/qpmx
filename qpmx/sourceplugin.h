#ifndef QPMX_SOURCEPLUGIN_H
#define QPMX_SOURCEPLUGIN_H

#include "packageinfo.h"

#include <QtCore/QObject>
#include <QtCore/QDir>
#include <QtCore/QVariantHash>
#include <QtCore/QJsonObject>
#include <QtCore/QException>

namespace qpmx { //qpmx public namespace

class SourcePlugin
{
	Q_DISABLE_COPY(SourcePlugin)
public:
	inline SourcePlugin() = default;
	virtual inline ~SourcePlugin() = default;

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

class SourcePluginException : public QException
{
public:
	inline SourcePluginException(QByteArray errorMessage) :
		_message{std::move(errorMessage)}
	{}
	inline SourcePluginException(const QString &errorMessage) :
		SourcePluginException{errorMessage.toUtf8()}
	{}
	inline SourcePluginException(const char *errorMessage) :
		SourcePluginException{QByteArray{errorMessage}}
	{}

	inline const char *what() const noexcept override {
		return _message.constData();
	}
	inline QString qWhat() const {
		return QString::fromUtf8(_message);
	}

	inline void raise() const override {
		throw *this;
	}
	inline QException *clone() const override {
		return new SourcePluginException{_message};
	}

protected:
	const QByteArray _message;
};

}

#define SourcePlugin_iid "de.skycoder42.qpmx.SourcePlugin"
Q_DECLARE_INTERFACE(qpmx::SourcePlugin, SourcePlugin_iid)

#endif // QPMX_SOURCEPLUGIN_H
