:: build
setlocal

set qtplatform=%PLATFORM%
set PATH=C:\projects\;C:\Qt\%QT_VER%\%qtplatform%\bin;%PATH%;%CD%\build-%qtplatform%\qpmx\release;

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /B 1

:: install plugins into qt
mkdir C:\Qt\%QT_VER%\%qtplatform%\plugins\qpmx || exit /B 1
xcopy /s build-%qtplatform%\plugins\qpmx C:\Qt\%QT_VER%\%qtplatform%\plugins\qpmx || exit /B 1

:: build tests (bin and src)
:: compile, compile-dev, src-dev, src
for /L %%i IN (0, 1, 3) DO (
	if "%%i" == "1" (
		ren submodules\qpmx-sample-package\qpmx-test\qpmx.json.user.cm qpmx.json.user
	)
	if "%%i" == "2" (
		del submodules\qpmx-sample-package\qpmx-test\qpmx.json
		ren submodules\qpmx-sample-package\qpmx-test\qpmx.json.src qpmx.json
	)
	if "%%i" == "3" (
		del submodules\qpmx-sample-package\qpmx-test\qpmx.json.user
	)

	mkdir build-%qtplatform%\tests-%%i
	cd build-%qtplatform%\tests-%%i

	C:\Qt\%QT_VER%\%qtplatform%\bin\qmake -r "CONFIG += debug_and_release" ../../submodules/qpmx-sample-package/qpmx-test/ || (
		for /D %%G in (C:\Users\appveyor\AppData\Local\Temp\1\qpmx*) do (
			type %%G\qmake.stdout.log
			type %%G\make.stdout.log
			type %%G\install.stdout.log
		)

		type "C:\tmp\.qpmx-dev-cache\build\git\https%%3A%%2F%%2Fgithub.com%%2Fskycoder42%%2Fqpmx-sample-package.git\1.0.14\qmake.stdout.log"
		type "C:\tmp\.qpmx-dev-cache\build\git\https%%3A%%2F%%2Fgithub.com%%2Fskycoder42%%2Fqpmx-sample-package.git\1.0.14\make.stdout.log"
		type "C:\tmp\.qpmx-dev-cache\build\git\https%%3A%%2F%%2Fgithub.com%%2Fskycoder42%%2Fqpmx-sample-package.git\1.0.14\install.stdout.log"

		exit /B 1
	)

	nmake all || exit /B 1
	nmake lrelease || exit /B 1
	call :mktemp
	nmake INSTALL_ROOT=%TMP_DIR% install || exit /B 1


	.\release\test.exe || exit /B 1
	.\debug\test.exe || exit /B 1

	cd ..\..
)

:: extra tests
:: test install without provider/version
qpmx install -cr --verbose de.skycoder42.qpathedit https://github.com/Skycoder42/qpmx-sample-package.git
qpmx list providers

:: create tmp dir
:mktemp
:uniqLoop
set "TMP_DIR=\tmp\install~%RANDOM%"
if exist "C:%TMP_DIR%" goto :uniqLoop
mkdir "C:%TMP_DIR%"
