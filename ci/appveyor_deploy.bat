:: deploy
setlocal

:: "normal" deployment
cd install
move bin\qpmx.exe qpmx.exe || exit \b 1
rmdir bin
C:\projects\Qt\%QT_VER%\%PLATFORM%\bin\windeployqt.exe --release --no-translations qpmx.exe || exit \b 1

:: create qt.conf & license
echo [Paths] > qt.conf || exit \b 1
echo Prefix=. >> qt.conf || exit \b 1
copy ..\LICENSE .\LICENSE.txt || exit \b 1
