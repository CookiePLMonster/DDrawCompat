#pragma once

#include <vector>

#include <Config/MappedSetting.h>

namespace Config
{
	class EnumSetting : public MappedSetting<unsigned>
	{
	protected:
		EnumSetting(const std::string& name, const std::string& default, const std::vector<std::string>& enumNames);
	};
}
