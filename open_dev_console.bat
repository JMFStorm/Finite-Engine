@echo off
:: Save the current directory
set CURRENT_DIR=%cd%

:: Change to the directory where the batch file is located
cd /d %~dp0

:: Call the vcvarsall.bat script for x64 architecture
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Change back to the original directory
cd /d %CURRENT_DIR%

:: Open a new command prompt window
cmder
