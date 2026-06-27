#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>

#include "eib/event.hpp"
#include "eib/hash.hpp"
#include "eib/types.hpp"

namespace eib {

// Obviously correct order book used as the test oracle for the flat production
// OrderBook (see tests/test_book_differential.cpp) and as a readable reference.
// Sorted std::map per side plus a hash of live orders. Not optimized; clarity is
// the whole point.
class ReferenceOrderBook {
 public:
  bool apply(const MarketEvent& ev) {
    switch (ev.type) {
      case EventType::Add: {
        if (ev.qty <= 0) return false;
        const bool inserted =
            orders_.emplace(ev.id, Order{ev.side, ev.price, ev.qty}).second;
        if (!inserted) return false;
        side_levels(ev.side)[ev.price] += ev.qty;
        return true;
      }
      case EventType::Cancel: {
        auto it = orders_.find(ev.id);
        if (it == orders_.end()) return false;
        remove_qty(it->second, it->second.qty);
        orders_.erase(it);
        return true;
      }
      case EventType::Reduce:
      case EventType::Execute: {
        if (ev.qty <= 0) return false;
        auto it = orders_.find(ev.id);
        if (it == orders_.end()) return false;
        if (ev.qty > it->second.qty) return false;
        remove_qty(it->second, ev.qty);
        it->second.qty -= ev.qty;
        if (it->second.qty == 0) orders_.erase(it);
        return true;
      }
      case EventType::Replace: {
        if (ev.qty <= 0) return false;
        const auto old_it = orders_.find(ev.id);
        if (old_it == orders_.end()) return false;
        if (orders_.find(ev.new_id) != orders_.end()) return false;
        const Side side = old_it->second.side;
        remove_qty(old_it->second, old_it->second.qty);
        orders_.erase(old_it);
        orders_.emplace(ev.new_id, Order{side, ev.price, ev.qty});
        side_levels(side)[ev.price] += ev.qty;
        return true;
      }
    }
    return false;
  }

  bool has_bids() const { return !bids_.empty(); }
  bool has_asks() const { return !asks_.empty(); }
  Ticks best_bid() const { return bids_.rbegin()->first; }
  Ticks best_ask() const { return asks_.begin()->first; }

  Qty qty_at(Side side, Ticks price) const {
    const auto& levels = (side == Side::Buy) ? bids_ : asks_;
    const auto it = levels.find(price);
    return (it == levels.end()) ? Qty{0} : it->second;
  }
  std::size_t level_count(Side side) const {
    return (side == Side::Buy ? bids_ : asks_).size();
  }
  std::size_t order_count() const { return orders_.size(); }

  std::uint64_t state_hash() const {
    std::uint64_t h = detail::kFnvOffset;
    for (const auto& [price, qty] : bids_) {
      detail::fnv(h, 1ull);
      detail::fnv(h, static_cast<std::uint64_t>(price));
      detail::fnv(h, static_cast<std::uint64_t>(qty));
    }
    for (const auto& [price, qty] : asks_) {
      detail::fnv(h, 2ull);
      detail::fnv(h, static_cast<std::uint64_t>(price));
      detail::fnv(h, static_cast<std::uint64_t>(qty));
    }
    return h;
  }

 private:
  struct Order {
    Side  side;
    Ticks price;
    Qty   qty;
  };
  std::map<Ticks, Qty>& side_levels(Side s) { return s == Side::Buy ? bids_ : asks_; }
  void remove_qty(const Order& o, Qty amount) {
    auto& levels = side_levels(o.side);
    auto it = levels.find(o.price);
    it->second -= amount;
    if (it->second <= 0) levels.erase(it);
  }

  std::map<Ticks, Qty> bids_;
  std::map<Ticks, Qty> asks_;
  std::unordered_map<OrderId, Order> orders_;
};

}  // namespace eib
