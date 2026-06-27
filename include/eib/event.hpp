#pragma once
#include "eib/types.hpp"

namespace eib {

// A normalized L3 / market-by-order event. The BookBuilder consumes a stream of
// these in timestamp order. POD by design: trivially copyable, no allocation,
// hot-path friendly. Concrete venue formats (ITCH, Databento
// DBN, Coinbase L3) are decoded into this neutral shape by the FeedParser.
enum class EventType : std::uint8_t {
  Add,      // a new resting order joins the book
  Cancel,   // a resting order is fully removed (e.g. ITCH "Order Delete")
  Reduce,   // a resting order's size is decreased (e.g. ITCH "Order Cancel", partial)
  Execute,  // a resting order trades against an aggressor; its size decreases
  Replace,  // a resting order is replaced by a new one (ITCH "Order Replace"):
            // old id removed, new id added at a new price/size, SAME side.
};

struct MarketEvent {
  EventType type{};
  Side      side{};     // meaningful for Add (ignored otherwise, lookups are by id)
  OrderId   id{};       // Add/Cancel/Reduce/Execute: the order; Replace: the OLD order
  Ticks     price{};    // meaningful for Add and Replace (integer ticks)
  Qty       qty{};      // Add/Replace: order size; Reduce/Execute: amount removed
  Timestamp ts{};
  OrderId   new_id{};   // meaningful for Replace only: the NEW order reference

  // Convenience constructors keep test/replay code readable.
  static MarketEvent add(OrderId id, Side side, Ticks price, Qty qty, Timestamp ts = 0) {
    return MarketEvent{EventType::Add, side, id, price, qty, ts};
  }
  static MarketEvent cancel(OrderId id, Timestamp ts = 0) {
    return MarketEvent{EventType::Cancel, Side::Buy, id, 0, 0, ts};
  }
  static MarketEvent reduce(OrderId id, Qty qty, Timestamp ts = 0) {
    return MarketEvent{EventType::Reduce, Side::Buy, id, 0, qty, ts};
  }
  static MarketEvent execute(OrderId id, Qty qty, Timestamp ts = 0) {
    return MarketEvent{EventType::Execute, Side::Buy, id, 0, qty, ts};
  }
  static MarketEvent replace(OrderId old_id, OrderId new_id, Ticks price, Qty qty,
                             Timestamp ts = 0) {
    return MarketEvent{EventType::Replace, Side::Buy, old_id, price, qty, ts, new_id};
  }
};

}  // namespace eib
