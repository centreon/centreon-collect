echo off

set "build_type=debug"

if "%~1" == "--help" (
    call :show_help
    goto :eof
) else if "%~1" == "--release" (
    set "build_type=release"
)

where /q cl.exe
IF ERRORLEVEL 1 (
    echo unable to find cl.exe, please run vcvarsall.bat or compile from x64 Native Tools Command Prompt for VS20xx
    exit /B
)

where /q cmake.exe
IF ERRORLEVEL 1 (
    echo unable to find cmake.exe, please install cmake.exe
    exit /B
)

where /q ninja.exe
IF ERRORLEVEL 1 (
    echo unable to find ninja.exe, please install ninja.exe
    exit /B
)

:: Use a vcpkg clone local to this repository (like CI does), pinned to the
:: same commit as the builtin-baseline CI injects in
:: .github\scripts\windows-agent-compile.ps1. vcpkg.json has no
:: builtin-baseline (Linux CI resolves a different one), so dependency
:: versions come from whatever VCPKG_ROOT has checked out: if that clone
:: moves, every port's ABI hash changes, the local binary cache misses and
:: grpc/abseil/boost rebuild from source. Pinning the checkout keeps builds
:: reproducible and fast.
set "vcpkg_commit=d015e31e90838a4c9dfa3eed45979bc70d9357fc"
set "VCPKG_ROOT=%~dp0vcpkg"

if not exist "%VCPKG_ROOT%\.git" (
    echo install vcpkg in %VCPKG_ROOT%
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%"
)

for /f %%i in ('git -C "%VCPKG_ROOT%" rev-parse HEAD') do set "vcpkg_head=%%i"
if not "%vcpkg_head%" == "%vcpkg_commit%" (
    echo pinning vcpkg to commit %vcpkg_commit%
    git -C "%VCPKG_ROOT%" fetch origin
    git -C "%VCPKG_ROOT%" checkout %vcpkg_commit%
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat"
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat"
)

set "PATH=%VCPKG_ROOT%;%PATH%"

cmake.exe --preset=%build_type%

cmake.exe --build build_windows

goto :eof


:show_help
echo This program build Centreon-Monitoring-Agent
echo   --release : Build on release mode
echo   --help     : help
goto :eof





