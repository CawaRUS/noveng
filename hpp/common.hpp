#pragma once
#include <iostream>
#include <windows.h>
#include <string>
#include <cctype>

#define CLR_RESET   "\033[0m"
#define CLR_NAME    "\033[1;36m" // Яркий циан
#define CLR_TEXT    "\033[0;37m" // Белый
#define CLR_SYSTEM  "\033[1;30m" // Серый
#define CLR_CHAPTER "\033[1;33m" // Желтый

inline bool isValidIdentifier(const std::string& s) {
    if (s.empty()) return false;
    if (std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

inline void setupConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

inline void clearScreen() {
    std::cout << "\033[2J\033[H" << std::flush;
}