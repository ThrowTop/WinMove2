#include "Overlay.h"
#include "Utils.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001

Overlay::Overlay() {
	overlayWindow = CreateOverlayWindow();
}

void Overlay::StartDrag(HWND hwnd, POINT startPoint) {
	targetWindow = hwnd;
	SetForegroundWindow(targetWindow);

	RECT windowRect{};
	GetWindowRect(targetWindow, &windowRect);

	// logic to move the anchor point when grabbing a window that is maximized so the mouse never goes outside the window
	if (IsZoomed(targetWindow)) {
		anchorXPercent = static_cast<double>(startPoint.x - windowRect.left) / (windowRect.right - windowRect.left);
		anchorYPercent = static_cast<double>(startPoint.y - windowRect.top) / (windowRect.bottom - windowRect.top);

		ShowWindow(targetWindow, SW_RESTORE);
		GetWindowRect(targetWindow, &windowRect);
		lastMousePos.x = windowRect.left + static_cast<int>((windowRect.right - windowRect.left) * anchorXPercent);
		lastMousePos.y = windowRect.top + static_cast<int>((windowRect.bottom - windowRect.top) * anchorYPercent);

		POINT currentMousePos = startPoint;
		int offsetX = currentMousePos.x - lastMousePos.x;
		int offsetY = currentMousePos.y - lastMousePos.y;

		OffsetRect(&windowRect, offsetX, offsetY);
		outlineRect = windowRect;
		DrawOverlay(outlineRect);

		lastMousePos = currentMousePos;
	}
	else {
		lastMousePos = startPoint;
		outlineRect = windowRect;
		DrawOverlay(outlineRect);
	}

	moving = true;
	SetCapture(targetWindow);
}

void Overlay::StartResize(HWND hwnd, POINT startPoint) {
	targetWindow = hwnd;
	SetForegroundWindow(targetWindow);

	lastMousePos = startPoint;
	GetWindowRect(targetWindow, &outlineRect);
	DrawOverlay(outlineRect);
	resizing = true;
	SetCapture(targetWindow);
}

void Overlay::StopDrag() {
	if (moving) {
		HideOverlay();
		MoveWindow(targetWindow, outlineRect.left, outlineRect.top, outlineRect.right - outlineRect.left, outlineRect.bottom - outlineRect.top, TRUE);
		RedrawWindow(targetWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
		moving = false;
		ReleaseCapture();
		targetWindow = NULL;
	}
}

void Overlay::StopResize() {
	if (resizing) {
		HideOverlay();
		MoveWindow(targetWindow, outlineRect.left, outlineRect.top, outlineRect.right - outlineRect.left, outlineRect.bottom - outlineRect.top, TRUE);
		RedrawWindow(targetWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
		resizing = false;
		ReleaseCapture();
		targetWindow = NULL;
	}
}

void Overlay::PerformDrag(POINT currentPos) {
	if (targetWindow != NULL) {
		int dx = currentPos.x - lastMousePos.x;
		int dy = currentPos.y - lastMousePos.y;
		OffsetRect(&outlineRect, dx, dy);
		DrawOverlay(outlineRect);
		lastMousePos = currentPos;
	}
}

void Overlay::PerformResize(POINT currentPos) {
	if (targetWindow != NULL) {
		MINMAXINFO minMaxInfo = {};
		SendMessage(targetWindow, WM_GETMINMAXINFO, 0, (LPARAM)&minMaxInfo);

		int dx = currentPos.x - lastMousePos.x;
		int dy = currentPos.y - lastMousePos.y;

		int newWidth = outlineRect.right - outlineRect.left + dx;
		int newHeight = outlineRect.bottom - outlineRect.top + dy;

		bool canResizeWidth = (newWidth >= minMaxInfo.ptMinTrackSize.x);
		bool canResizeHeight = (newHeight >= minMaxInfo.ptMinTrackSize.y);

		if (canResizeWidth) {
			outlineRect.right += dx;
			lastMousePos.x = currentPos.x;
		}
		if (canResizeHeight) {
			outlineRect.bottom += dy;
			lastMousePos.y = currentPos.y;
		}

		DrawOverlay(outlineRect);
	}
}

void Overlay::DrawOverlay(RECT rect) {
	if (!overlayWindow) {
		overlayWindow = CreateOverlayWindow();
	}

	SetWindowPos(overlayWindow, HWND_TOPMOST, rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void Overlay::HideOverlay() {
	if (overlayWindow) {
		ShowWindow(overlayWindow, SW_HIDE);
	}
}

bool Overlay::IsMoving() {
	return moving;
}

bool Overlay::IsResizing() {
	return resizing;
}

HWND Overlay::CreateOverlayWindow() {
	accentColor = Utils::GetAccentColor();

	WNDCLASS wndClass = {};
	wndClass.lpfnWndProc = OverlayWindowProc;
	wndClass.hInstance = GetModuleHandle(NULL);
	wndClass.lpszClassName = L"OverlayWindowClass";
	RegisterClass(&wndClass);

	HWND hwnd = CreateWindowExW(
		WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
		L"OverlayWindowClass", L"OverlayWindow", WS_POPUP,
		0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

	if (hwnd) {
		SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		NOTIFYICONDATA nid = {};
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.hWnd = hwnd;
		nid.uID = 1;
		nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		nid.uCallbackMessage = WM_TRAYICON;
		nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wcscpy_s(nid.szTip, L"Window Mover");
		Shell_NotifyIconW(NIM_ADD, &nid);
	}

	return hwnd;
}

LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	Overlay* pOverlay = reinterpret_cast<Overlay*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (uMsg) {
		case WM_TRAYICON:
			if (lParam == WM_RBUTTONUP) {
				HMENU hMenu = CreatePopupMenu();
				AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

				POINT cursorPos;
				GetCursorPos(&cursorPos);
				TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursorPos.x, cursorPos.y, 0, hwnd, NULL);
				DestroyMenu(hMenu);
			}
			break;
		case WM_COMMAND:
			if (LOWORD(wParam) == ID_TRAY_EXIT) {
				PostQuitMessage(0);
			}
			break;
		case WM_DESTROY:
		{
			NOTIFYICONDATA nid = {};
			nid.cbSize = sizeof(NOTIFYICONDATA);
			nid.hWnd = hwnd;
			nid.uID = 1;
			Shell_NotifyIcon(NIM_DELETE, &nid);
			break;
		}
		case WM_PAINT:
			if (pOverlay) {
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);

				HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
				FillRect(hdc, &ps.rcPaint, hBrush);
				DeleteObject(hBrush);

				HPEN hPen = CreatePen(PS_SOLID, 2, pOverlay->getColor());
				HGDIOBJ oldPen = SelectObject(hdc, hPen);
				HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

				//int cornerRadius = 12;
				//RoundRect(hdc, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom, cornerRadius, cornerRadius);

				Rectangle(hdc, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom);

				SelectObject(hdc, oldPen);
				SelectObject(hdc, oldBrush);
				DeleteObject(hPen);

				EndPaint(hwnd, &ps);
			}
			break;
		case WM_ERASEBKGND:
			return 1;
		case WM_DWMCOLORIZATIONCOLORCHANGED:
			if (pOverlay) {
				pOverlay->SetColor(Utils::GetAccentColor());
				InvalidateRect(hwnd, NULL, TRUE);
			}
			break;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}