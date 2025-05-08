#include <windows.h>
#include "MouseHook.h"
#include "Overlay.h"
#include "Utils.h"

using namespace Utils;

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	Overlay overlay;
	MouseHook mh(&overlay);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}