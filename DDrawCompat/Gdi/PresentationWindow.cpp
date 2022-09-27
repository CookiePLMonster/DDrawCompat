#include <Common/Hook.h>
#include <Common/Log.h>
#include <D3dDdi/ScopedCriticalSection.h>
#include <Dll/Dll.h>
#include <Gdi/PresentationWindow.h>
#include <Gdi/WinProc.h>
#include <Overlay/ConfigWindow.h>
#include <Win32/DisplayMode.h>

namespace
{
	const UINT WM_CREATEPRESENTATIONWINDOW = WM_USER;
	const UINT WM_SETPRESENTATIONWINDOWPOS = WM_USER + 1;
	const UINT WM_SETPRESENTATIONWINDOWRGN = WM_USER + 2;

	HANDLE g_presentationWindowThread = nullptr;
	unsigned g_presentationWindowThreadId = 0;
	Overlay::ConfigWindow* g_configWindow = nullptr;
	HWND g_messageWindow = nullptr;
	bool g_isThreadReady = false;

	BOOL CALLBACK initChildWindow(HWND hwnd, LPARAM /*lParam*/)
	{
		Gdi::WinProc::onCreateWindow(hwnd);
		return TRUE;
	}

	BOOL CALLBACK initTopLevelWindow(HWND hwnd, LPARAM /*lParam*/)
	{
		DWORD windowPid = 0;
		GetWindowThreadProcessId(hwnd, &windowPid);
		if (GetCurrentProcessId() == windowPid)
		{
			Gdi::WinProc::onCreateWindow(hwnd);
			EnumChildWindows(hwnd, &initChildWindow, 0);
			if (8 == Win32::DisplayMode::getBpp())
			{
				PostMessage(hwnd, WM_PALETTECHANGED, reinterpret_cast<WPARAM>(GetDesktopWindow()), 0);
			}
		}
		return TRUE;
	}

	LRESULT CALLBACK messageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		LOG_FUNC("messageWindowProc", Compat::WindowMessageStruct(hwnd, uMsg, wParam, lParam));

		switch (uMsg)
		{
		case WM_CREATEPRESENTATIONWINDOW:
		{
			// Workaround for ForceSimpleWindow shim
			static auto origCreateWindowExA = reinterpret_cast<decltype(&CreateWindowExA)>(
				Compat::getProcAddress(GetModuleHandle("user32"), "CreateWindowExA"));

			HWND owner = reinterpret_cast<HWND>(wParam);
			HWND presentationWindow = origCreateWindowExA(
				WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOPARENTNOTIFY | WS_EX_TOOLWINDOW,
				"DDrawCompatPresentationWindow",
				nullptr,
				WS_DISABLED | WS_POPUP,
				0, 0, 1, 1,
				owner,
				nullptr,
				nullptr,
				nullptr);

			if (presentationWindow)
			{
				if (lParam)
				{
					CALL_ORIG_FUNC(SetWindowLongA)(presentationWindow, GWL_WNDPROC, lParam);
				}
				CALL_ORIG_FUNC(SetLayeredWindowAttributes)(presentationWindow, 0, 255, LWA_ALPHA);
			}

			return LOG_RESULT(reinterpret_cast<LRESULT>(presentationWindow));
		}

		case WM_DESTROY:
			PostQuitMessage(0);
			return LOG_RESULT(0);
		}

		return LOG_RESULT(CALL_ORIG_FUNC(DefWindowProc)(hwnd, uMsg, wParam, lParam));
	}

	LRESULT CALLBACK presentationWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		LOG_FUNC("presentationWindowProc", Compat::WindowMessageStruct(hwnd, uMsg, wParam, lParam));

		switch (uMsg)
		{
		case WM_SETPRESENTATIONWINDOWPOS:
		{
			const auto& wp = *reinterpret_cast<WINDOWPOS*>(lParam);
			return SetWindowPos(hwnd, wp.hwndInsertAfter, wp.x, wp.y, wp.cx, wp.cy, wp.flags);
		}

		case WM_SETPRESENTATIONWINDOWRGN:
		{
			HRGN rgn = nullptr;
			if (wParam)
			{
				rgn = CreateRectRgn(0, 0, 0, 0);
				CombineRgn(rgn, reinterpret_cast<HRGN>(wParam), nullptr, RGN_COPY);
			}
			return SetWindowRgn(hwnd, rgn, FALSE);
		}
		}

		return CALL_ORIG_FUNC(DefWindowProc)(hwnd, uMsg, wParam, lParam);
	}

	unsigned WINAPI presentationWindowThreadProc(LPVOID /*lpParameter*/)
	{
		WNDCLASS wc = {};
		wc.lpfnWndProc = &messageWindowProc;
		wc.hInstance = Dll::g_currentModule;
		wc.lpszClassName = "DDrawCompatMessageWindow";
		CALL_ORIG_FUNC(RegisterClassA)(&wc);

		g_messageWindow = CreateWindow(
			"DDrawCompatMessageWindow", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
		if (!g_messageWindow)
		{
			Compat::Log() << "ERROR: Failed to create a message-only window";
			return 0;
		}

		{
			D3dDdi::ScopedCriticalSection lock;
			Gdi::WinProc::installHooks();
			g_isThreadReady = true;
			EnumWindows(initTopLevelWindow, 0);
		}

		Compat::closeDbgEng();

		Overlay::ConfigWindow configWindow;
		g_configWindow = &configWindow;

		MSG msg = {};
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return 0;
	}

	LRESULT sendMessageBlocking(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		DWORD_PTR result = 0;
		SendMessageTimeout(hwnd, msg, wParam, lParam, SMTO_BLOCK | SMTO_NOTIMEOUTIFNOTHUNG, 0, &result);
		return result;
	}
}

namespace Gdi
{
	namespace PresentationWindow
	{
		HWND create(HWND owner, WNDPROC wndProc)
		{
			return reinterpret_cast<HWND>(sendMessageBlocking(g_messageWindow, WM_CREATEPRESENTATIONWINDOW,
				reinterpret_cast<WPARAM>(owner), reinterpret_cast<LPARAM>(wndProc)));
		}

		void destroy(HWND hwnd)
		{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		}

		Overlay::ConfigWindow* getConfigWindow()
		{
			return g_configWindow;
		}

		void installHooks()
		{
			WNDCLASS wc = {};
			wc.lpfnWndProc = &presentationWindowProc;
			wc.hInstance = Dll::g_currentModule;
			wc.lpszClassName = "DDrawCompatPresentationWindow";
			CALL_ORIG_FUNC(RegisterClassA)(&wc);

			g_presentationWindowThread = Dll::createThread(presentationWindowThreadProc, &g_presentationWindowThreadId,
				THREAD_PRIORITY_TIME_CRITICAL, CREATE_SUSPENDED);
		}

		bool isPresentationWindow(HWND hwnd)
		{
			return GetWindowThreadProcessId(hwnd, nullptr) == g_presentationWindowThreadId;
		}

		bool isThreadReady()
		{
			return g_isThreadReady;
		}

		void setWindowPos(HWND hwnd, const WINDOWPOS& wp)
		{
			sendMessageBlocking(hwnd, WM_SETPRESENTATIONWINDOWPOS, 0, reinterpret_cast<WPARAM>(&wp));
		}

		void setWindowRgn(HWND hwnd, HRGN rgn)
		{
			sendMessageBlocking(hwnd, WM_SETPRESENTATIONWINDOWRGN, reinterpret_cast<WPARAM>(rgn), 0);
		}

		void startThread()
		{
			ResumeThread(g_presentationWindowThread);
		}
	}
}
