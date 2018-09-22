#ifndef BRIDGE_H
#define BRIDGE_H

#include <qpmxbridge_p.h>
#include "command.h"

namespace qpmx {
namespace priv {

class Bridge : public QpmxBridge
{
public:
	Bridge(Command *command);

	QDir qpmxCacheDir() const override;
	QDir srcDir() const override;
	QDir buildDir() const override;
	QDir tmpDir() const override;
	QString pkgEncode(const QString &name) const override;
	QString pkgDecode(QString name) const override;

private:
	Command *_command;
};

}
}

#endif // BRIDGE_H
