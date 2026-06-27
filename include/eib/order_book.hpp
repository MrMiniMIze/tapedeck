#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>

#include "eib/event.hpp"
#include "eib/types.hpp"

namespace eib {

// BookBuilder: reconstructs the venue's resting limit order book from a stream
// of normalized L3/MBO events. It does NOT match our own orders, that is the
// MatchEngine's job in Phase 2.
//
// Phase 1a reference: sorted std::map<price, qty> per side + hash<id -> order>.
// Correct and simple. best_bid()/best_ask() reads are O(1) (map begin/rbegin);
// level inserts and cancels are O(log L). Phase 1b will swap the internals to a
// flat tick-indexed array + intrusive FIFO lists (O(1) everywhere, cache-
// friendly) behind THIS SAME interface, guarded by the same tests, with a
// before/after benchmark. The public contract here does not change.
class OrderBook {
 public:
  // Apply one normalized event in arrival order. Returns false on an
  // inconsistent event (unknown id, duplicate id, over-reduce, non-positive
  // add) WITHOUT mutating the book, so callers can fail loudly rather than let
  // the book silently desync from the venue (the Phase 1 determinism oracle).
  bool apply(const MarketEvent& ev);

  bool has_bids() const { return !bids_.empty(); }
  bool has_asks() const { return !asks_.empty(); }

  // Preconditions: has_bids() / has_asks() respectively.
  Ticks best_bid() const { return bids_.rbegin()->first; }
  Ticks best_ask() const { return asks_.begin()->first; }

  Qty qty_at(Side side, Ticks price) const;
  std::size_t level_count(Side side) const;
  std::size_t order_count() const { return orders_.size(); }

  // Deterministic 64-bit digest of the book state, for the determinism oracle
  // and golden-master diffs. Depends ONLY on the sorted (price, qty) per side,
  // never on hash-map layout or pointers, so it is stable across runs and
  // platforms.
  std::uint64_t state_hash() const;

 private:
  struct Order {
    Side  side;
    Ticks price;
    Qty   qty;
  };

  std::map<Ticks, Qty>& side_levels(Side s) { return s == Side::Buy ? bids_ : asks_; }
  void remove_qty(const Order& o, Qty amount);

  std::map<Ticks, Qty> bids_;  // price -> aggregate resting qty (sorted)
  std::map<Ticks, Qty> asks_;
  std::unordered_map<OrderId, Order> orders_;  // id -> resting order (O(1) cancel)
};

}  // namespace eib
