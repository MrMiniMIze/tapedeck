#pragma once
#include <cstdint>

namespace eib::detail {

// FNV-1a 64-bit, fed byte by byte so the digest is endian stable. Shared by the
// reference and flat order books so their state_hash values are identical, which
// is what lets the differential test compare them directly.
inline constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
inline constexpr std::uint64_t kFnvPrime = 1099511628211ull;

inline void fnv(std::uint64_t& h, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    h ^= (v & 0xffu);
    h *= kFnvPrime;
    v >>= 8;
  }
}

}  // namespace eib::detail
