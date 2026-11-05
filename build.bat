@echo off
setlocal

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo Could not find Visual Studio build tools at:
    echo   %VCVARS%
    exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 exit /b %errorlevel%

if not exist build mkdir build

cl /guard:cf- /guard:ehcont- /Zo- /Zc:inline,tlsGuards- /D_HAS_EXCEPTIONS=0 /MD ^
    /O1 /Os /Qpar /arch:AVX2 /nologo /W4 /WX- /std:c11 /GR- /GL /Oy /Gy /GS- /GF /Ob2 /Oi /volatileMetadata- ^
    /D_CRT_SECURE_NO_WARNINGS /Fo"build\\" /Fe:"build\Fling.exe" ^
    src\main.c ^
    /link /LTCG ^
    /map:build\Fling.map ^
    /nocoffgrpinfo /EMITVOLATILEMETADATA:NO ^
    /fixed /dynamicbase:no /safeseh:no ^
    /SUBSYSTEM:WINDOWS ^
    /merge:.pdata=.text /merge:.rdata=.text
if errorlevel 1 exit /b %errorlevel%

echo Built build\Fling.exe successfully.
