@echo off
cls
echo [ amongunlock.cpp    ]
echo [ https://roooot.dev ]
echo.

echo [ 1/2 ] compiling icon
windres "%~dp0resources\icon.rc" -o "%~dp0resources\icon.o" --use-temp-file
if errorlevel 1 (
    echo [ - ] failed to compile icon.rc
    timeout 5
    exit /b 1
)

cls
echo [ amongunlock.cpp    ]
echo [ https://roooot.dev ]
echo.

echo [ 2/2 ] building .exe
g++ -std=c++17 -O2 -Wall "%~dp0amongunlock.cpp" "%~dp0resources\icon.o" -o "%~dp0amongunlock.exe" -luser32 -lkernel32 -lpsapi
if errorlevel 1 (
    echo [ - ] failed to build amongunlock.exe
    timeout 5
    exit /b 1
)

echo.
echo [ i ] build done
timeout 5
cls
exit /b 1