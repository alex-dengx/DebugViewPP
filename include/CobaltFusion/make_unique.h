// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#pragma once

#include <memory>

namespace fusion {

template <typename T>
std::unique_ptr<T> make_unique()
{
	return std::unique_ptr<T>(new T);
}

template <typename T, typename A1>
std::unique_ptr<T> make_unique(A1&& a1)
{
	return std::unique_ptr<T>(new T(std::forward<A1>(a1)));
}

template <typename T, typename A1, typename A2>
std::unique_ptr<T> make_unique(A1&& a1, A2&& a2)
{
	return std::unique_ptr<T>(new T(std::forward<A1>(a1), std::forward<A2>(a2)));
}

template <typename T, typename A1, typename A2, typename A3>
std::unique_ptr<T> make_unique(A1&& a1, A2&& a2, A3&& a3)
{
	return std::unique_ptr<T>(new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3)));
}

template <typename T, typename A1, typename A2, typename A3, typename A4>
std::unique_ptr<T> make_unique(A1&& a1, A2&& a2, A3&& a3, A4&& a4)
{
	return std::unique_ptr<T>(new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4)));
}

template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5>
std::unique_ptr<T> make_unique(A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5)
{
	return std::unique_ptr<T>(new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4), std::forward<A5>(a5)));
}

template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
std::unique_ptr<T> make_unique(A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6)
{
	return std::unique_ptr<T>(new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6)));
}
} // namespace fusion
