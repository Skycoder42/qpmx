:: build
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /B 1

set qtplatform=%PLATFORM%
set PATH=C:\Qt\%QT_VER%\%qtplatform%\bin;%PATH%;%CD%\build-%qtplatform%\qpmx;

:: install plugins into qt
dir
cd build-%qtplatform%
dir
cd plugins
dir

:: cd build-%qtplatform%\plugins
nmake install || exit /B 1
cd ..\..

mkdir build-%qtplatform%\tests
cd build-%qtplatform%\tests

C:\Qt\%QT_VER%\%qtplatform%\bin\qmake -r ../../submodules/qpmx-sample-package/qpmx-test/ || exit /B 1
nmake || exit /B 1

.\test.exe || exit /B 1
