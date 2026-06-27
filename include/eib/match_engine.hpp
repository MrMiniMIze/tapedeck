#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "eib/event.hpp"
#include "eib/types.hpp"

namespace eib {

// How a cancel ahead of us in the queue is treated. Real fills sit between these
// two extremes, so Phase 3 sweeps this parameter to draw the kill curve.
enum class FillModel : std::uint8_t {
  Conservative,  // a cancel at our level is assumed to be BEHIND us (never helps);
                 // we advance only when real trades execute through our position
  Optimistic,    // a cancel at our level is assumed to be AHEAD of us (advances us)
};

struct Fill {
  OrderId   id;
  Side      side;
  Ticks     price;
  Qty       qty;
  Timestamp ts;
};

// Matches OUR simulated orders against the venue's reconstructed book. This is
// NOT the BookBuilder: the BookBuilder rebuilds the venue book from the feed,
// while MatchEngine models only our own orders' queue position and fills. The
// backtester resolves each venue event's (side, price, qty delta) from the
// BookBuilder, then calls on_book_event so the matcher can drain our queue
// position and emit fills.
//
// These are backtest-time structures, not the zero-allocation replay hot path;
// the backtester gates calls with empty() so this runs only while we hold
// resting orders. v1 models one of our orders per (side, price) level.
class MatchEngine {
 public:
  explicit MatchEngine(FillModel model = FillModel::Conservative) : model_(model) {}

  bool empty() const { return live_.empty(); }
  std::size_t live_count() const { return live_.size(); }
  bool is_live(OrderId id) const { return live_.find(id) != live_.end(); }

  // Post one of our resting limit orders at (side, price) for qty. queue_ahead
  // is the venue qty already resting at that level when we join, i.e. our place
  // in line. Returns false on a duplicate id, on a non-positive size or negative
  // queue, or if we already rest at that (side, price).
  bool post(OrderId id, Side side, Ticks price, Qty qty, Qty queue_ahead,
            Timestamp ts);

  // Cancel one of our resting orders. Returns false if it is unknown.
  bool cancel(OrderId id);

  // Observe a venue book event already resolved to (type, side, price, qty),
  // where qty is the size that joined (Add) or left (Cancel/Reduce/Execute) that
  // level. Appends any resulting fills of our orders to out. Replace is expected
  // to be decomposed by the backtester into Reduce(old) plus Add(new).
  void on_book_event(EventType type, Side side, Ticks price, Qty qty,
                     Timestamp ts, std::vector<Fill>& out);

 private:
  struct Resting {
    Side  side;
    Ticks price;
    Qty   remaining;  // our unfilled size
    Qty   ahead;      // venue qty still in front of us at this level
  };

  FillModel model_;
  std::unordered_map<OrderId, Resting> live_;  // our id -> resting state
};

}  // namespace eib
