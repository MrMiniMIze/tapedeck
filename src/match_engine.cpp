#include "eib/match_engine.hpp"

#include <algorithm>

namespace eib {

bool MatchEngine::post(OrderId id, Side side, Ticks price, Qty qty,
                       Qty queue_ahead, Timestamp /*ts*/) {
  if (qty <= 0 || queue_ahead < 0) return false;
  if (live_.find(id) != live_.end()) return false;
  for (const auto& [oid, r] : live_) {
    (void)oid;
    if (r.side == side && r.price == price) return false;  // v1: one per level
  }
  live_.emplace(id, Resting{side, price, qty, queue_ahead});
  return true;
}

bool MatchEngine::cancel(OrderId id) { return live_.erase(id) > 0; }

void MatchEngine::on_book_event(EventType type, Side side, Ticks price, Qty qty,
                                Timestamp ts, std::vector<Fill>& out) {
  for (auto it = live_.begin(); it != live_.end();) {
    Resting& o = it->second;
    bool erase = false;
    if (o.side == side && o.price == price) {
      switch (type) {
        case EventType::Add:
          break;  // a venue order joins behind us; our queue position is unchanged
        case EventType::Execute: {
          // Trades fill the front of the queue first: drain what is ahead of us,
          // then any remainder fills our order.
          const Qty drain = std::min(o.ahead, qty);
          o.ahead -= drain;
          const Qty rem = qty - drain;
          if (rem > 0 && o.remaining > 0) {
            const Qty fill = std::min(o.remaining, rem);
            o.remaining -= fill;
            out.push_back(Fill{it->first, o.side, o.price, fill, ts});
            if (o.remaining == 0) erase = true;
          }
          break;
        }
        case EventType::Cancel:
        case EventType::Reduce:
          // Whether the cancelled size was ahead of or behind us is unobservable.
          if (model_ == FillModel::Optimistic) o.ahead -= std::min(o.ahead, qty);
          // Conservative: assume it was behind us, so our position is unchanged.
          break;
        case EventType::Replace:
          break;  // decomposed by the backtester into Reduce(old) + Add(new)
      }
    }
    if (erase) it = live_.erase(it);
    else ++it;
  }
}

}  // namespace eib
