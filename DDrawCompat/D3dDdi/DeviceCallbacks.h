#pragma once

#include <d3d.h>
#include <d3dumddi.h>

#include <D3dDdi/Log/DeviceCallbacksLog.h>

namespace D3dDdi
{
	namespace DeviceCallbacks
	{
		void hookVtable(const D3DDDI_DEVICECALLBACKS& vtable, UINT version);
	}
}
