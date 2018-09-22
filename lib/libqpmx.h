#ifndef LIBQPMX_H
#define LIBQPMX_H

#include <QDir>

#include "libqpmx_global.h"

namespace qpmx {

LIBQPMX_EXPORT QDir qpmxCacheDir();

LIBQPMX_EXPORT QDir srcDir();
LIBQPMX_EXPORT QDir buildDir();
LIBQPMX_EXPORT QDir tmpDir();

LIBQPMX_EXPORT QString pkgEncode(const QString &name);
LIBQPMX_EXPORT QString pkgDecode(QString name);

}

#endif // LIBQPMX_H
