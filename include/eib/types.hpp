#pragma once
#include <cstdint>

namespace eib {

// Core domain types. Prices are INTEGER ticks, never floating point. Quantities are signed so intermediate arithmetic can't silently wrap;
// resting sizes are >= 0 by invariant.
using Ticks     = std::int64_t;   // price expressed in integer ticks
using Qty       = std::int64_t;   // order / level size
using OrderId   = std::uint64_t;  // venue order reference
using Timestamp = std::uint64_t;  // venue timestamp (nanoseconds)

enum class Side : std::uint8_t { Buy, Sell };

}  // namespace eib
