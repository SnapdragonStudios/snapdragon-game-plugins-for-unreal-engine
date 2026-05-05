:: Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
:: SPDX-License-Identifier: BSD-3-Clause

@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: QAIRT SDK Setup Script
:: Downloads and extracts the Qualcomm AI Runtime SDK for Unreal Engine plugin
::
:: Location: {plugin-root}/SetupQAIRT.bat
:: Target:   {plugin-root}/Source/ThirdParty/qairt/
:: ============================================================================

set SDK_VERSION=2.45.0.260326

:: Minimum Hexagon DSP version to include (versions at or below this are skipped)
set MIN_HEXAGON_VER=66

:: Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"
set "THIRDPARTY_DIR=%SCRIPT_DIR%Source\ThirdParty\qairt"

:: Use a SHORT temp path to avoid Windows MAX_PATH (260 char) limit
set "TEMP_DIR=%TEMP%\QAIRT_SDK"
set "TEMP_ZIP=%TEMP_DIR%\sdk.zip"

:: Remove any trailing spaces or carriage returns from version
for /f "tokens=* delims= " %%a in ("%SDK_VERSION%") do set "SDK_VERSION=%%a"

:: ============================================================================
:: Parse command-line parameters
:: ============================================================================

if "%~1"=="" goto :no_params

set "USER_PARAM=%~1"

if /i "%USER_PARAM%"=="/h" goto :show_menu
if /i "%USER_PARAM%"=="/help" goto :show_menu
if /i "%USER_PARAM%"=="/?" goto :show_menu

:: Check if it's a zip file path
if /i "%USER_PARAM:~-4%"==".zip" (
    if not exist "%USER_PARAM%" (
        echo [ERROR] ZIP file not found: %USER_PARAM%
        pause
        exit /b 1
    )
    set "SETUP_MODE=2"
    set "USER_ZIP_PATH=%USER_PARAM%"
    goto :start_setup
)

:: Check if it's a directory path
if exist "%USER_PARAM%\*" (
    set "SETUP_MODE=3"
    set "USER_FOLDER_PATH=%USER_PARAM%"
    goto :start_setup
)

echo [ERROR] Invalid parameter: %~1
echo.
echo Usage:
echo   SetupQAIRT.bat                            - Download and install (default)
echo   SetupQAIRT.bat /h                         - Show menu with all options
echo   SetupQAIRT.bat "path\to\sdk.zip"          - Use existing ZIP
echo   SetupQAIRT.bat "path\to\extracted\folder" - Use extracted folder
pause
exit /b 1

:no_params
set "SETUP_MODE=1"
goto :start_setup

:: ============================================================================
:: Interactive Menu (only via /h)
:: ============================================================================

:show_menu
cls
echo.
echo ============================================
echo    QAIRT SDK Setup Script
echo ============================================
echo.
echo Choose an option:
echo   1. Download and install SDK (recommended)
echo   2. Use existing ZIP file
echo   3. Use already-extracted folder
echo.
set /p "SETUP_MODE=Enter your choice (1-3): "

if "%SETUP_MODE%"=="1" goto :start_setup
if "%SETUP_MODE%"=="2" goto :prompt_zip
if "%SETUP_MODE%"=="3" goto :prompt_folder

echo [ERROR] Invalid choice. Please enter 1, 2, or 3.
pause
goto :show_menu

:prompt_zip
set /p "USER_ZIP_PATH=Enter the full path to the ZIP file: "
set "USER_ZIP_PATH=%USER_ZIP_PATH:"=%"
if not exist "%USER_ZIP_PATH%" (
    echo [ERROR] ZIP file not found: %USER_ZIP_PATH%
    pause
    goto :show_menu
)
if /i not "%USER_ZIP_PATH:~-4%"==".zip" (
    echo [ERROR] File must have .zip extension
    pause
    goto :show_menu
)
goto :start_setup

:prompt_folder
set /p "USER_FOLDER_PATH=Enter the full path to the extracted folder: "
set "USER_FOLDER_PATH=%USER_FOLDER_PATH:"=%"
if not exist "%USER_FOLDER_PATH%\*" (
    echo [ERROR] Folder not found: %USER_FOLDER_PATH%
    pause
    goto :show_menu
)
goto :start_setup

:: ============================================================================
:: Common Setup
:: ============================================================================

:start_setup
echo.
echo ============================================
echo    QAIRT SDK Setup - v%SDK_VERSION%
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

:: Check if SDK already installed
set "SDK_EXISTS=0"
if exist "%THIRDPARTY_DIR%\inc" if exist "%THIRDPARTY_DIR%\lib" set "SDK_EXISTS=1"

if "%SDK_EXISTS%"=="1" (
    echo [INFO] Removing existing installation...
    if exist "%THIRDPARTY_DIR%\inc" rmdir /s /q "%THIRDPARTY_DIR%\inc" 2>nul
    if exist "%THIRDPARTY_DIR%\lib" rmdir /s /q "%THIRDPARTY_DIR%\lib" 2>nul
    echo.
)

:: ============================================================================
:: Branch by mode
:: Modes 2 and 3 jump ahead; mode 1 falls through linearly into extraction.
:: ============================================================================

if "%SETUP_MODE%"=="2" goto :mode_zip
if "%SETUP_MODE%"=="3" goto :mode_folder

:: ============================================================================
:: MODE 1: Download and Install (falls through to extraction)
:: ============================================================================

set "DOWNLOAD_URL=https://softwarecenter.qualcomm.com/api/download/software/sdks/Qualcomm_AI_Runtime_Community/All/%SDK_VERSION%/v%SDK_VERSION%.zip"

:: Create temp directory
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%" 2>nul
mkdir "%TEMP_DIR%"
if not exist "%TEMP_DIR%" (
    echo [ERROR] Failed to create temp directory.
    goto :error
)

echo [INFO] Downloading SDK (~1.4 GB)...
echo.

powershell -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; $job = Start-Job -ScriptBlock { param($u,$f) [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; (New-Object Net.WebClient).DownloadFile($u,$f) } -ArgumentList '%DOWNLOAD_URL%','%TEMP_ZIP%'; $d=0; Write-Host '[INFO] Downloading' -NoNewline; while($job.State -eq 'Running'){ Write-Host '.' -NoNewline; $d++; if($d%%60 -eq 0){Write-Host ''; Write-Host '       Downloading' -NoNewline}; Start-Sleep -Milliseconds 500 }; Write-Host ''; Receive-Job $job -ErrorAction Stop; Remove-Job $job"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Download failed. Check internet connection and URL accessibility.
    goto :error
)

if not exist "%TEMP_ZIP%" (
    echo [ERROR] Downloaded file not found.
    goto :error
)

for %%A in ("%TEMP_ZIP%") do set "FILE_SIZE=%%~zA"
if not defined FILE_SIZE (
    echo [ERROR] Cannot read downloaded file.
    goto :error
)
if "%FILE_SIZE%"=="0" (
    echo [ERROR] Downloaded file is empty. URL may be invalid.
    goto :error
)

set /a FILE_SIZE_MB=%FILE_SIZE% / 1048576
echo [INFO] Download complete (%FILE_SIZE_MB% MB).

:: ---- Mode 1 falls through directly into extraction below ----

:: ============================================================================
:: Extract ZIP (reached by mode 1 fallthrough, mode 2 goto)
:: ============================================================================

:do_extract
echo [INFO] Extracting SDK...

mkdir "%TEMP_DIR%\ex" 2>nul
tar -xf "%TEMP_ZIP%" -C "%TEMP_DIR%\ex"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Extraction failed. ZIP may be corrupted.
    goto :error
)

if not exist "%TEMP_DIR%\ex" (
    echo [ERROR] Extraction produced no output.
    goto :error
)

echo [INFO] Extraction complete.

:: ============================================================================
:: Locate SDK inside extracted files
:: ============================================================================

echo [INFO] Locating SDK structure...

set "SDK_BASE_PATH="
set "EXTRACTED_DIR=%TEMP_DIR%\ex"

:: Pattern 1: qairt/{version}/
for /d %%Q in ("%EXTRACTED_DIR%\qairt\*") do (
    if exist "%%Q\lib" if exist "%%Q\include" (
        set "SDK_BASE_PATH=%%Q"
        goto :sdk_found
    )
)

:: Pattern 2: Recursive fallback
for /r "%EXTRACTED_DIR%" %%D in (.) do (
    if exist "%%D\lib" if exist "%%D\include" (
        set "SDK_BASE_PATH=%%~fD"
        goto :sdk_found
    )
)

echo [ERROR] SDK structure not found. Expected 'lib' and 'include' directories.
goto :error

:sdk_found
echo [INFO] SDK located successfully.
goto :copy_files

:: ============================================================================
:: MODE 2: Use Existing ZIP (jumps to :do_extract)
:: ============================================================================

:mode_zip
echo [INFO] Using provided ZIP: %USER_ZIP_PATH%
echo.

if not exist "%USER_ZIP_PATH%" (
    echo [ERROR] ZIP file not found.
    goto :error
)

for %%A in ("%USER_ZIP_PATH%") do set "FILE_SIZE=%%~zA"
if "%FILE_SIZE%"=="0" (
    echo [ERROR] ZIP file is empty.
    goto :error
)

if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%" 2>nul
mkdir "%TEMP_DIR%"
if not exist "%TEMP_DIR%" (
    echo [ERROR] Failed to create temp directory.
    goto :error
)

echo [INFO] Preparing ZIP for extraction...
copy "%USER_ZIP_PATH%" "%TEMP_ZIP%" >nul 2>&1
if not exist "%TEMP_ZIP%" (
    echo [ERROR] Failed to stage ZIP file.
    goto :error
)

goto :do_extract

:: ============================================================================
:: MODE 3: Use Already-Extracted Folder (jumps to :copy_files)
:: ============================================================================

:mode_folder
echo [INFO] Using provided folder: %USER_FOLDER_PATH%
echo.

if not exist "%USER_FOLDER_PATH%\*" (
    echo [ERROR] Folder not found.
    goto :error
)

set "SDK_BASE_PATH=%USER_FOLDER_PATH%"

echo [INFO] Locating SDK structure...

:: Check root
if exist "%SDK_BASE_PATH%\lib" if exist "%SDK_BASE_PATH%\include" goto :mode_folder_found

:: Check qairt subfolder
for /d %%Q in ("%SDK_BASE_PATH%\qairt\*") do (
    if exist "%%Q\lib" if exist "%%Q\include" (
        set "SDK_BASE_PATH=%%Q"
        goto :mode_folder_found
    )
)

:: Recursive fallback
for /r "%SDK_BASE_PATH%" %%D in (.) do (
    if exist "%%D\lib" if exist "%%D\include" (
        set "SDK_BASE_PATH=%%~fD"
        goto :mode_folder_found
    )
)

echo [ERROR] SDK structure not found. Expected 'lib' and 'include' directories.
goto :error

:mode_folder_found
echo [INFO] SDK located successfully.
goto :copy_files

:: ============================================================================
:: Copy Files to Target Directory
:: ============================================================================

:copy_files

if not defined SDK_BASE_PATH (
    echo [ERROR] Internal error: SDK path not resolved.
    goto :error
)

if not exist "%THIRDPARTY_DIR%\inc" mkdir "%THIRDPARTY_DIR%\inc"
if not exist "%THIRDPARTY_DIR%\lib" mkdir "%THIRDPARTY_DIR%\lib"

:: ------------------------------------------------------------------
:: Copy platform libraries
:: ------------------------------------------------------------------

set "PLATFORMS=x86_64-windows-msvc arm64x-windows-msvc aarch64-windows-msvc aarch64-android android"

echo.
echo [INFO] Installing platform libraries...

set "PLATFORM_COUNT=0"
set "PLATFORM_LIST="
for %%P in (%PLATFORMS%) do (
    set "SRC=%SDK_BASE_PATH%\lib\%%P"
    set "DST=%THIRDPARTY_DIR%\lib\%%P"

    if exist "!SRC!" (
        if not exist "!DST!" mkdir "!DST!"
        xcopy "!SRC!\*" "!DST!\" /E /Y /Q >nul 2>&1
        if !ERRORLEVEL! equ 0 (
            set /a PLATFORM_COUNT+=1
            set "PLATFORM_LIST=!PLATFORM_LIST! %%P"
        )
    )
)

if "%PLATFORM_COUNT%"=="0" (
    echo [ERROR] No platform libraries found in SDK.
    goto :error
)

echo [INFO] Installed %PLATFORM_COUNT% platform(s).

:: ------------------------------------------------------------------
:: Copy Hexagon DSP libraries to Windows ARM platforms
:: ------------------------------------------------------------------

echo [INFO] Installing Hexagon DSP libraries...

set "HEXAGON_COPY_COUNT=0"
set "HEXAGON_FOLDER_COUNT=0"

for /d %%H in ("%SDK_BASE_PATH%\lib\hexagon-*") do (
    set /a HEXAGON_FOLDER_COUNT+=1
    call :process_hexagon_folder "%%H"
)

if not "%HEXAGON_FOLDER_COUNT%"=="0" echo [INFO] Hexagon DSP libraries installed (!HEXAGON_COPY_COUNT! files).

:: ------------------------------------------------------------------
:: Copy header files
:: ------------------------------------------------------------------

echo [INFO] Installing headers...

if exist "%SDK_BASE_PATH%\include\*" (
    xcopy "%SDK_BASE_PATH%\include\*" "%THIRDPARTY_DIR%\inc\" /E /Y /Q >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        echo [WARNING] Some headers may not have copied correctly.
    )
) else (
    echo [ERROR] No include directory found in SDK.
    goto :error
)

echo [INFO] Headers installed.

:: ============================================================================
:: Cleanup temp files
:: ============================================================================

if "%SETUP_MODE%"=="1" goto :do_cleanup
if "%SETUP_MODE%"=="2" goto :do_cleanup
goto :verify

:do_cleanup
echo [INFO] Cleaning up temporary files...
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%" 2>nul

:: ============================================================================
:: Verify Installation
:: ============================================================================

:verify
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

set "LIB_COUNT=0"
for /d %%D in ("%THIRDPARTY_DIR%\lib\*") do set /a LIB_COUNT+=1
if "%LIB_COUNT%"=="0" (
    echo [FAIL] No platform libraries installed.
    set "VERIFY_OK=0"
)

set "INC_COUNT=0"
for %%F in ("%THIRDPARTY_DIR%\inc\*") do set /a INC_COUNT+=1
for /d %%D in ("%THIRDPARTY_DIR%\inc\*") do set /a INC_COUNT+=1
if "%INC_COUNT%"=="0" (
    echo [FAIL] Headers directory is empty.
    set "VERIFY_OK=0"
)

if "%VERIFY_OK%"=="0" goto :error

echo.
echo ============================================
echo    Setup Complete
echo ============================================
echo.
echo    Location : %THIRDPARTY_DIR%
echo    Platforms:%PLATFORM_LIST%
echo.
for /d %%D in ("%THIRDPARTY_DIR%\lib\*") do echo      - %%~nxD
echo.
goto :end

:: ############################################################################
:: SUBROUTINES
:: ############################################################################

:: ============================================================================
:: Subroutine: Process one hexagon-vXX folder
:: ============================================================================

:process_hexagon_folder
set "HEX_PATH=%~1"
set "HEX_NAME=%~nx1"
set "HEX_VER="

:: Extract version number: hexagon-v73 -> 73
set "HEX_SUFFIX=!HEX_NAME:hexagon-v=!"

:: If nothing was stripped, skip (unexpected naming)
if "!HEX_SUFFIX!"=="!HEX_NAME!" goto :eof

set "HEX_VER=!HEX_SUFFIX!"

:: Validate purely numeric
for /f "delims=0123456789" %%X in ("!HEX_VER!") do goto :eof

:: Skip versions at or below minimum
if !HEX_VER! LEQ %MIN_HEXAGON_VER% goto :eof

:: Need unsigned subfolder
set "UNSIGNED=!HEX_PATH!\unsigned"
if not exist "!UNSIGNED!\*" goto :eof

:: Target files for this version
set "SO_FILES=libSnpeHtpV!HEX_VER!Skel.so libQnnHtpV!HEX_VER!Skel.so libQnnHtpV!HEX_VER!.so"
set "CAT_FILES=libsnpehtpv!HEX_VER!.cat libqnnhtpv!HEX_VER!.cat"

:: Check at least one exists
set "ANY_FOUND=0"
for %%F in (!SO_FILES! !CAT_FILES!) do (
    if exist "!UNSIGNED!\%%F" set "ANY_FOUND=1"
)
if "!ANY_FOUND!"=="0" goto :eof

:: Copy to Windows ARM targets
set "HEX_TARGETS=aarch64-windows-msvc arm64x-windows-msvc aarch64-android"

for %%T in (!HEX_TARGETS!) do (
    set "TDIR=%THIRDPARTY_DIR%\lib\%%T"
    if exist "!TDIR!\*" (
        for %%F in (!SO_FILES!) do (
            if exist "!UNSIGNED!\%%F" (
                copy "!UNSIGNED!\%%F" "!TDIR!\" >nul 2>&1
                if !ERRORLEVEL! equ 0 set /a HEXAGON_COPY_COUNT+=1
            )
        )
        for %%F in (!CAT_FILES!) do (
            if exist "!UNSIGNED!\%%F" (
                copy "!UNSIGNED!\%%F" "!TDIR!\" >nul 2>&1
                if !ERRORLEVEL! equ 0 set /a HEXAGON_COPY_COUNT+=1
            )
        )
    )
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
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%" 2>nul
if exist "%THIRDPARTY_DIR%\inc" rmdir /s /q "%THIRDPARTY_DIR%\inc" 2>nul
if exist "%THIRDPARTY_DIR%\lib" rmdir /s /q "%THIRDPARTY_DIR%\lib" 2>nul
pause
exit /b 1

:end
pause
exit /b 0