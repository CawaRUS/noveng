@echo off
setlocal enabledelayedexpansion


if exist config.cfg (
    for /f "delims=" %%x in (config.cfg) do (set "%%x")
)

if "%TARGET_NAME%"=="" set TARGET_NAME=game.exe
if "%DIR_RES%"=="" set DIR_RES=res
if "%DIR_SCENARIO%"=="" set DIR_SCENARIO=scenario
if "%DIR_MUSIC%"=="" set DIR_MUSIC=music
if "%DIR_SFX%"=="" set DIR_SFX=sfx
if "%DIR_SAVE%"=="" set DIR_SAVE=save

set SRC_DIR=cpp
set HDR_DIR=hpp
set OBJ_DIR=build
set LIBS=-lole32 -lwinmm

set DEFINES=-DAPP_VERSION=\"%APP_VERSION%\" -DAPP_NAME=\"%APP_NAME%\" ^
-DDIR_RES=\"%DIR_RES%\" -DDIR_SCENARIO=\"%DIR_SCENARIO%\" ^
-DDIR_MUSIC=\"%DIR_MUSIC%\" -DDIR_SFX=\"%DIR_SFX%\" -DDIR_SAVE=\"%DIR_SAVE%\"
color 4
echo This build is not up to date, use build.bat instead
color 7
echo [1/4] Cleaning old files...
if exist %OBJ_DIR%\*.o del %OBJ_DIR%\*.o
if exist %TARGET_NAME% del %TARGET_NAME%
if not exist %OBJ_DIR% mkdir %OBJ_DIR%

echo [2/4] Compiling Modules for %APP_NAME%...
set OBJECTS=

for %%f in (%SRC_DIR%\*.cpp) do (
    echo Compiling: %%~nxf...
    g++ -c %%f -o %OBJ_DIR%\%%~nf.o -I %HDR_DIR% -std=c++17 -Wall %DEFINES%
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to compile %%f
        pause
        exit /b
    )
    set OBJECTS=!OBJECTS! %OBJ_DIR%\%%~nf.o
)

echo [3/4] Linking %TARGET_NAME%...
g++ !OBJECTS! -o %TARGET_NAME% %LIBS% -static -static-libgcc -static-libstdc++

if %errorlevel% equ 0 (
    color 2
    echo [4/4] Success!
    color 7
    %TARGET_NAME%
) else (
    pause
)