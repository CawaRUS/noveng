@echo off
setlocal enabledelayedexpansion

:: Пути к папкам
set SRC_DIR=cpp
set HDR_DIR=hpp
set OBJ_DIR=build
set LIBS=-lole32 -lwinmm

echo [1/4] Cleaning old files...
if exist %OBJ_DIR%\*.o del %OBJ_DIR%\*.o
if exist game.exe del game.exe
if not exist %OBJ_DIR% mkdir %OBJ_DIR%

echo [2/4] Compiling Modules...
set OBJECTS=

:: Цикл по всем .cpp файлам в папке SRC_DIR
for %%f in (%SRC_DIR%\*.cpp) do (
    echo Compiling: %%~nxf...
    g++ -c %%f -o %OBJ_DIR%\%%~nf.o -I %HDR_DIR% -std=c++17 -Wall
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to compile %%f
        pause
        exit /b
    )
    :: Собираем список объектных файлов для линковки
    set OBJECTS=!OBJECTS! %OBJ_DIR%\%%~nf.o
)

echo [3/4] Linking...
:: Линкуем всё, что собрали в цикле
g++ !OBJECTS! -o game.exe %LIBS% -static -static-libgcc -static-libstdc++

if %errorlevel% equ 0 (
    echo [4/4] Success! Running game...
    echo.
    game.exe
) else (
    echo [ERROR] Linking failed.
    pause
)