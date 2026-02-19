@echo off
chcp 65001 >nul 2>&1
setlocal

echo ========================================
echo OpenWrt Connect - Build Script
echo ========================================
echo.

REM ----------------------------------------
REM Verify config file exists
REM ----------------------------------------
if not exist "openwrt-connect.conf" (
    echo [ERROR] openwrt-connect.conf not found.
    echo This file defines available commands.
    pause
    exit /b 1
)
echo Config: openwrt-connect.conf found
echo.

REM ----------------------------------------
REM Find gcc
REM ----------------------------------------
if exist "C:\mingw64\bin\gcc.exe" (
    set "GCC=C:\mingw64\bin\gcc.exe"
    set "WINDRES=C:\mingw64\bin\windres.exe"
    goto :found_gcc
)

gcc --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set "GCC=gcc"
    set "WINDRES=windres"
    goto :found_gcc
)

echo [ERROR] gcc not found.
echo Please install MinGW-w64 to C:\mingw64\bin
echo https://winlibs.com/
pause
exit /b 1

:found_gcc
echo Using: %GCC%
echo.

REM ----------------------------------------
REM Build executable
REM ----------------------------------------
echo Building openwrt-connect.exe...
"%WINDRES%" openwrt-connect.rc -o openwrt-connect_res.o
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] windres failed & pause & exit /b 1 )
"%GCC%" -o openwrt-connect.exe openwrt-connect.c openwrt-connect_res.o -mconsole -liphlpapi -lws2_32
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] gcc failed & pause & exit /b 1 )
del /Q openwrt-connect_res.o 2>nul
echo OK
echo.

REM ----------------------------------------
REM Build MSI (optional)
REM ----------------------------------------
set WIXPATH=C:\Program Files (x86)\WiX Toolset v3.14\bin

if not exist "%WIXPATH%\candle.exe" (
    echo [SKIP] WiX Toolset not found - skipping MSI build
    echo.
    echo To build MSI, install WiX Toolset v3.11:
    echo https://github.com/wixtoolset/wix3/releases/tag/wix3112rtm
    echo.
    pause
    exit /b 0
)

REM ----------------------------------------
REM Generate Product.wxs from config
REM ----------------------------------------
echo Generating Product.wxs from openwrt-connect.conf...

REM Try PowerShell 7 first (pwsh), then fall back to Windows PowerShell (powershell)
set "PS_EXE="

pwsh -Command "exit 0" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set "PS_EXE=pwsh"
    goto :found_powershell
)

powershell -Command "exit 0" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set "PS_EXE=powershell"
    goto :found_powershell
)

echo [ERROR] PowerShell not found.
echo Please install PowerShell 5.0 or later.
pause
exit /b 1

:found_powershell
"%PS_EXE%" -ExecutionPolicy Bypass -File generate-wxs.ps1
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] WXS generation failed & pause & exit /b 1 )
echo.

REM ----------------------------------------
REM Compile and link MSI
REM ----------------------------------------
echo Building MSI...
if exist Product.wixobj del /Q Product.wixobj
if exist openwrt-connect.msi del /Q openwrt-connect.msi

"%WIXPATH%\candle.exe" Product.wxs -ext WixUIExtension
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] WiX compilation failed & pause & exit /b 1 )

"%WIXPATH%\light.exe" Product.wixobj -ext WixUIExtension -out openwrt-connect.msi
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] WiX linking failed & pause & exit /b 1 )

echo.
echo ========================================
echo Build completed!
echo ========================================
echo Output: openwrt-connect.exe + openwrt-connect.conf
echo   MSI: openwrt-connect.msi
echo.
pause
