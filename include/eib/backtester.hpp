#pragma once
#include <cstdint>
#include <vector>

#include "eib/event.hpp"
#include "eib/match_engine.hpp"
#include "eib/order_book.hpp"
#include "eib/types.hpp"

namespace eib {

// Integer portfolio accounting. cash is in (ticks * shares) and goes negative
// while long; position is signed shares.
struct Portfolio {
  Qty          position = 0;
  std::int64_t cash = 0;
  Qty          filled_shares = 0;
};

// Event-driven backtester. It drives ONE timestamp-ordered normalized event
// stream: for each event it (1) resolves the level info and lets the MatchEngine
// drain our queue position and settle passive fills, (2) applies the event to
// the BookBuilder, then (3) hands the UPDATED book to the strategy. The strategy
// only ever sees state up to the current event, so there is no lookahead by
// construction. The lookahead oracle test injects future information to show the
// contrast.
//
// Strategy is a template type with `on_event(Backtester&, const MarketEvent&)`,
// so there is no virtual dispatch on the per-event path.
class Backtester {
 public:
  explicit Backtester(std::size_t level_capacity = 1u << 16,
                      std::size_t order_capacity = 1u << 16,
                      FillModel model = FillModel::Conservative,
                      Ticks center = 0)
      : book_(level_capacity, order_capacity, center), matcher_(model) {}

  const OrderBook& book() const { return book_; }
  const Portfolio& portfolio() const { return port_; }

  bool two_sided() const { return book_.has_bids() && book_.has_asks(); }
  // 2 * mid, kept integral to avoid fractional ticks (precondition: two_sided()).
  Ticks mid2() const { return book_.best_bid() + book_.best_ask(); }

  // Aggressive immediate fill at the touch.
  void market(Side side, Qty qty) {
    if (qty <= 0) return;
    if (side == Side::Buy) {
      if (!book_.has_asks()) return;
      const Ticks px = book_.best_ask();
      port_.position += qty;
      port_.cash -= static_cast<std::int64_t>(px) * qty;
      port_.filled_shares += qty;
    } else {
      if (!book_.has_bids()) return;
      const Ticks px = book_.best_bid();
      port_.position -= qty;
      port_.cash += static_cast<std::int64_t>(px) * qty;
      port_.filled_shares += qty;
    }
  }

  // Trade to a target net position using marketable orders.
  void set_target(Qty target) {
    const Qty d = target - port_.position;
    if (d > 0) market(Side::Buy, d);
    else if (d < 0) market(Side::Sell, -d);
  }

  // Rest a passive order; queue_ahead is the venue size already at that level.
  bool post_passive(OrderId id, Side side, Ticks price, Qty qty) {
    return matcher_.post(id, side, price, qty, book_.qty_at(side, price), 0);
  }
  bool cancel_passive(OrderId id) { return matcher_.cancel(id); }
  bool passive_live(OrderId id) const { return matcher_.is_live(id); }

  // Mark-to-market equity in 2 * (ticks * shares) units.
  std::int64_t equity2() const {
    std::int64_t eq = 2 * port_.cash;
    if (two_sided()) eq += static_cast<std::int64_t>(mid2()) * port_.position;
    return eq;
  }

  template <class Strategy>
  void run(const std::vector<MarketEvent>& stream, Strategy& strat) {
    for (const auto& ev : stream) {
      if (!matcher_.empty()) notify_matcher(ev);
      book_.apply(ev);
      strat.on_event(*this, ev);
    }
  }

 private:
  void notify_matcher(const MarketEvent& ev) {
    fills_.clear();
    switch (ev.type) {
      case EventType::Add:
        matcher_.on_book_event(EventType::Add, ev.side, ev.price, ev.qty, ev.ts, fills_);
        break;
      case EventType::Cancel: {
        Side s{};
        Ticks p = 0;
        Qty q = 0;
        if (book_.order_info(ev.id, s, p, q))
          matcher_.on_book_event(EventType::Cancel, s, p, q, ev.ts, fills_);
        break;
      }
      case EventType::Reduce:
      case EventType::Execute: {
        Side s{};
        Ticks p = 0;
        Qty q = 0;
        if (book_.order_info(ev.id, s, p, q))
          matcher_.on_book_event(ev.type, s, p, ev.qty, ev.ts, fills_);
        break;
      }
      case EventType::Replace: {
        Side s{};
        Ticks p = 0;
        Qty q = 0;
        if (book_.order_info(ev.id, s, p, q)) {
          matcher_.on_book_event(EventType::Reduce, s, p, q, ev.ts, fills_);
          matcher_.on_book_event(EventType::Add, s, ev.price, ev.qty, ev.ts, fills_);
        }
        break;
      }
    }
    settle();
  }

  void settle() {
    for (const auto& f : fills_) {
      if (f.side == Side::Buy) {
        port_.position += f.qty;
        port_.cash -= static_cast<std::int64_t>(f.price) * f.qty;
      } else {
        port_.position -= f.qty;
        port_.cash += static_cast<std::int64_t>(f.price) * f.qty;
      }
      port_.filled_shares += f.qty;
    }
  }

  OrderBook book_;
  MatchEngine matcher_;
  Portfolio port_;
  std::vector<Fill> fills_;
};

}  // namespace eib
