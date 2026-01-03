@echo off
setlocal

echo =============================================
echo    OrcaSlicer Windows Installer / Builder
echo =============================================

echo.
echo Checking for Visual Studio...
where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: Visual Studio Compiler (cl.exe) not found in PATH.
    echo Please install Visual Studio 2019 or 2022 + C++ Desktop Development Workload.
    echo Then run this script from the "Developer Command Prompt for VS".
    pause
    exit /b 1
)

echo.
echo Checking for CMake...
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo CMake not found. Attempting to install via Winget...
    winget install Kitware.CMake -e --source winget
    if %errorlevel% neq 0 (
        echo Error: Failed to install CMake. Please install manually from https://cmake.org/download/
        pause
        exit /b 1
    )
    echo CMake installed. Please restart the script to refresh PATH.
    pause
    exit /b 0
) else (
    echo CMake is already installed.
)

echo.
echo Checking for Git...
where git >nul 2>nul
if %errorlevel% neq 0 (
    echo Git not found. Attempting to install via Winget...
    winget install Git.Git -e --source winget
    if %errorlevel% neq 0 (
        echo Error: Failed to install Git. Please install manually from https://git-scm.com/downloads
        pause
        exit /b 1
    )
    echo Git installed. Please restart the script to refresh PATH.
    pause
    exit /b 0
) else (
    echo Git is already installed.
)

echo.
echo Starting Build Process...
echo This build script will build dependencies first if needed.
echo.

call build_release_vs.bat all

if %errorlevel% neq 0 (
    echo.
    echo Build Failed!
    pause
    exit /b 1
)

echo.
echo =============================================
echo    Build Complete!
echo =============================================
echo You can find the executable in the 'build/OrcaSlicer/Release' (or similar) folder.
pause
