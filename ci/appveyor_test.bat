:: build
setlocal

set qtplatform=%PLATFORM%
set PATH=C:\projects\;C:\Qt\%QT_VER%\%qtplatform%\bin;%PATH%;%CD%\build-%qtplatform%\qpmx\release;

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /B 1

:: install plugins into qt
cd build-%qtplatform%\plugins
nmake install || exit /B 1
cd ..\..

mkdir build-%qtplatform%\tests
cd build-%qtplatform%\tests

C:\Qt\%QT_VER%\%qtplatform%\bin\qmake -r "CONFIG += debug_and_release" ../../submodules/qpmx-sample-package/qpmx-test/ || exit /B 1
nmake all || exit /B 1

.\release\test.exe || exit /B 1
.\debug\test.exe || exit /B 1
