#include <Common/ScopedCriticalSection.h>

#include <Common/Hook.h>
#include <Common/Log.h>
#include <DDraw/Surfaces/PrimarySurface.h>
#include <Gdi/Cursor.h>
#include <Win32/DisplayMode.h>

namespace
{
	RECT g_clipRect = {};
	HCURSOR g_cursor = nullptr;
	bool g_isEmulated = false;
	RECT g_monitorClipRect = {};
	HCURSOR g_nullCursor = nullptr;
	CURSORINFO g_prevCursorInfo = {};
	Compat::CriticalSection g_cs;

	RECT intersectRect(RECT rect1, RECT rect2);
	void normalizeRect(RECT& rect);

	BOOL WINAPI clipCursor(const RECT* lpRect)
	{
		LOG_FUNC("ClipCursor", lpRect);
		Compat::ScopedCriticalSection lock(g_cs);
		BOOL result = CALL_ORIG_FUNC(ClipCursor)(lpRect);
		if (!result || IsRectEmpty(&g_monitorClipRect))
		{
			return LOG_RESULT(result);
		}

		CALL_ORIG_FUNC(GetClipCursor)(&g_clipRect);
		RECT rect = intersectRect(g_clipRect, g_monitorClipRect);
		CALL_ORIG_FUNC(ClipCursor)(&rect);
		return LOG_RESULT(result);
	}

	BOOL WINAPI getClipCursor(LPRECT lpRect)
	{
		LOG_FUNC("GetClipCursor", lpRect);
		Compat::ScopedCriticalSection lock(g_cs);
		BOOL result = CALL_ORIG_FUNC(GetClipCursor)(lpRect);
		if (result && !IsRectEmpty(&g_monitorClipRect))
		{
			*lpRect = g_clipRect;
		}
		return LOG_RESULT(result);
	}

	HCURSOR WINAPI getCursor()
	{
		LOG_FUNC("GetCursor");
		Compat::ScopedCriticalSection lock(g_cs);
		return LOG_RESULT(g_isEmulated ? g_cursor : CALL_ORIG_FUNC(GetCursor)());
	}

	BOOL WINAPI getCursorInfo(PCURSORINFO pci)
	{
		LOG_FUNC("GetCursorInfo", pci);
		Compat::ScopedCriticalSection lock(g_cs);
		BOOL result = CALL_ORIG_FUNC(GetCursorInfo)(pci);
		if (result && pci->hCursor == g_nullCursor)
		{
			pci->hCursor = g_cursor;
		}
		return LOG_RESULT(result);
	}

	RECT intersectRect(RECT rect1, RECT rect2)
	{
		normalizeRect(rect1);
		normalizeRect(rect2);
		IntersectRect(&rect1, &rect1, &rect2);
		return rect1;
	}

	void normalizeRect(RECT& rect)
	{
		if (rect.left == rect.right && rect.top == rect.bottom)
		{
			rect.right++;
			rect.bottom++;
		}
	}

	HCURSOR WINAPI setCursor(HCURSOR hCursor)
	{
		LOG_FUNC("SetCursor", hCursor);
		return LOG_RESULT(Gdi::Cursor::setCursor(hCursor));
	}

	void updateClipRect()
	{
		auto hwnd = GetForegroundWindow();
		if (!hwnd)
		{
			return;
		}

		DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		if (pid != GetCurrentProcessId())
		{
			return;
		}

		RECT realClipRect = {};
		CALL_ORIG_FUNC(GetClipCursor)(&realClipRect);

		RECT clipRect = intersectRect(g_clipRect, g_monitorClipRect);
		if (!EqualRect(&clipRect, &realClipRect))
		{
			CALL_ORIG_FUNC(ClipCursor)(&clipRect);
		}
	}
}

namespace Gdi
{
	namespace Cursor
	{
		CURSORINFO getEmulatedCursorInfo()
		{
			CURSORINFO ci = {};
			ci.cbSize = sizeof(ci);

			Compat::ScopedCriticalSection lock(g_cs);
			if (g_isEmulated)
			{
				CALL_ORIG_FUNC(GetCursorInfo)(&ci);
				if (ci.hCursor == g_nullCursor)
				{
					ci.hCursor = g_cursor;
				}
				else
				{
					ci.hCursor = nullptr;
					ci.flags = 0;
				}
			}

			return ci;
		}

		void installHooks()
		{
			BYTE andPlane = 0xFF;
			BYTE xorPlane = 0;
			g_nullCursor = CreateCursor(nullptr, 0, 0, 1, 1, &andPlane, &xorPlane);

			HOOK_FUNCTION(user32, ClipCursor, clipCursor);
			HOOK_FUNCTION(user32, GetCursor, getCursor);
			HOOK_FUNCTION(user32, GetCursorInfo, getCursorInfo);
			HOOK_FUNCTION(user32, GetClipCursor, getClipCursor);
			HOOK_FUNCTION(user32, SetCursor, ::setCursor);
		}

		bool isEmulated()
		{
			return g_isEmulated;
		}

		HCURSOR setCursor(HCURSOR cursor)
		{
			Compat::ScopedCriticalSection lock(g_cs);
			if (!g_isEmulated)
			{
				return CALL_ORIG_FUNC(SetCursor)(cursor);
			}

			HCURSOR prevCursor = g_cursor;
			if (cursor != g_nullCursor)
			{
				g_cursor = cursor;
				CALL_ORIG_FUNC(SetCursor)(cursor ? g_nullCursor : nullptr);
			}
			return prevCursor;
		}

		void setEmulated(bool isEmulated)
		{
			Compat::ScopedCriticalSection lock(g_cs);
			if (isEmulated == g_isEmulated)
			{
				return;
			}

			g_isEmulated = isEmulated;
			g_prevCursorInfo = {};

			if (isEmulated)
			{
				setCursor(CALL_ORIG_FUNC(GetCursor)());
			}
			else
			{
				CALL_ORIG_FUNC(SetCursor)(g_cursor);
				g_cursor = nullptr;
			}
		}

		void setMonitorClipRect(const RECT& rect)
		{
			Compat::ScopedCriticalSection lock(g_cs);
			if (IsRectEmpty(&rect))
			{
				if (!IsRectEmpty(&g_monitorClipRect))
				{
					CALL_ORIG_FUNC(ClipCursor)(&g_clipRect);
					g_clipRect = {};
					g_monitorClipRect = {};
				}
			}
			else
			{
				if (IsRectEmpty(&g_monitorClipRect))
				{
					CALL_ORIG_FUNC(GetClipCursor)(&g_clipRect);
				}
				g_monitorClipRect = rect;
				updateClipRect();
			}
		}

		bool update()
		{
			Compat::ScopedCriticalSection lock(g_cs);
			if (!IsRectEmpty(&g_monitorClipRect))
			{
				updateClipRect();
			}

			if (g_isEmulated)
			{
				CURSORINFO cursorInfo = getEmulatedCursorInfo();
				if ((CURSOR_SHOWING == cursorInfo.flags) != (CURSOR_SHOWING == g_prevCursorInfo.flags) ||
					CURSOR_SHOWING == cursorInfo.flags &&
					(cursorInfo.hCursor != g_prevCursorInfo.hCursor || cursorInfo.ptScreenPos != g_prevCursorInfo.ptScreenPos))
				{
					g_prevCursorInfo = cursorInfo;
					return true;
				}
			}

			return false;
		}
	}
}
