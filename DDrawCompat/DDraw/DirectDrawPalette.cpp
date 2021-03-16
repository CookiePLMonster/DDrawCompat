#include <cstring>
#include <deque>

#include <Common/CompatVtable.h>
#include <Common/Time.h>
#include <Config/Config.h>
#include <DDraw/DirectDrawPalette.h>
#include <DDraw/ScopedThreadLock.h>
#include <DDraw/Surfaces/PrimarySurface.h>
#include <DDraw/Visitors/DirectDrawPaletteVtblVisitor.h>

namespace
{
	HRESULT STDMETHODCALLTYPE SetEntries(
		IDirectDrawPalette* This, DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries)
	{
		if (This == DDraw::PrimarySurface::s_palette)
		{
			DDraw::DirectDrawPalette::waitForNextUpdate();
		}

		HRESULT result = getOrigVtable(This).SetEntries(
			This, dwFlags, dwStartingEntry, dwCount, lpEntries);
		if (SUCCEEDED(result) && This == DDraw::PrimarySurface::s_palette)
		{
			DDraw::PrimarySurface::updatePalette();
		}
		return result;
	}

	constexpr void setCompatVtable(IDirectDrawPaletteVtbl& vtable)
	{
		vtable.SetEntries = &SetEntries;
	}
}

namespace DDraw
{
	namespace DirectDrawPalette
	{
		void waitForNextUpdate()
		{
			static std::deque<long long> updatesInLastMs;

			const long long qpcNow = Time::queryPerformanceCounter();
			const long long qpcLastMsBegin = qpcNow - Time::g_qpcFrequency / 1000;
			while (!updatesInLastMs.empty() && qpcLastMsBegin - updatesInLastMs.front() > 0)
			{
				updatesInLastMs.pop_front();
			}

			if (updatesInLastMs.size() >= Config::maxPaletteUpdatesPerMs)
			{
				Sleep(1);
				updatesInLastMs.clear();
			}

			updatesInLastMs.push_back(Time::queryPerformanceCounter());
		}

		void hookVtable(const IDirectDrawPaletteVtbl& vtable)
		{
			CompatVtable<IDirectDrawPaletteVtbl>::hookVtable<ScopedThreadLock>(vtable);
		}
	}
}
