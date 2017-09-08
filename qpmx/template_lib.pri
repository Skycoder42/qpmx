INCLUDEPATH += "$$PWD/include"

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/lib -l$${QPMX_LIBNAME}
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib -l$${QPMX_LIBNAME}d
else:unix: LIBS += -L$$PWD/lib/ -l$${QPMX_LIBNAME}
