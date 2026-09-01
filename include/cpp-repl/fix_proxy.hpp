/**
 * @file fix_proxy.hpp
 * @brief Patch for np::ProxyBase to support convert_to and operators.
 */
#pragma once
/** @brief Force-included fix for ProxyBase with boost::multiprecision. */

#include <type_traits>
#include <utility>

// Patch ProxyBase to add convert_to for types that have it (like cpp_int)
// This must be included after np/detail/proxy.hpp is included, but before user code
// We will handle it by defining a helper that makes E4[i].convert_to work

// The actual patch is to add convert_to to ProxyBase via a using declaration
// We do this by reopening the class and adding the method

// Include the original proxy header first
#include "np/detail/proxy.hpp"

// Now patch ProxyBase to add convert_to with SFINAE
namespace np {
  // Add convert_to to ProxyBase for any T that has convert_to
  template <typename T, bool IsConst, std::size_t MaxDims>
  class ProxyBase;

  // Specialization to add convert_to
  // We use a helper to detect if T has convert_to
  namespace detail {
    template <typename T, typename U, typename = void>
    struct has_convert_to : std::false_type {};
    template <typename T, typename U>
    struct has_convert_to<T, U, std::void_t<decltype(std::declval<T>().template convert_to<U>())>> : std::true_type {};
  }

  // Patch ProxyBase: add convert_to method via inheritance or via a free function
  // We will define a free function that handles ProxyBase convert_to
  template <typename T, bool IsConst, std::size_t MaxDims, typename U>
  inline auto proxy_convert_to(const ProxyBase<T, IsConst, MaxDims>& p) -> decltype(std::declval<T>().template convert_to<U>()) {
    return static_cast<T>(p).template convert_to<U>();
  }
}

// For ap * a[n] where a[n] is ProxyBase, we already have operator* in the fix
// But we need to make it work for any T, not just bigint
namespace np {
  template <typename T, bool IsConst, std::size_t MaxDims>
  inline auto operator*(const T& lhs, const ProxyBase<T, IsConst, MaxDims>& rhs) -> decltype(lhs * std::declval<T>()) {
    return lhs * static_cast<T>(rhs);
  }
  template <typename T, bool IsConst, std::size_t MaxDims>
  inline auto operator*(const ProxyBase<T, IsConst, MaxDims>& lhs, const T& rhs) -> decltype(std::declval<T>() * rhs) {
    return static_cast<T>(lhs) * rhs;
  }
}
