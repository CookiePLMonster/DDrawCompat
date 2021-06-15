#pragma once

#include <tuple>
#include <type_traits>

#include <Windows.h>

template <typename T>
std::enable_if_t<std::is_class_v<T> && std::is_trivial_v<T>, bool> operator==(const T& left, const T& right)
{
	return toTuple(left) == toTuple(right);
}

template <typename T>
std::enable_if_t<std::is_class_v<T>&& std::is_trivial_v<T>, bool> operator!=(const T& left, const T& right)
{
	return toTuple(left) != toTuple(right);
}

template <typename T>
std::enable_if_t<std::is_class_v<T> && std::is_trivial_v<T>, bool> operator<(const T& left, const T& right)
{
	return toTuple(left) < toTuple(right);
}

inline auto toTuple(const LUID& luid)
{
	return std::make_tuple(luid.LowPart, luid.HighPart);
}

inline auto toTuple(const POINT& pt)
{
	return std::make_tuple(pt.x, pt.y);
}

inline auto toTuple(const SIZE& size)
{
	return std::make_tuple(size.cx, size.cy);
}
