#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "eib/backtester.hpp"

using namespace eib;

namespace {

// Build a frictionless synthetic stream whose best bid and best ask both sit at
// P[t] each tick (zero spread), so mid moves cleanly through the given path. Per
// tick: cancel the previous two orders, then add an ask and a bid at P[t]. The
// only event that leaves the book two-sided is the bid add, so a strategy that
// acts when two_sided() is true acts exactly once per tick, in order.
std::vector<MarketEvent> build_stream(const std::vector<Ticks>& P) {
  std::vector<MarketEvent> s;
  OrderId id = 1, prev_bid = 0, prev_ask = 0;
  for (std::size_t t = 0; t < P.size(); ++t) {
    if (t > 0) {
      s.push_back(MarketEvent::cancel(prev_bid));
      s.push_back(MarketEvent::cancel(prev_ask));
    }
    const OrderId ask_id = id++;
    const OrderId bid_id = id++;
    s.push_back(MarketEvent::add(ask_id, Side::Sell, P[t], 100));
    s.push_back(MarketEvent::add(bid_id, Side::Buy, P[t], 100));
    prev_ask = ask_id;
    prev_bid = bid_id;
  }
  return s;
}

// Causal momentum: trades on the move just observed. No future access.
struct HonestMomentum {
  Ticks prev = -1;
  void on_event(Backtester& bt, const MarketEvent&) {
    if (!bt.two_sided()) return;
    const Ticks m = bt.mid2();
    if (prev >= 0) {
      if (m > prev) bt.set_target(1);
      else if (m < prev) bt.set_target(-1);
    }
    prev = m;
  }
};

// Smuggles the future price series in (a lookahead bug) and trades on the NEXT
// move. This is what a leaked future looks like.
struct CheatForesight {
  const std::vector<Ticks>* path;
  std::size_t t = 0;
  void on_event(Backtester& bt, const MarketEvent&) {
    if (!bt.two_sided()) return;
    if (t + 1 < path->size()) {
      const Ticks cur = (*path)[t];
      const Ticks nxt = (*path)[t + 1];
      if (nxt > cur) bt.set_target(1);
      else if (nxt < cur) bt.set_target(-1);
      else bt.set_target(0);
    } else {
      bt.set_target(0);
    }
    ++t;
  }
};

}  // namespace

TEST_CASE("lookahead oracle: foresight prints money, the honest path does not",
          "[backtester][lookahead]") {
  std::vector<Ticks> path;
  for (int i = 0; i < 20; ++i) path.push_back((i % 2) ? 110 : 100);  // mean reverting
  const auto stream = build_stream(path);

  Backtester honest_bt;
  HonestMomentum honest;
  honest_bt.run(stream, honest);

  Backtester cheat_bt;
  CheatForesight cheat{&path, 0};
  cheat_bt.run(stream, cheat);

  // With the future, the strategy is strongly profitable; without it, the same
  // shape of strategy loses on the mean-reverting path. The gap is the value of
  // the no-lookahead guarantee.
  REQUIRE(cheat_bt.equity2() > 0);
  REQUIRE(honest_bt.equity2() < 0);
  REQUIRE(cheat_bt.equity2() > honest_bt.equity2() + 500);
}

namespace {
// Posts one passive buy at 100 the first time the book shows size there.
struct PassiveBuyer {
  bool posted = false;
  void on_event(Backtester& bt, const MarketEvent&) {
    if (!posted && bt.book().qty_at(Side::Buy, 100) >= 5) {
      bt.post_passive(1, Side::Buy, 100, 3);
      posted = true;
    }
  }
};
}  // namespace

TEST_CASE("backtester settles passive fills from queue drain", "[backtester]") {
  Backtester bt(1u << 16, 1u << 16, FillModel::Conservative);
  PassiveBuyer strat;
  // Venue activity at our level drains the queue ahead of us, then fills us.
  const std::vector<MarketEvent> stream = {
      MarketEvent::add(10, Side::Buy, 100, 5),   // we join behind this (ahead = 5)
      MarketEvent::add(20, Side::Sell, 102, 5),  // make the book two-sided
      MarketEvent::add(11, Side::Buy, 100, 4),   // joins behind us
      MarketEvent::execute(10, 5),               // drains our 5 ahead, no fill yet
      MarketEvent::execute(11, 4),               // ahead is 0, this fills our 3
  };
  bt.run(stream, strat);
  REQUIRE(bt.portfolio().position == 3);
  REQUIRE(bt.portfolio().filled_shares == 3);
}
