#ifndef LIBQPMX_GLOBAL_H
#define LIBQPMX_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QPMX_LIBRARY)
#  define LIBQPMX_EXPORT Q_DECL_EXPORT
#else
#  define LIBQPMX_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBQPMX_GLOBAL_H
