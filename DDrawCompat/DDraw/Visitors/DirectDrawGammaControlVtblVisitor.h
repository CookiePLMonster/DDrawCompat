#pragma once

#include <Common/VtableVisitor.h>

template <>
struct VtableForEach<IDirectDrawGammaControlVtbl>
{
	template <typename Vtable, typename Visitor>
	static void forEach(Visitor& visitor, UINT version)
	{
		VtableForEach<IUnknownVtbl>::forEach<Vtable>(visitor, version);

		DD_VISIT(GetGammaRamp);
		DD_VISIT(SetGammaRamp);
	}
};
