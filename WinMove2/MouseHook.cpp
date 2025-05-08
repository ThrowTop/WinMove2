// MouseHook.cpp
#include "MouseHook.h"
#include "Utils.h"
#include <chrono>
#include <thread>

static MouseHook* g_instance = nullptr;

MouseHook::MouseHook(Overlay* overlay)
	: overlay_(overlay)
	, running_(true) {
	g_instance = this;
	hook_ = SetWindowsHookExW(WH_MOUSE_LL, HookProc, nullptr, 0);
	worker_ = std::thread(&MouseHook::Worker, this);
}

MouseHook::~MouseHook() {
	running_ = false;
	if (worker_.joinable()) worker_.join();
	if (hook_) UnhookWindowsHookEx(hook_);
	g_instance = nullptr;
}

LRESULT CALLBACK MouseHook::HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode < 0)
		return CallNextHookEx(nullptr, nCode, wParam, lParam);

	auto ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
	if (!ms)
		return CallNextHookEx(nullptr, nCode, wParam, lParam);

	bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
	if (!altHeld)
		return CallNextHookEx(nullptr, nCode, wParam, lParam);

	if (wParam == WM_MOUSEMOVE) {
		g_instance->latestPos_.store(ms->pt);
		g_instance->movePending_.store(true);
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	switch (wParam) {
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		{
			int idx = g_instance->head_.fetch_add(1) % BUFFER_SIZE;
			g_instance->buffer_[idx] = { wParam, ms->pt };
			return 1;  // block button events
		}
		default:
			return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}
}

void MouseHook::Worker() {
	using namespace std::chrono_literals;
	while (running_) {
		int t = tail_.load();
		int h = head_.load();
		while (t != h) {
			MouseEvent evt = buffer_[t % BUFFER_SIZE];
			tail_.store(++t);
			POINT p = evt.pt;
			if (evt.type == WM_LBUTTONDOWN) {
				targetWindow_ = Utils::GetWindow(p);
				GetWindowRect(targetWindow_, &startRect_);
				startPos_ = p;
				dragging_.store(true);
				resizing_.store(false);
				overlay_->StartDrag(targetWindow_, p);
			}
			else if (evt.type == WM_RBUTTONDOWN) {
				targetWindow_ = Utils::GetWindow(p);
				GetWindowRect(targetWindow_, &startRect_);
				startPos_ = p;
				resizing_.store(true);
				dragging_.store(false);
				overlay_->StartResize(targetWindow_, p);
			}
			else if (evt.type == WM_LBUTTONUP && dragging_.exchange(false)) {
				overlay_->StopDrag();
				overlay_->HideOverlay();
			}
			else if (evt.type == WM_RBUTTONUP && resizing_.exchange(false)) {
				overlay_->StopResize();
				overlay_->HideOverlay();
			}
			h = head_.load();
		}

		if (movePending_.exchange(false)) {
			POINT p = latestPos_.load();
			if (dragging_)   overlay_->PerformDrag(p);
			else if (resizing_) overlay_->PerformResize(p);
		}

		if ((GetAsyncKeyState(VK_MENU) & 0x8000) == 0) {
			if (dragging_.exchange(false)) { overlay_->StopDrag();   overlay_->HideOverlay(); }
			if (resizing_.exchange(false)) { overlay_->StopResize(); overlay_->HideOverlay(); }
		}

		std::this_thread::sleep_for(10ms);
	}
}