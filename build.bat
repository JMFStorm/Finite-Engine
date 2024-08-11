cl /std:c++20 /Zi /MTd /DDEBUG /D_UNICODE /DUNICODE /Fo"debug\\" /Fd"debug\\" src/win32_main.cpp /link /LIBPATH:"./lib" user32.lib gdi32.lib kernel32.lib comdlg32.lib shell32.lib d3d11.lib D3DCompiler.lib DXGI.lib xaudio2.lib Ole32.lib /SUBSYSTEM:WINDOWS /OUT:"debug\finite_engine_debug.exe"


@echo off
:: Check the error level (exit code) of the last command
if %ERRORLEVEL% neq 0 (
    :: Set text color to red
    color 0C
    echo :: BUILD FAILED! BUILD FAILED! BUILD FAILED! BUILD FAILED! BUILD FAILED! ::
    pause
    :: Reset color to default
    color 07
    exit /b 1
)
