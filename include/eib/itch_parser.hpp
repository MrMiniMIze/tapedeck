#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_set>

#include "eib/event.hpp"

namespace eib {

// NASDAQ TotalView-ITCH 5.0 feed parser.
//
// Handles the book-affecting message types: Add ('A'), Add-with-MPID ('F'),
// Order Executed ('E'), Executed-With-Price ('C'), Order Cancel ('X', partial),
// Order Delete ('D'), Order Replace ('U'). All other message types are ignored
// for book reconstruction. All integers are big-endian; prices are 4-byte
// integers in units of 1/10000 (already integer ticks). Order references are
// 8-byte; timestamps are 48-bit nanoseconds since midnight.

enum class ItchStatus : std::uint8_t { Event, Ignored, Truncated };

// Decode ONE ITCH message (type byte at m[0]) into a normalized MarketEvent.
// Returns Ignored for book-irrelevant types and Truncated if the buffer is
// shorter than the message's fixed layout. Zero-copy, reads straight from the
// caller's buffer, no allocation.
ItchStatus decode_itch_message(std::span<const std::uint8_t> m, MarketEvent& out);

// Drive a buffer of length-framed ITCH messages, each message preceded by a
// 2-byte big-endian length, as in NASDAQ's downloadable sample files, invoking
// on_event(const MarketEvent&) for each book-relevant message. Templated on the
// callback so there is no std::function on the hot path.
// Returns the number of events dispatched; stops cleanly at a truncated tail.
template <class OnEvent>
std::size_t parse_itch_stream(std::span<const std::uint8_t> buf, OnEvent&& on_event) {
  std::size_t consumed = 0;
  std::size_t count = 0;
  while (consumed + 2 <= buf.size()) {
    const std::size_t len = (static_cast<std::size_t>(buf[consumed]) << 8) |
                            static_cast<std::size_t>(buf[consumed + 1]);
    if (len == 0) break;                          // framing terminator
    if (consumed + 2 + len > buf.size()) break;   // truncated tail: stop cleanly
    MarketEvent ev;
    if (decode_itch_message(buf.subspan(consumed + 2, len), ev) == ItchStatus::Event) {
      on_event(ev);
      ++count;
    }
    consumed += 2 + len;
  }
  return count;
}

namespace detail {
inline std::uint64_t read_be(const std::uint8_t* p, int n) {
  std::uint64_t v = 0;
  for (int i = 0; i < n; ++i) v = (v << 8) | static_cast<std::uint64_t>(p[i]);
  return v;
}
}  // namespace detail

// Like parse_itch_stream, but emits only events for orders belonging to one
// stock symbol (8 chars, space padded, e.g. "AAPL    "). Real ITCH interleaves
// every symbol on one feed, so the BookBuilder needs a single-symbol stream.
// Order ids for the symbol are tracked, so the id-based events (Execute, Cancel,
// Reduce, Replace) are filtered correctly even though they carry no symbol.
template <class OnEvent>
std::size_t parse_itch_symbol(std::span<const std::uint8_t> buf,
                              const char symbol[8], OnEvent&& on_event) {
  std::unordered_set<OrderId> ours;
  std::size_t consumed = 0;
  std::size_t count = 0;
  while (consumed + 2 <= buf.size()) {
    const std::size_t len = (static_cast<std::size_t>(buf[consumed]) << 8) |
                            static_cast<std::size_t>(buf[consumed + 1]);
    if (len == 0) break;
    if (consumed + 2 + len > buf.size()) break;
    const std::uint8_t* m = buf.data() + consumed + 2;
    const char type = static_cast<char>(m[0]);

    bool keep = false;
    if ((type == 'A' || type == 'F') && len >= 36u) {
      if (std::memcmp(m + 24, symbol, 8) == 0) {
        ours.insert(detail::read_be(m + 11, 8));
        keep = true;
      }
    } else if ((type == 'E' || type == 'C' || type == 'X' || type == 'D') &&
               len >= 19u) {
      const OrderId id = detail::read_be(m + 11, 8);
      if (ours.find(id) != ours.end()) {
        keep = true;
        if (type == 'D') ours.erase(id);
      }
    } else if (type == 'U' && len >= 35u) {
      const OrderId old_id = detail::read_be(m + 11, 8);
      if (ours.find(old_id) != ours.end()) {
        ours.insert(detail::read_be(m + 19, 8));
        ours.erase(old_id);
        keep = true;
      }
    }

    if (keep) {
      MarketEvent ev;
      if (decode_itch_message(buf.subspan(consumed + 2, len), ev) ==
          ItchStatus::Event) {
        on_event(ev);
        ++count;
      }
    }
    consumed += 2 + len;
  }
  return count;
}

}  // namespace eib
