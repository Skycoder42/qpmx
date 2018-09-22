#ifndef QPMXBRIDGE_P_H
#define QPMXBRIDGE_P_H

#include <QDir>
#include "libqpmx_global.h"

namespace qpmx {
namespace priv {

class LIBQPMX_EXPORT QpmxBridge
{
public:
	QpmxBridge();
	virtual ~QpmxBridge();

	static void registerInstance(QpmxBridge *bridge);
	static QpmxBridge *instance();

	virtual QDir qpmxCacheDir() const = 0;
	virtual QDir srcDir() const = 0;
	virtual QDir buildDir() const = 0;
	virtual QDir tmpDir() const = 0;
	virtual QString pkgEncode(const QString &name) const = 0;
	virtual QString pkgDecode(QString name) const = 0;

private:
	static QpmxBridge *_instance;
};

}
}

#endif // QPMXBRIDGE_P_H
