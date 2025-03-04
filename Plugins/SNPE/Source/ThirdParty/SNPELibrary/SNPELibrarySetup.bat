:: ============================================================================
:: 
::  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
::                 SPDX-License-Identifier: BSD-3-Clause
:: 
:: ============================================================================
::
::
:: ========================================================================================
::  Sets up the ThirdParty/SNPELibrary folder with the necessary includes and libraries.
::
::  IMPORTANT: Before using this script, ensure that
::      - the Qualcomm Neural Processing SDK is downloaded
::        (https://developer.qualcomm.com/software/qualcomm-neural-processing-sdk)
::
::  USAGE: SNPELibrarySetup.bat <SNPE_INSTALL_DIR>
::      - (the dir has the `include` and `lib` subfolders)
:: ========================================================================================

@echo off

set SNPE_SDK_ROOT=%1

set SNPE_LIBRARY_ROOT=.
set SNPE_PLUGIN_ROOT=..\..\..

echo SNPE SDK installed at: %SNPE_SDK_ROOT%

echo Setting up includes...
md %SNPE_LIBRARY_ROOT%\inc
xcopy %SNPE_SDK_ROOT%\include\SNPE %SNPE_LIBRARY_ROOT%\inc /E

echo Setting up libraries...
md %SNPE_LIBRARY_ROOT%\lib
md %SNPE_PLUGIN_ROOT%\Binaries
md %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty
md %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE

md %SNPE_LIBRARY_ROOT%\lib\x64
md %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\x64
copy %SNPE_SDK_ROOT%\lib\x86_64-windows-msvc\SNPE.lib %SNPE_LIBRARY_ROOT%\lib\x64
copy %SNPE_SDK_ROOT%\lib\x86_64-windows-msvc\SNPE.dll %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\x64

md %SNPE_LIBRARY_ROOT%\lib\Arm64
md %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\Arm64
copy %SNPE_SDK_ROOT%\lib\aarch64-windows-msvc\*.lib %SNPE_LIBRARY_ROOT%\lib\Arm64
copy %SNPE_SDK_ROOT%\lib\aarch64-windows-msvc\*.dll %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\Arm64
copy %SNPE_SDK_ROOT%\lib\hexagon-v68\unsigned\libSnpeHtpV??Skel.so %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\Arm64
copy %SNPE_SDK_ROOT%\lib\hexagon-v69\unsigned\libSnpeHtpV??Skel.so %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\Arm64
copy %SNPE_SDK_ROOT%\lib\hexagon-v73\unsigned\libSnpeHtpV??Skel.so %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\Arm64
copy %SNPE_SDK_ROOT%\lib\hexagon-v75\unsigned\libSnpeHtpV??Skel.so %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\Arm64
copy %SNPE_SDK_ROOT%\lib\hexagon-v79\unsigned\libSnpeHtpV??Skel.so %SNPE_PLUGIN_ROOT%\Binaries\ThirdParty\SNPE\Arm64

md %SNPE_LIBRARY_ROOT%\lib\android
copy %SNPE_SDK_ROOT%\lib\android\snpe-release.aar %SNPE_LIBRARY_ROOT%\lib\android

md %SNPE_LIBRARY_ROOT%\lib\aarch64-android
copy %SNPE_SDK_ROOT%\lib\aarch64-android\libSNPE.so %SNPE_LIBRARY_ROOT%\lib\aarch64-android
copy %SNPE_SDK_ROOT%\lib\aarch64-android\libSnpeHtpPrepare.so %SNPE_LIBRARY_ROOT%\lib\aarch64-android
copy %SNPE_SDK_ROOT%\lib\aarch64-android\libSnpeHtpV??Stub.so %SNPE_LIBRARY_ROOT%\lib\aarch64-android

echo All done!
@echo on