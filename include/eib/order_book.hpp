#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "eib/event.hpp"
#include "eib/types.hpp"

namespace eib {

namespace detail {

// Fixed-capacity open-addressing hash from order id to its resting record.
// Linear probing with backward-shift deletion (no tombstones, so heavy churn
// over a full session never degrades it). Sized at construction; never allocates
// afterward, which is what keeps OrderBook's steady-state path allocation free.
class IdIndex {
 public:
  struct Entry {
    OrderId id;
    Side    side;
    Ticks   price;
    Qty     qty;
  };

  explicit IdIndex(std::size_t capacity) : limit_(capacity) {
    std::size_t n = 16;
    while (n < capacity * 2) n <<= 1;  // load factor <= 0.5, always has empties
    mask_ = n - 1;
    slots_.resize(n);
    used_.assign(n, 0);
  }

  std::size_t size() const { return size_; }

  Entry* find(OrderId id) {
    std::size_t i = home(id);
    while (used_[i]) {
      if (slots_[i].id == id) return &slots_[i];
      i = (i + 1) & mask_;
    }
    return nullptr;
  }

  // Returns the inserted entry, or nullptr if the id is already present or the
  // logical capacity is reached (caller treats nullptr as a loud failure).
  Entry* insert(OrderId id, Side side, Ticks price, Qty qty) {
    if (size_ >= limit_) return nullptr;
    std::size_t i = home(id);
    while (used_[i]) {
      if (slots_[i].id == id) return nullptr;
      i = (i + 1) & mask_;
    }
    slots_[i] = Entry{id, side, price, qty};
    used_[i] = 1;
    ++size_;
    return &slots_[i];
  }

  bool erase(OrderId id) {
    std::size_t i = home(id);
    while (used_[i] && slots_[i].id != id) i = (i + 1) & mask_;
    if (!used_[i]) return false;
    used_[i] = 0;
    --size_;
    // Backward-shift: pull up any later entry whose home is not inside (i, j].
    std::size_t j = i;
    for (;;) {
      j = (j + 1) & mask_;
      if (!used_[j]) break;
      const std::size_t k = home(slots_[j].id);
      const bool in_ij = (i <= j) ? (i < k && k <= j) : (k > i || k <= j);
      if (in_ij) continue;  // correctly placed relative to the hole; leave it
      slots_[i] = slots_[j];
      used_[i] = 1;
      used_[j] = 0;
      i = j;
    }
    return true;
  }

 private:
  std::size_t home(OrderId id) const {
    // splitmix64 finalizer: order ids are often sequential, so mix before masking
    std::uint64_t x = id;
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ull;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebull;
    x ^= x >> 31;
    return static_cast<std::size_t>(x) & mask_;
  }

  std::vector<Entry> slots_;
  std::vector<unsigned char> used_;
  std::size_t mask_ = 0;
  std::size_t limit_ = 0;
  std::size_t size_ = 0;
};

}  // namespace detail

// Flat-array BookBuilder: the production order book. Price levels live in two
// flat arrays indexed by integer tick offset from a lazily chosen base, giving
// cache-friendly O(1) updates and O(1) best-bid/ask reads. Order lookup is the
// custom IdIndex above. Same public contract as ReferenceOrderBook, so the two
// are compared directly in the differential test. Per-order FIFO queue position
// is intentionally NOT tracked here; it lands in Phase 2 with the MatchEngine,
// where queue position is actually consumed for fills.
//
// A price outside the window [base, base + level_capacity) is rejected (apply
// returns false) rather than silently corrupting the book. For real data, size
// the window to the instrument's plausible band (the replay tool does this).
class OrderBook {
 public:
  explicit OrderBook(std::size_t level_capacity = 1u << 16,
                     std::size_t order_capacity = 1u << 16);

  bool apply(const MarketEvent& ev);

  bool has_bids() const { return bid_levels_ > 0; }
  bool has_asks() const { return ask_levels_ > 0; }

  // Preconditions: has_bids() / has_asks() respectively.
  Ticks best_bid() const { return base_ + static_cast<Ticks>(best_bid_idx_); }
  Ticks best_ask() const { return base_ + static_cast<Ticks>(best_ask_idx_); }

  Qty qty_at(Side side, Ticks price) const;
  std::size_t level_count(Side side) const {
    return side == Side::Buy ? bid_levels_ : ask_levels_;
  }
  std::size_t order_count() const { return orders_.size(); }

  std::uint64_t state_hash() const;

 private:
  bool in_window(Ticks price, std::size_t& idx) const;
  // For prices already known to be in the window (a resting order's own price).
  std::size_t index_of(Ticks price) const {
    return static_cast<std::size_t>(price - base_);
  }
  void add_level_qty(Side side, std::size_t idx, Qty qty);
  void remove_level_qty(Side side, std::size_t idx, Qty amount);
  void rescan_best_bid();
  void rescan_best_ask();

  std::size_t cap_;
  Ticks base_ = 0;
  bool based_ = false;
  std::vector<Qty> bid_qty_;
  std::vector<Qty> ask_qty_;
  std::size_t bid_levels_ = 0;
  std::size_t ask_levels_ = 0;
  std::size_t best_bid_idx_ = 0;  // valid when bid_levels_ > 0
  std::size_t best_ask_idx_ = 0;  // valid when ask_levels_ > 0
  detail::IdIndex orders_;
};

}  // namespace eib
