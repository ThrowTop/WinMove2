#include <windows.h>
#include <stdio.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <string>
#include <format>

// --- Base Message and Derived Types ---

enum class MsgType {
	MousePosition,
	MouseButton,
	Info
};

struct MessageBase {
	MsgType type;
	virtual ~MessageBase() = default;
};

struct MousePositionMessage : public MessageBase {
	int x, y;
	MousePositionMessage(int _x, int _y) : x(_x), y(_y) { type = MsgType::MousePosition; }
};

struct MouseButtonMessage : public MessageBase {
	bool leftDown;
	MouseButtonMessage(bool _leftDown) : leftDown(_leftDown) { type = MsgType::MouseButton; }
};

struct InfoMessage : public MessageBase {
	std::string text;
	InfoMessage(const std::string& txt) : text(txt) { type = MsgType::Info; }
};

// --- Thread-Safe Queue for Messages ---

class MessageQueue {
public:
	void push(std::unique_ptr<MessageBase> msg) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue.push(std::move(msg));
		m_cv.notify_one();
	}

	// Wait until a message is available.
	bool pop(std::unique_ptr<MessageBase>& msg) {
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [&]() { return !m_queue.empty() || m_shutdown; });
		if (m_queue.empty())
			return false;
		msg = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	void shutdown() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_shutdown = true;
		m_cv.notify_all();
	}

private:
	std::queue<std::unique_ptr<MessageBase>> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_shutdown = false;
};

MessageQueue g_msgQueue;

// --- Consumer Thread (Processes Messages) ---
void ConsumerThreadProc() {
	std::unique_ptr<MessageBase> msg;
	while (g_msgQueue.pop(msg)) {
		switch (msg->type) {
			case MsgType::MousePosition:
			{
				auto posMsg = static_cast<MousePositionMessage*>(msg.get());
				printf("Mouse Position: x=%d, y=%d\n", posMsg->x, posMsg->y);
				break;
			}
			case MsgType::MouseButton:
			{
				auto btnMsg = static_cast<MouseButtonMessage*>(msg.get());
				printf("Mouse Button: Left %s\n", btnMsg->leftDown ? "Down" : "Up");
				break;
			}
			case MsgType::Info:
			{
				auto infoMsg = static_cast<InfoMessage*>(msg.get());
				printf("Info: %s\n", infoMsg->text.c_str());
				break;
			}
		}
	}
	printf("Consumer thread exiting.\n");
}

// --- Console Creation Function ---
void CreateConsole() {
	AllocConsole();
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hOut, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	// Disable QuickEdit mode.
	dwMode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hOut, dwMode);
	FILE* stream = nullptr;
	freopen_s(&stream, "CONOUT$", "w", stdout);
	HWND consoleWindow = GetConsoleWindow();
	SetWindowPos(consoleWindow, 0, 20, 50, 100, 100, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

// --- Mouse Hook Class ---
class MouseHook {
public:
	MouseHook() : m_hHook(nullptr), m_running(false) {}
	~MouseHook() { Stop(); }

	void Start() {
		m_running = true;
		m_thread = std::thread(&MouseHook::ThreadProc, this);
	}

	void Stop() {
		m_running = false;
		if (m_thread.joinable()) {
			PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
			m_thread.join();
		}
	}

private:
	// The hook procedure posts messages to the queue.
	static LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
		LARGE_INTEGER start, end, freq;
		QueryPerformanceCounter(&start);
		if (nCode >= 0) {
			PMSLLHOOKSTRUCT pMouseStruct = reinterpret_cast<PMSLLHOOKSTRUCT>(lParam);
			if (pMouseStruct) {
				if (wParam == WM_MOUSEMOVE) {
					auto msg = std::make_unique<MousePositionMessage>(pMouseStruct->pt.x, pMouseStruct->pt.y);
					g_msgQueue.push(std::move(msg));
				}
				else if (wParam == WM_LBUTTONDOWN || wParam == WM_LBUTTONUP) {
					bool leftDown = (wParam == WM_LBUTTONDOWN);
					//Sleep(2);
					auto msg = std::make_unique<MouseButtonMessage>(leftDown);
					g_msgQueue.push(std::move(msg));
				}
			}
		}
		QueryPerformanceCounter(&end);
		QueryPerformanceFrequency(&freq);
		double elapsedMs = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
		if (elapsedMs > 1.0) {
			std::string info = std::format("Hook execution exceeded 1ms ({:.3f} ms). Unhooking.", elapsedMs);
			auto msg = std::make_unique<InfoMessage>(info);
			g_msgQueue.push(std::move(msg));
			PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0);
		}
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	void ThreadProc() {
		m_threadId = GetCurrentThreadId();
		m_hHook = SetWindowsHookEx(WH_MOUSE_LL, HookProc, GetModuleHandle(nullptr), 0);
		if (!m_hHook) {
			std::string info = std::format("Failed to install mouse hook. Error: {}", GetLastError());
			g_msgQueue.push(std::make_unique<InfoMessage>(info));
			return;
		}
		g_msgQueue.push(std::make_unique<InfoMessage>("Mouse hook installed in thread."));
		MSG m;
		while (m_running && GetMessage(&m, nullptr, 0, 0) > 0) {
			TranslateMessage(&m);
			DispatchMessage(&m);
		}
		if (m_hHook) {
			UnhookWindowsHookEx(m_hHook);
			m_hHook = nullptr;
		}
		g_msgQueue.push(std::make_unique<InfoMessage>("Mouse hook thread exiting."));
	}

	HHOOK m_hHook;
	std::thread m_thread;
	DWORD m_threadId = 0;
	std::atomic<bool> m_running;
};

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow) {
	CreateConsole();

	// Launch the consumer thread.
	std::thread consumerThread(ConsumerThreadProc);
	consumerThread.detach();

	// Start the mouse hook.
	MouseHook mouseHook;
	mouseHook.Start();

	// Main loop.
	while (true)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

	mouseHook.Stop();
	g_msgQueue.shutdown();
	return 0;
}