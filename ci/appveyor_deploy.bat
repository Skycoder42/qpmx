:: deploy
setlocal
@echo on

:: "normal" deployment
cd install
move bin\qpmx.exe qpmx.exe || exit \b 1
rmdir bin
C:\projects\Qt\%QT_VER%\%qtplatform%\bin\windeployqt.exe --release --no-translations qpmx.exe || exit \b 1

:: create qt.conf
echo [Paths] > qt.conf
echo Prefix=. >> qt.conf

