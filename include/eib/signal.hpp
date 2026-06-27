#pragma once
#include <cstdint>

#include "eib/order_book.hpp"
#include "eib/types.hpp"

namespace eib {

// Top of book snapshot used by the signals.
struct Bbo {
  bool  valid = false;
  Ticks bid_px = 0;
  Qty   bid_sz = 0;
  Ticks ask_px = 0;
  Qty   ask_sz = 0;
};

inline Bbo top_of_book(const OrderBook& b) {
  Bbo q;
  if (b.has_bids() && b.has_asks()) {
    q.valid = true;
    q.bid_px = b.best_bid();
    q.bid_sz = b.qty_at(Side::Buy, q.bid_px);
    q.ask_px = b.best_ask();
    q.ask_sz = b.qty_at(Side::Sell, q.ask_px);
  }
  return q;
}

// Order Flow Imbalance (Cont, Kukanov, Stoikov). This is a well-known, heavily
// published microstructure signal; it is used here as a known quantity to drive
// the honest-evaluation machine, not as a discovered edge. Each update returns
// the OFI increment from the previous best bid/ask and accumulates it. Positive
// OFI is net buying pressure and is associated with a short-horizon up move.
class OfiSignal {
 public:
  std::int64_t update(const Bbo& q) {
    if (!q.valid) return 0;
    std::int64_t inc = 0;
    if (has_prev_) {
      std::int64_t e_bid;
      if (q.bid_px > prev_.bid_px) e_bid = q.bid_sz;
      else if (q.bid_px == prev_.bid_px) e_bid = q.bid_sz - prev_.bid_sz;
      else e_bid = -prev_.bid_sz;

      std::int64_t e_ask;
      if (q.ask_px < prev_.ask_px) e_ask = q.ask_sz;
      else if (q.ask_px == prev_.ask_px) e_ask = q.ask_sz - prev_.ask_sz;
      else e_ask = -prev_.ask_sz;

      inc = e_bid - e_ask;
    }
    prev_ = q;
    has_prev_ = true;
    accum_ += inc;
    return inc;
  }

  std::int64_t ofi() const { return accum_; }
  void reset() { accum_ = 0; }

 private:
  Bbo prev_{};
  bool has_prev_ = false;
  std::int64_t accum_ = 0;
};

// Microprice direction without floating point. Returns +1 when the size-weighted
// fair value sits above the mid (up pressure), -1 below, 0 at the mid or when the
// book is one-sided. microprice = (bid*ask_sz + ask*bid_sz) / (bid_sz + ask_sz);
// comparing it to mid = (bid + ask) / 2 reduces exactly to the sign of
// (bid_sz - ask_sz) * (ask - bid), so whichever side rests more size wins.
inline int microprice_direction(const Bbo& q) {
  if (!q.valid) return 0;
  const std::int64_t tot = q.bid_sz + q.ask_sz;
  if (tot <= 0) return 0;
  const std::int64_t lhs =
      2 * (static_cast<std::int64_t>(q.bid_px) * q.ask_sz +
           static_cast<std::int64_t>(q.ask_px) * q.bid_sz);
  const std::int64_t rhs =
      (static_cast<std::int64_t>(q.bid_px) + q.ask_px) * tot;
  if (lhs > rhs) return 1;
  if (lhs < rhs) return -1;
  return 0;
}

}  // namespace eib
