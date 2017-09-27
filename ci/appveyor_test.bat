:: build
setlocal

set qtplatform=%PLATFORM%
set PATH=C:\projects\;C:\Qt\%QT_VER%\%qtplatform%\bin;%PATH%;%CD%\build-%qtplatform%\qpmx\release;

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /B 1

:: install plugins into qt
mkdir C:\Qt\%QT_VER%\%qtplatform%\plugins\qpmx || exit /B 1
xcopy /s build-%qtplatform%\plugins\qpmx C:\Qt\%QT_VER%\%qtplatform%\plugins\qpmx || exit /B 1

:: build tests (bin and src)

for /L %%i IN (0, 1, 2) DO (
	if "%%i" == "1" (
		ren submodules\qpmx-sample-package\qpmx-test\qpmx.json.user.cm qpmx.json.user
	)
	if "%%i" == "2" (
		del submodules\qpmx-sample-package\qpmx-test\qpmx.json
		del submodules\qpmx-sample-package\qpmx-test\qpmx.json.user
		ren submodules\qpmx-sample-package\qpmx-test\qpmx.json.src qpmx.json
	)

	mkdir build-%qtplatform%\tests-%%i
	cd build-%qtplatform%\tests-%%i

	C:\Qt\%QT_VER%\%qtplatform%\bin\qmake -r "CONFIG += debug_and_release" ../../submodules/qpmx-sample-package/qpmx-test/ || exit /B 1
	nmake all || exit /B 1

	.\release\test.exe || exit /B 1
	.\debug\test.exe || exit /B 1

	cd ..\..
)

:: debug
build-%qtplatform%\qpmx\qtifw-installer\packages\de.skycoder42.qpmx\data\qpmx.exe list providers
