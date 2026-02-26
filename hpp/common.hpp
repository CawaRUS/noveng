#pragma once
#include <iostream>
#include <windows.h>

// ANSI Escape Codes для Windows 10+
#define CLR_RESET   "\033[0m"
#define CLR_NAME    "\033[1;36m" // Яркий циан
#define CLR_TEXT    "\033[0;37m" // Белый
#define CLR_SYSTEM  "\033[1;30m" // Серый
#define CLR_CHAPTER "\033[1;33m" // Желтый

inline void setupConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // Включаем поддержку ANSI в Windows консоли
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

inline void clearScreen() {
    system("cls");
}