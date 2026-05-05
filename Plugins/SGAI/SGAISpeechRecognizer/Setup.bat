:: Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
:: SPDX-License-Identifier: BSD-3-Clause

@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: VoiceAI ASR SDK Setup Script
:: Installs the VoiceAI ASR Community SDK for the Unreal Engine plugin.
::
:: Usage:
::   Setup.bat "path\to\extracted\folder"   - Install from already-extracted folder
::
:: Download the SDK from:
::   https://qpm.qualcomm.com/#/main/tools/details/VoiceAI_ASR
::
:: Location: {repo-root}/plugin/SGAISpeechRecognizer/Setup.bat
:: Target:   {repo-root}/plugin/SGAISpeechRecognizer/Source/VoiceAIASR/ThirdParty/VoiceAIASRLib/
:: ============================================================================

:: Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"
set "THIRDPARTY_DIR=%SCRIPT_DIR%Source\VoiceAIASR\ThirdParty\VoiceAIASRLib"

:: ============================================================================
:: Parse command-line parameters
:: ============================================================================

if "%~1"=="" goto :show_usage

set "USER_PARAM=%~1"

:: Check if it's a directory path
if exist "%USER_PARAM%\*" (
    set "USER_FOLDER_PATH=%USER_PARAM%"
    call :locate_whisper_sdk "%USER_PARAM%"
    if not defined WHISPER_SDK_PATH (
        pause
        exit /b 1
    )
    goto :start_setup
)

echo [ERROR] Invalid parameter: %~1
echo.
goto :show_usage

:show_usage
echo Usage:
echo   Setup.bat "path\to\extracted\folder"   - Install from already-extracted folder
echo.
echo Download the SDK from:
echo   https://qpm.qualcomm.com/#/main/tools/details/VoiceAI_ASR
echo.
pause
exit /b 1

:: ============================================================================
:: Common Setup
:: ============================================================================

:start_setup
echo.
echo ============================================
echo    VoiceAI ASR SDK Setup
echo ============================================
echo.
echo [INFO] Target: %THIRDPARTY_DIR%
echo.

:: Create target directory if missing
if not exist "%THIRDPARTY_DIR%" mkdir "%THIRDPARTY_DIR%" 2>nul
if not exist "%THIRDPARTY_DIR%" (
    echo [ERROR] Failed to create target directory.
    goto :error
)


echo [INFO] Using provided folder: %USER_FOLDER_PATH%
echo.

echo.
echo [INFO] Installing SDK components...

:: ------------------------------------------------------------------
:: 1. Copy headers: whisper_sdk/include/npu/rpc/windows/*.h -> inc/
:: ------------------------------------------------------------------

set "SRC_INC=%WHISPER_SDK_PATH%\include\npu\rpc\windows"

if not exist "%SRC_INC%\*" (
    echo [ERROR] Headers not found in SDK.
    goto :error
)

if not exist "%THIRDPARTY_DIR%\inc" mkdir "%THIRDPARTY_DIR%\inc"
if not exist "%THIRDPARTY_DIR%\inc" (
    echo [ERROR] Failed to create headers directory.
    goto :error
)

xcopy "%SRC_INC%\*.h" "%THIRDPARTY_DIR%\inc\" /Y /Q >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to install headers.
    goto :error
)

echo [INFO] Headers installed.

:: ------------------------------------------------------------------
:: 2. Copy libs: whisper_sdk/libs/npu/rpc_libraries/{windows,android} -> lib/
:: ------------------------------------------------------------------

set "SRC_LIBS=%WHISPER_SDK_PATH%\libs\npu\rpc_libraries"

if not exist "%SRC_LIBS%\*" (
    echo [ERROR] Libraries not found in SDK.
    goto :error
)

if not exist "%THIRDPARTY_DIR%\lib" mkdir "%THIRDPARTY_DIR%\lib"
if not exist "%THIRDPARTY_DIR%\lib" (
    echo [ERROR] Failed to create libraries directory.
    goto :error
)

set "LIB_COUNT=0"

:: Copy windows libraries (only whisper_all_quantized)
if exist "%SRC_LIBS%\windows\whisper_all_quantized\*" (
    if not exist "%THIRDPARTY_DIR%\lib\windows\whisper_all_quantized" mkdir "%THIRDPARTY_DIR%\lib\windows\"
    xcopy "%SRC_LIBS%\windows\whisper_all_quantized\*" "%THIRDPARTY_DIR%\lib\windows\" /E /Y /Q >nul 2>&1
    if !ERRORLEVEL! equ 0 set /a LIB_COUNT+=1
)

:: Copy android libraries (only whisper_all_quantized)
if exist "%SRC_LIBS%\android\whisper_all_quantized\*" (
    if not exist "%THIRDPARTY_DIR%\lib\android\whisper_all_quantized" mkdir "%THIRDPARTY_DIR%\lib\android\"
    xcopy "%SRC_LIBS%\android\whisper_all_quantized\*" "%THIRDPARTY_DIR%\lib\android\" /E /Y /Q >nul 2>&1
    if !ERRORLEVEL! equ 0 set /a LIB_COUNT+=1
)

:: Copy assets
if not exist "%THIRDPARTY_DIR%\assets" mkdir "%THIRDPARTY_DIR%\assets"
xcopy "%SRC_LIBS%\assets\*" "%THIRDPARTY_DIR%\assets\" /E /Y /Q >nul 2>&1

if "%LIB_COUNT%"=="0" (
    echo [ERROR] No platform libraries found in SDK.
    goto :error
)

echo [INFO] Platform libraries installed (%LIB_COUNT% platform(s)).

:: ============================================================================
:: Verify Installation
:: ============================================================================

echo.
echo [INFO] Verifying installation...

set "VERIFY_OK=1"

if not exist "%THIRDPARTY_DIR%\inc" (
    echo [FAIL] Headers directory missing.
    set "VERIFY_OK=0"
)
if not exist "%THIRDPARTY_DIR%\lib" (
    echo [FAIL] Libraries directory missing.
    set "VERIFY_OK=0"
)

set "INC_COUNT=0"
for %%F in ("%THIRDPARTY_DIR%\inc\*.h") do set /a INC_COUNT+=1
if "%INC_COUNT%"=="0" (
    echo [FAIL] No header files installed.
    set "VERIFY_OK=0"
)

set "LIB_DIR_COUNT=0"
for /d %%D in ("%THIRDPARTY_DIR%\lib\*") do set /a LIB_DIR_COUNT+=1
if "%LIB_DIR_COUNT%"=="0" (
    echo [FAIL] No platform libraries installed.
    set "VERIFY_OK=0"
)

if "%VERIFY_OK%"=="0" goto :error

echo.
echo ============================================
echo    Setup Complete
echo ============================================
echo.
echo    Location: %THIRDPARTY_DIR%
echo.
echo    Installed:
echo      - inc/ (%INC_COUNT% headers)
echo      - lib/
for /d %%D in ("%THIRDPARTY_DIR%\lib\*") do echo        - %%~nxD
echo.
goto :end

:: ############################################################################
:: SUBROUTINES
:: ############################################################################

:: ============================================================================
:: Subroutine: Locate whisper_sdk inside SDK base path
:: Sets WHISPER_SDK_PATH if found, clears it if not.
:: Expected structure: {base}/{version}/whisper_sdk/
:: ============================================================================

:locate_whisper_sdk
set "SEARCH_BASE=%~1"
set "WHISPER_SDK_PATH="

if exist "%SEARCH_BASE%\whisper_sdk\libs\npu\rpc_libraries\android\whisper_all_quantized\whisper-sdk.jar" (
    set "WHISPER_SDK_PATH=%SEARCH_BASE%\whisper_sdk"
    echo [INFO] SDK located successfully.
) else (
    echo [ERROR] Invalid SDK folder: %SEARCH_BASE%
    echo         Expected: whisper_sdk\libs\npu\rpc_libraries\android\whisper_all_quantized\whisper-sdk.jar
)

goto :eof

:: ============================================================================
:: Error and End Handlers
:: ============================================================================

:error
echo.
echo ============================================
echo    Setup Failed
echo ============================================
echo.
if exist "%THIRDPARTY_DIR%\inc" rmdir /s /q "%THIRDPARTY_DIR%\inc" 2>nul
if exist "%THIRDPARTY_DIR%\lib" rmdir /s /q "%THIRDPARTY_DIR%\lib" 2>nul
pause
exit /b 1

:end
pause
exit /b 0
