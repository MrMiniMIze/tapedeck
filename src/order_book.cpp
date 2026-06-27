#include "eib/order_book.hpp"

#include "eib/hash.hpp"

namespace eib {

OrderBook::OrderBook(std::size_t level_capacity, std::size_t order_capacity)
    : cap_(level_capacity),
      bid_qty_(level_capacity, 0),
      ask_qty_(level_capacity, 0),
      orders_(order_capacity) {}

bool OrderBook::in_window(Ticks price, std::size_t& idx) const {
  if (!based_) return false;
  const Ticks off = price - base_;
  if (off < 0 || static_cast<std::size_t>(off) >= cap_) return false;
  idx = static_cast<std::size_t>(off);
  return true;
}

void OrderBook::add_level_qty(Side side, std::size_t idx, Qty qty) {
  if (side == Side::Buy) {
    if (bid_qty_[idx] == 0) {
      ++bid_levels_;
      if (bid_levels_ == 1 || idx > best_bid_idx_) best_bid_idx_ = idx;
    }
    bid_qty_[idx] += qty;
  } else {
    if (ask_qty_[idx] == 0) {
      ++ask_levels_;
      if (ask_levels_ == 1 || idx < best_ask_idx_) best_ask_idx_ = idx;
    }
    ask_qty_[idx] += qty;
  }
}

void OrderBook::rescan_best_bid() {
  // The emptied level was the highest bid, so the next best is strictly below it.
  std::size_t i = best_bid_idx_;
  while (i > 0) {
    --i;
    if (bid_qty_[i] != 0) {
      best_bid_idx_ = i;
      return;
    }
  }
}

void OrderBook::rescan_best_ask() {
  // The emptied level was the lowest ask, so the next best is strictly above it.
  for (std::size_t i = best_ask_idx_ + 1; i < cap_; ++i) {
    if (ask_qty_[i] != 0) {
      best_ask_idx_ = i;
      return;
    }
  }
}

void OrderBook::remove_level_qty(Side side, std::size_t idx, Qty amount) {
  if (side == Side::Buy) {
    bid_qty_[idx] -= amount;
    if (bid_qty_[idx] == 0) {
      --bid_levels_;
      if (bid_levels_ > 0 && idx == best_bid_idx_) rescan_best_bid();
    }
  } else {
    ask_qty_[idx] -= amount;
    if (ask_qty_[idx] == 0) {
      --ask_levels_;
      if (ask_levels_ > 0 && idx == best_ask_idx_) rescan_best_ask();
    }
  }
}

bool OrderBook::apply(const MarketEvent& ev) {
  switch (ev.type) {
    case EventType::Add: {
      if (ev.qty <= 0) return false;
      if (!based_) {
        base_ = ev.price - static_cast<Ticks>(cap_ / 2);
        based_ = true;
      }
      std::size_t idx;
      if (!in_window(ev.price, idx)) return false;       // out of window
      if (orders_.find(ev.id)) return false;             // duplicate id
      if (!orders_.insert(ev.id, ev.side, ev.price, ev.qty)) return false;  // full
      add_level_qty(ev.side, idx, ev.qty);
      return true;
    }
    case EventType::Cancel: {
      auto* e = orders_.find(ev.id);
      if (!e) return false;
      std::size_t idx;
      in_window(e->price, idx);  // a resting order is always inside the window
      remove_level_qty(e->side, idx, e->qty);
      orders_.erase(ev.id);
      return true;
    }
    case EventType::Reduce:
    case EventType::Execute: {
      if (ev.qty <= 0) return false;
      auto* e = orders_.find(ev.id);
      if (!e) return false;
      if (ev.qty > e->qty) return false;
      std::size_t idx;
      in_window(e->price, idx);
      remove_level_qty(e->side, idx, ev.qty);
      e->qty -= ev.qty;
      if (e->qty == 0) orders_.erase(ev.id);
      return true;
    }
    case EventType::Replace: {
      if (ev.qty <= 0) return false;
      auto* e = orders_.find(ev.id);
      if (!e) return false;
      if (orders_.find(ev.new_id)) return false;  // new id already live
      const Side side = e->side;
      const Qty old_qty = e->qty;
      std::size_t old_idx;
      in_window(e->price, old_idx);
      std::size_t new_idx;
      if (!in_window(ev.price, new_idx)) return false;  // reject before mutating
      remove_level_qty(side, old_idx, old_qty);
      orders_.erase(ev.id);
      orders_.insert(ev.new_id, side, ev.price, ev.qty);
      add_level_qty(side, new_idx, ev.qty);
      return true;
    }
  }
  return false;
}

Qty OrderBook::qty_at(Side side, Ticks price) const {
  std::size_t idx;
  if (!in_window(price, idx)) return 0;
  return side == Side::Buy ? bid_qty_[idx] : ask_qty_[idx];
}

std::uint64_t OrderBook::state_hash() const {
  std::uint64_t h = detail::kFnvOffset;
  for (std::size_t i = 0; i < cap_; ++i) {
    if (bid_qty_[i] != 0) {
      detail::fnv(h, 1ull);
      detail::fnv(h, static_cast<std::uint64_t>(base_ + static_cast<Ticks>(i)));
      detail::fnv(h, static_cast<std::uint64_t>(bid_qty_[i]));
    }
  }
  for (std::size_t i = 0; i < cap_; ++i) {
    if (ask_qty_[i] != 0) {
      detail::fnv(h, 2ull);
      detail::fnv(h, static_cast<std::uint64_t>(base_ + static_cast<Ticks>(i)));
      detail::fnv(h, static_cast<std::uint64_t>(ask_qty_[i]));
    }
  }
  return h;
}

}  // namespace eib
