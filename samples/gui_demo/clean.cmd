@echo off
REM Clean build artifacts for FCL GUI Demo

echo Cleaning build artifacts...

if exist "build" (
    rmdir /s /q "build"
    echo Removed: build\
)

if exist "*.obj" (
    del /q *.obj
    echo Removed: *.obj
)

if exist "*.pdb" (
    del /q *.pdb
    echo Removed: *.pdb
)

echo.
echo Clean complete!
