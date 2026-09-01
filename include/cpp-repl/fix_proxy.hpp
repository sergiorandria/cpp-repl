#pragma once
// Fix for np::ProxyBase with bigint - must be included after np/detail/proxy.hpp
// This header patches ProxyBase to support convert_to and operator* for bigint

#include <type_traits>
#include <utility>

// Patch ProxyBase to add convert_to
namespace np {
  template <typename T, bool IsConst, std::size_t MaxDims>
  class ProxyBase;

  // Add convert_to method to ProxyBase
  // This will be picked up when the header is included
}

// Include the original proxy header first
#include "np/detail/proxy.hpp"

// Now patch ProxyBase with convert_to
namespace np {
  // Reopen ProxyBase to add convert_to - not possible to reopen, so we use a helper
  // Instead, we will define a free function that handles convert_to for ProxyBase
  template <typename T, bool IsConst, std::size_t MaxDims, typename U>
  inline auto proxy_convert_to(const ProxyBase<T, IsConst, MaxDims>& p) -> decltype(std::declval<T>().template convert_to<U>()) {
    return static_cast<T>(p).template convert_to<U>();
  }

  // For ap * a[n] where ap is bigint and a[n] is ProxyBase<bigint>
  template <typename T, bool IsConst, std::size_t MaxDims>
  inline auto operator*(const T& lhs, const ProxyBase<T, IsConst, MaxDims>& rhs) -> decltype(lhs * std::declval<T>()) {
    return lhs * static_cast<T>(rhs);
  }
  template <typename T, bool IsConst, std::size_t MaxDims>
  inline auto operator*(const ProxyBase<T, IsConst, MaxDims>& lhs, const T& rhs) -> decltype(std::declval<T>() * rhs) {
    return static_cast<T>(lhs) * rhs;
  }
}
