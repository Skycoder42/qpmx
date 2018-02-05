:: build
setlocal
setlocal enabledelayedexpansion

set qtplatform=%PLATFORM%
set "PATH=%CD%\build-%qtplatform%\qpmx\release;C:\projects\;C:\projects\Qt\%QT_VER%\%qtplatform%\bin;%PATH%;"
where.exe qpmx.exe

if "%qtplatform%" == "msvc2017_64" goto :setup_vc
	set PATH=C:\projects\Qt\Tools\mingw530_32\bin;%PATH%;
	set MAKEFLAGS=-j%NUMBER_OF_PROCESSORS%
	set MAKE=mingw32-make
	goto :setup_done
:setup_vc
	call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64 || exit /B 1
	set MAKE=nmake
:setup_done

:: install plugins into qt
mkdir C:\projects\Qt\%QT_VER%\%qtplatform%\plugins\qpmx || exit /B 1
xcopy /s build-%qtplatform%\plugins\qpmx C:\projects\Qt\%QT_VER%\%qtplatform%\plugins\qpmx || exit /B 1
qpmx list providers || (
   echo Failed to run qpmx with error code %errorlevel%
   exit /B %errorlevel%
)

:: build tests (bin and src)
:: compile, compile-dev, src-dev, src
set "QMAKE_FLAGS=%QMAKE_FLAGS% CONFIG+=debug_and_release"
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

	for /L %%j IN (0, 1, 2) DO (
		echo running test case %%i-%%j
		
		set "M_FLAGS=%QMAKE_FLAGS%"
		if "%%j" == "1" (
			set "M_FLAGS=%QMAKE_FLAGS% CONFIG+=test_as_shared"
		)
		if "%%j" == "2" (
			set "M_FLAGS=%QMAKE_FLAGS% CONFIG+=test_as_static"
		)
	
		mkdir build-%qtplatform%\tests-%%i-%%j
		cd build-%qtplatform%\tests-%%i-%%j

		echo running qmake with flags %M_FLAGS%
		C:\projects\Qt\%QT_VER%\%qtplatform%\bin\qmake %M_FLAGS% ../../submodules/qpmx-sample-package/qpmx-test/ || (
			for /D %%G in (C:\Users\appveyor\AppData\Local\Temp\1\qpmx*) do (
				echo %%G
				type %%G\qmake.stdout.log
				type %%G\make.stdout.log
				type %%G\install.stdout.log
			)

			type "C:\tmp\.qpmx-dev-cache\build\git\https.3A.2F.2Fgithub.2Ecom.2Fskycoder42.2Fqpmx-sample-package.2Egit\1.0.14\qmake.stdout.log"
			type "C:\tmp\.qpmx-dev-cache\build\git\https.3A.2F.2Fgithub.2Ecom.2Fskycoder42.2Fqpmx-sample-package.2Egit\1.0.14\make.stdout.log"
			type "C:\tmp\.qpmx-dev-cache\build\git\https.3A.2F.2Fgithub.2Ecom.2Fskycoder42.2Fqpmx-sample-package.2Egit\1.0.14\install.stdout.log"

			exit /B 1
		)

		%MAKE% qmake_all || exit /B 1
		%MAKE% || exit /B 1
		%MAKE% lrelease || exit /B 1
		call :mktemp
		%MAKE% INSTALL_ROOT=%TMP_DIR% install || exit /B 1

		if "%%j" == "0" (
			.\release\test.exe || exit /B 1
			.\debug\test.exe || exit /B 1
		)

		cd ..\..
	)
)

:: extra tests
:: test install without provider/version
qpmx install -cr --verbose de.skycoder42.qpathedit https://github.com/Skycoder42/qpmx-sample-package.git

:: create tmp dir
:mktemp
:uniqLoop
set "TMP_DIR=\tmp\install~%RANDOM%"
if exist "C:%TMP_DIR%" goto :uniqLoop
mkdir "C:%TMP_DIR%"
