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

if not defined VCPKG_ROOT (
    echo "install vcpkg"
    set "current_dir=%cd%"
    cd /D %USERPROFILE%
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg && bootstrap-vcpkg.bat
    cd /D %current_dir%
    set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
    set "PATH=%VCPKG_ROOT%;%PATH%"
    echo "Please add this variables to environment for future compile:"
    echo "VCPKG_ROOT=%USERPROFILE%\vcpkg"
    echo "PATH=%VCPKG_ROOT%;%PATH%"
)


cmake.exe --preset=%build_type%

cmake.exe --build build_windows

goto :eof


:show_help
echo This program build Centreon-Monitoring-Agent
echo   --release : Build on release mode
echo   --help     : help
goto :eof





