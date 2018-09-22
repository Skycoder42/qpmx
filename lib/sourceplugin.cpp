#include "sourceplugin.h"
using namespace qpmx;

SourcePlugin::SourcePlugin() = default;

SourcePlugin::~SourcePlugin() = default;



SourcePluginException::SourcePluginException(QByteArray errorMessage) :
	_message{std::move(errorMessage)}
{}

SourcePluginException::SourcePluginException(const QString &errorMessage) :
	SourcePluginException{errorMessage.toUtf8()}
{}

SourcePluginException::SourcePluginException(const char *errorMessage) :
	SourcePluginException{QByteArray{errorMessage}}
{}

const char *SourcePluginException::what() const noexcept
{
	return _message.constData();
}

QString SourcePluginException::qWhat() const
{
	return QString::fromUtf8(_message);
}

void SourcePluginException::raise() const
{
	throw *this;
}

QException *SourcePluginException::clone() const
{
	return new SourcePluginException{_message};
}
