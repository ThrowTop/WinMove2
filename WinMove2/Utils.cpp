#include "Utils.h"

namespace Utils {
    COLORREF GetAccentColor() {
        HKEY hKey;
        DWORD color = 0;
        DWORD size = sizeof(color);

        if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueEx(hKey, L"AccentColor", nullptr, nullptr, (LPBYTE)&color, &size);
            RegCloseKey(hKey);
        }

        return RGB(GetRValue(color), GetGValue(color), GetBValue(color));
    }

    HWND FindTopLevelWindow(HWND hwnd) {
        while (hwnd != NULL && GetParent(hwnd) != NULL) {
            hwnd = GetParent(hwnd);
        }
        return hwnd;
    }

    HWND GetWindow(const POINT p) {
        HWND hwnd = WindowFromPoint(p);
        if (hwnd) {
            hwnd = Utils::FindTopLevelWindow(hwnd);
            //return hwnd;
            if (hwnd && IsWindow(hwnd) && IsWindowVisible(hwnd) && (GetWindowLongW(hwnd, GWL_STYLE) & WS_CAPTION)) {
                return hwnd;
            }
        }
        return NULL; // Return NULL if no valid window is found
    }

}
