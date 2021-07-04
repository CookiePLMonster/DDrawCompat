#pragma once

#include <Config/Settings/CpuAffinity.h>
#include <Config/Settings/DesktopColorDepth.h>
#include <Config/Settings/DisplayFilter.h>
#include <Config/Settings/DisplayResolution.h>
#include <Config/Settings/SupportedResolutions.h>
#include <Config/Settings/ThreadPriorityBoost.h>

namespace Config
{
	const unsigned delayedFlipModeTimeout = 200;
	const unsigned evictionTimeout = 200;
	const unsigned maxPaletteUpdatesPerMs = 5;

	extern Settings::CpuAffinity cpuAffinity;
	extern Settings::DesktopColorDepth desktopColorDepth;
	extern Settings::DisplayFilter displayFilter;
	extern Settings::DisplayResolution displayResolution;
	extern Settings::SupportedResolutions supportedResolutions;
	extern Settings::ThreadPriorityBoost threadPriorityBoost;
}
