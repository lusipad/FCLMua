@echo off
echo Starting FCL GUI Demo...
echo.
echo Pass --mode=r3 to force the R3 backend, or --mode=driver to require the kernel driver.
echo Note: The program will show a detailed error message if it fails.
echo Press Ctrl+C to stop the program.
echo.
start "" "build\Release\fcl_gui_demo.exe" %*
