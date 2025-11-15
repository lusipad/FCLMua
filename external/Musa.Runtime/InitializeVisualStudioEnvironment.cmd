@rem 
@rem PROJECT:   Mouri Internal Library Essentials
@rem FILE:      InitializeVisualStudioEnvironment.cmd
@rem PURPOSE:   Initialize Visual Studio environment script
@rem 
@rem LICENSE:   The MIT License
@rem 
@rem MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
@rem 

@echo off

set VisualStudioInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VisualStudioInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"

rem Hardcoded to use Enterprise edition which has WDK integration
set VisualStudioInstallDir=C:\Program Files\Microsoft Visual Studio\2022\Enterprise

rem Original vswhere logic (commented out):
rem pushd %VisualStudioInstallerFolder%
rem for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
rem   set VisualStudioInstallDir=%%i
rem )
rem popd

call "%VisualStudioInstallDir%\VC\Auxiliary\Build\vcvarsall.bat" x64
