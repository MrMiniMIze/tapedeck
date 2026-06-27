#include "eib/order_book.hpp"

namespace eib {

namespace {
// FNV-1a 64-bit, fed byte-by-byte so the digest is endian-stable.
constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

inline void fnv(std::uint64_t& h, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    h ^= (v & 0xffu);
    h *= kFnvPrime;
    v >>= 8;
  }
}
}  // namespace

void OrderBook::remove_qty(const Order& o, Qty amount) {
  auto& levels = side_levels(o.side);
  auto it = levels.find(o.price);
  // Invariant: a resting order's level always exists while the order does.
  it->second -= amount;
  if (it->second <= 0) levels.erase(it);
}

bool OrderBook::apply(const MarketEvent& ev) {
  switch (ev.type) {
    case EventType::Add: {
      if (ev.qty <= 0) return false;  // reject non-positive sizes
      const bool inserted =
          orders_.emplace(ev.id, Order{ev.side, ev.price, ev.qty}).second;
      if (!inserted) return false;  // duplicate id
      side_levels(ev.side)[ev.price] += ev.qty;
      return true;
    }
    case EventType::Cancel: {
      auto it = orders_.find(ev.id);
      if (it == orders_.end()) return false;  // unknown id
      remove_qty(it->second, it->second.qty);
      orders_.erase(it);
      return true;
    }
    case EventType::Reduce:
    case EventType::Execute: {
      if (ev.qty <= 0) return false;
      auto it = orders_.find(ev.id);
      if (it == orders_.end()) return false;       // unknown id
      if (ev.qty > it->second.qty) return false;   // over-reduce: reject, no mutation
      remove_qty(it->second, ev.qty);
      it->second.qty -= ev.qty;
      if (it->second.qty == 0) orders_.erase(it);
      return true;
    }
    case EventType::Replace: {
      if (ev.qty <= 0) return false;
      const auto old_it = orders_.find(ev.id);
      if (old_it == orders_.end()) return false;          // unknown original id
      if (orders_.find(ev.new_id) != orders_.end()) return false;  // new id already live
      const Side side = old_it->second.side;              // Replace inherits the side
      remove_qty(old_it->second, old_it->second.qty);
      orders_.erase(old_it);
      orders_.emplace(ev.new_id, Order{side, ev.price, ev.qty});
      side_levels(side)[ev.price] += ev.qty;
      return true;
    }
  }
  return false;
}

Qty OrderBook::qty_at(Side side, Ticks price) const {
  const auto& levels = (side == Side::Buy) ? bids_ : asks_;
  const auto it = levels.find(price);
  return (it == levels.end()) ? Qty{0} : it->second;
}

std::size_t OrderBook::level_count(Side side) const {
  return (side == Side::Buy ? bids_ : asks_).size();
}

std::uint64_t OrderBook::state_hash() const {
  std::uint64_t h = kFnvOffset;
  for (const auto& [price, qty] : bids_) {
    fnv(h, 1ull);
    fnv(h, static_cast<std::uint64_t>(price));
    fnv(h, static_cast<std::uint64_t>(qty));
  }
  for (const auto& [price, qty] : asks_) {
    fnv(h, 2ull);
    fnv(h, static_cast<std::uint64_t>(price));
    fnv(h, static_cast<std::uint64_t>(qty));
  }
  return h;
}

}  // namespace eib
