@echo off
setlocal enabledelayedexpansion

if exist config.cfg (
    for /f "delims=" %%x in (config.cfg) do (
        set "line=%%x"
        if not "!line:~0,1!"=="#" (
            set "%%x"
        )
    )
)

if "%TARGET_NAME%"=="" set TARGET_NAME=game.exe
if "%DIR_RES%"=="" set DIR_RES=res
if "%DIR_SCENARIO%"=="" set DIR_SCENARIO=scenario
if "%DIR_MUSIC%"=="" set DIR_MUSIC=music
if "%DIR_SFX%"=="" set DIR_SFX=sfx
if "%DIR_SAVE%"=="" set DIR_SAVE=save
if "%USE_CUSTOM_ABOUT%"=="" set USE_CUSTOM_ABOUT=false
if "%DECRYPT%"=="" set DECRYPT=false
if "%ASSET_KEY%"=="" set ASSET_KEY=keyok
if "%DEFAULT_LANG%"=="" set DEFAULT_LANG=ru

if "%USE_CUSTOM_ABOUT%"=="true" (set USE_CUSTOM_ABOUT_VAL=1) else (set USE_CUSTOM_ABOUT_VAL=0)
if "%DECRYPT%"=="true" (set USE_DECRYPT_VAL=1) else (set USE_DECRYPT_VAL=0)

set SRC_DIR=cpp
set HDR_DIR=hpp
set OBJ_DIR=build
set LIBS=-lole32 -lwinmm

set DEFINES=-DAPP_VERSION=\"%APP_VERSION%\" -DAPP_NAME=\"%APP_NAME%\" -DDEFAULT_LANG=\"%DEFAULT_LANG%\" ^
-DUSE_CUSTOM_ABOUT=%USE_CUSTOM_ABOUT_VAL% -DUSE_DECRYPT=%USE_DECRYPT_VAL% -DASSET_KEY=\"%ASSET_KEY%\" ^
-DDIR_RES=\"%DIR_RES%\" -DDIR_SCENARIO=\"%DIR_SCENARIO%\" -DDIR_MUSIC=\"%DIR_MUSIC%\" -DDIR_SFX=\"%DIR_SFX%\" -DDIR_SAVE=\"%DIR_SAVE%\"

echo [1/4] Cleaning old files...
if exist %TARGET_NAME% del %TARGET_NAME%
if exist %OBJ_DIR% rd /s /q %OBJ_DIR%
mkdir %OBJ_DIR%
mkdir %OBJ_DIR%\cmds

echo [2/4] Compiling Modules for %APP_NAME%...
set OBJECTS=

for /r %SRC_DIR% %%f in (*.cpp) do (
    set "abspath=%%f"
    set "relpath=!abspath:%CD%\%SRC_DIR%\=!"
    set "objpath=%OBJ_DIR%\!relpath:.cpp=.o!"
    
    echo Compiling: !relpath!...
    
    for %%d in ("!objpath!") do if not exist "%%~dpd" mkdir "%%~dpd"

    g++ -c "%%f" -o "!objpath!" -I %HDR_DIR% -std=c++17 -Wall %DEFINES%
    
    if !errorlevel! neq 0 (
        color 4
        echo [ERROR] Failed to compile %%f
        pause
        exit /b
    )
    set OBJECTS=!OBJECTS! "!objpath!"
)

echo [3/4] Linking %TARGET_NAME%...
g++ !OBJECTS! -o %TARGET_NAME% %LIBS% -static -static-libgcc -static-libstdc++

if %errorlevel% equ 0 (
    color 2
    echo [4/4] Success!
    color 7
    echo Launching...
    %TARGET_NAME%
) else (
    color 4
    echo [ERROR] Linking failed.
    pause
)