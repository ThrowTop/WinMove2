#pragma once
#ifndef OVERLAY_H
#define OVERLAY_H

#include <windows.h>

LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

class Overlay {
private:
	HHOOK mouseHook;
	HWND targetWindow = NULL;

	POINT lastMousePos;
	bool altHeld = false;
	bool moving = false;
	bool resizing = false;
	double anchorXPercent = 0.0;
	double anchorYPercent = 0.0;
	RECT outlineRect;

	COLORREF accentColor;

	HWND CreateOverlayWindow();
public:
	Overlay();

	HWND overlayWindow = NULL;

	inline COLORREF getColor() {
		return accentColor;
	}

	inline void SetColor(COLORREF clr) {
		accentColor = clr;
	}

	void StartDrag(HWND hwnd, POINT startPoint);
	void StartResize(HWND hwnd, POINT startPoint);
	void StopDrag();
	void StopResize();
	void PerformDrag(POINT currentPos);
	void PerformResize(POINT currentPos);
	void DrawOverlay(RECT rect);
	void HideOverlay();
	bool IsMoving();
	bool IsResizing();
};

#endif; // OVERLAY_H
;