LIB_OUTDIR = $$shadowed($$PWD)

win32:CONFIG(release, debug|release): LIBS += -L$$LIB_OUTDIR/lib/release/ -lqpmx
else:win32:CONFIG(debug, debug|release): LIBS += -L$$LIB_OUTDIR/lib/debug/ -lqpmx
else:unix: LIBS += -L$$LIB_OUTDIR/lib/ -lqpmx

DEFINES += QTCOROUTINE_EXPORTED

INCLUDEPATH += $$PWD/lib $$PWD/submodules/qtcoroutines/
DEPENDPATH += $$PWD/lib $$PWD/submodules/qtcoroutines/
