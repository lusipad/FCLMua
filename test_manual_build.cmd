@echo off
echo Testing manual_build.cmd...
echo.
call "tools\manual_build.cmd" Debug
echo.
echo Exit code: %ERRORLEVEL%
