@echo off
:: Save the current directory
set CURRENT_DIR=%cd%

:: Change to the directory where the batch file is located
cd /d %~dp0

:: Call the vcvarsall.bat script for x64 architecture
call "G:\Visual Studio 2022\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Change back to the original directory
cd /d %CURRENT_DIR%

:: Open a new command prompt window
cmder
