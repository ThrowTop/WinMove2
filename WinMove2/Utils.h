#pragma once
#ifndef UTILS_H
#define UTILS_H

#include <windows.h>

namespace Utils {
    COLORREF GetAccentColor();
    HWND FindTopLevelWindow(HWND hwnd);
    HWND GetWindow(const POINT p); 
}

#endif // UTILS_H
