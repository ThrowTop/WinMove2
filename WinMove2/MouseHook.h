// MouseHook.h
#pragma once
#include <windows.h>
#include <thread>
#include <atomic>
#include "Overlay.h"

struct MouseEvent { WPARAM type; POINT pt; };

class MouseHook {
public:
	MouseHook(Overlay* overlay);
	~MouseHook();

private:
	static LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam);
	void Worker();

	HHOOK               hook_ = nullptr;
	Overlay* overlay_ = nullptr;
	std::thread         worker_;
	std::atomic<bool>   running_{ false };

	static constexpr int BUFFER_SIZE = 128;
	MouseEvent          buffer_[BUFFER_SIZE]{};
	std::atomic<int>    head_{ 0 };
	std::atomic<int>    tail_{ 0 };
	std::atomic<POINT>  latestPos_{ POINT{0,0} };
	std::atomic<bool>   movePending_{ false };
	std::atomic<bool>   dragging_{ false };
	std::atomic<bool>   resizing_{ false };
	POINT               startPos_{ 0,0 };
	RECT                startRect_{ 0,0,0,0 };
	HWND                targetWindow_ = nullptr;
};
