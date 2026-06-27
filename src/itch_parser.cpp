#include "eib/itch_parser.hpp"

namespace eib {
namespace {

inline std::uint32_t be32(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) |
         (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) |
         static_cast<std::uint32_t>(p[3]);
}

// Read an n-byte big-endian unsigned (n <= 8). Used for 6-byte timestamps and
// 8-byte order references.
inline std::uint64_t be_n(const std::uint8_t* p, int n) {
  std::uint64_t v = 0;
  for (int i = 0; i < n; ++i) v = (v << 8) | static_cast<std::uint64_t>(p[i]);
  return v;
}

}  // namespace

ItchStatus decode_itch_message(std::span<const std::uint8_t> m, MarketEvent& out) {
  if (m.empty()) return ItchStatus::Truncated;
  const std::uint8_t* p = m.data();

  switch (static_cast<char>(p[0])) {
    case 'A':  // Add Order (no MPID), 36 bytes
    case 'F':  // Add Order with MPID, 40 bytes (MPID ignored for the book)
      if (m.size() < 36u) return ItchStatus::Truncated;
      out = MarketEvent::add(be_n(p + 11, 8),
                             (p[19] == 'B') ? Side::Buy : Side::Sell,
                             static_cast<Ticks>(be32(p + 32)),
                             static_cast<Qty>(be32(p + 20)),
                             be_n(p + 5, 6));
      return ItchStatus::Event;

    case 'E':  // Order Executed, 31 bytes
      if (m.size() < 31u) return ItchStatus::Truncated;
      out = MarketEvent::execute(be_n(p + 11, 8), static_cast<Qty>(be32(p + 19)),
                                 be_n(p + 5, 6));
      return ItchStatus::Event;

    case 'C':  // Order Executed With Price, 36 bytes (book reduces by exec shares)
      if (m.size() < 36u) return ItchStatus::Truncated;
      out = MarketEvent::execute(be_n(p + 11, 8), static_cast<Qty>(be32(p + 19)),
                                 be_n(p + 5, 6));
      return ItchStatus::Event;

    case 'X':  // Order Cancel (partial), 23 bytes
      if (m.size() < 23u) return ItchStatus::Truncated;
      out = MarketEvent::reduce(be_n(p + 11, 8), static_cast<Qty>(be32(p + 19)),
                                be_n(p + 5, 6));
      return ItchStatus::Event;

    case 'D':  // Order Delete, 19 bytes
      if (m.size() < 19u) return ItchStatus::Truncated;
      out = MarketEvent::cancel(be_n(p + 11, 8), be_n(p + 5, 6));
      return ItchStatus::Event;

    case 'U':  // Order Replace, 35 bytes
      if (m.size() < 35u) return ItchStatus::Truncated;
      out = MarketEvent::replace(be_n(p + 11, 8), be_n(p + 19, 8),
                                 static_cast<Ticks>(be32(p + 31)),
                                 static_cast<Qty>(be32(p + 27)),
                                 be_n(p + 5, 6));
      return ItchStatus::Event;

    default:
      return ItchStatus::Ignored;
  }
}

}  // namespace eib
