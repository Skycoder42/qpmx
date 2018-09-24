@echo off
setlocal

set qtplatform=%PLATFORM%
for %%* in (.) do set CurrDirName=%%~nx*

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /B 1

cd build-%qtplatform%
nmake INSTALL_ROOT=\projects\%CurrDirName%\install deploy || exit /B 1
nmake INSTALL_ROOT=\projects\%CurrDirName%\install package || exit /B 1
cd ..

dir /s install\
