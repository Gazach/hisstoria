@echo off
setlocal

if "%1"=="" (
    echo Usage: build.bat [debug^|release]
    goto end
)

REM Path to Clang — update if your LLVM is installed elsewhere
set CLANG_PATH=C:\Program Files\LLVM\bin

if /I "%1"=="debug" (
    set CMAKE_BUILD_TYPE=Debug
) else if /I "%1"=="release" (
    set CMAKE_BUILD_TYPE=Release
) else (
    echo Invalid build type: %1
    echo Use "debug" or "release"
    goto end
)

set ROOT_DIR=%~dp0

if not exist "%ROOT_DIR%build\%1" mkdir "%ROOT_DIR%build\%1"
cd /d "%ROOT_DIR%build\%1"

echo [cmake] Configuring %CMAKE_BUILD_TYPE% build...
cmake -G "Ninja" ^
  -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
  -DCMAKE_C_COMPILER="%CLANG_PATH%\clang.exe" ^
  -DCMAKE_CXX_COMPILER="%CLANG_PATH%\clang++.exe" ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
  "%ROOT_DIR%."
if errorlevel 1 (
    echo [error] CMake configuration failed.
    goto end
)

echo [ninja] Building...
ninja
if errorlevel 1 (
    echo [error] Build failed.
    goto end
)

echo [run] Running hiss.exe...
hiss.exe

:end
endlocal
pause
