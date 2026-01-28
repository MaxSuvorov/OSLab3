@echo off
echo Building LabWork for Windows...

if not exist build mkdir build
cd build

cmake -G "MinGW Makefiles" ..
if %errorlevel% neq 0 (
    echo CMake configuration failed
    pause
    exit /b %errorlevel%
)

cmake --build .
if %errorlevel% neq 0 (
    echo Build failed
    pause
    exit /b %errorlevel%
)

echo.
echo Build completed successfully!
echo Executable: build\labwork.exe
echo.
pause