#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "eib/match_engine.hpp"

using namespace eib;

namespace {
Qty total_filled(const std::vector<Fill>& fills) {
  Qty q = 0;
  for (const auto& f : fills) q += f.qty;
  return q;
}
}  // namespace

TEST_CASE("a trade drains the queue ahead before filling us", "[match]") {
  MatchEngine m(FillModel::Conservative);
  std::vector<Fill> fills;
  REQUIRE(m.post(1, Side::Buy, 100, 10, /*queue_ahead=*/5, 0));

  m.on_book_event(EventType::Execute, Side::Buy, 100, 3, 1, fills);
  REQUIRE(fills.empty());  // 3 of the 5 ahead drained, nothing reaches us

  m.on_book_event(EventType::Execute, Side::Buy, 100, 4, 2, fills);
  // 2 left ahead drained, remaining 2 fills us
  REQUIRE(total_filled(fills) == 2);
  REQUIRE(m.live_count() == 1);  // 8 of our 10 still resting
}

TEST_CASE("a front-of-queue order fills immediately", "[match]") {
  MatchEngine m;
  std::vector<Fill> fills;
  REQUIRE(m.post(1, Side::Sell, 102, 4, /*queue_ahead=*/0, 0));
  m.on_book_event(EventType::Execute, Side::Sell, 102, 10, 1, fills);
  REQUIRE(total_filled(fills) == 4);  // fully filled (our 4)
  REQUIRE(m.live_count() == 0);       // removed once fully filled
}

TEST_CASE("conservative ignores cancels ahead; optimistic advances", "[match]") {
  std::vector<Fill> fills;

  MatchEngine cons(FillModel::Conservative);
  REQUIRE(cons.post(1, Side::Buy, 100, 10, 5, 0));
  cons.on_book_event(EventType::Cancel, Side::Buy, 100, 5, 1, fills);  // no advance
  cons.on_book_event(EventType::Execute, Side::Buy, 100, 1, 2, fills);
  REQUIRE(fills.empty());  // still 4 ahead under conservative

  fills.clear();
  MatchEngine opt(FillModel::Optimistic);
  REQUIRE(opt.post(1, Side::Buy, 100, 10, 5, 0));
  opt.on_book_event(EventType::Cancel, Side::Buy, 100, 3, 1, fills);   // ahead 5 -> 2
  opt.on_book_event(EventType::Execute, Side::Buy, 100, 3, 2, fills);  // drains 2, fills 1
  REQUIRE(total_filled(fills) == 1);
}

TEST_CASE("a venue add at our level does not affect our position", "[match]") {
  MatchEngine m;
  std::vector<Fill> fills;
  REQUIRE(m.post(1, Side::Buy, 100, 10, 5, 0));
  m.on_book_event(EventType::Add, Side::Buy, 100, 50, 1, fills);  // joins behind us
  m.on_book_event(EventType::Execute, Side::Buy, 100, 6, 2, fills);
  REQUIRE(total_filled(fills) == 1);  // 5 ahead drained, 1 fills us (add was irrelevant)
}

TEST_CASE("events at other levels are ignored", "[match]") {
  MatchEngine m;
  std::vector<Fill> fills;
  REQUIRE(m.post(1, Side::Buy, 100, 10, 0, 0));
  m.on_book_event(EventType::Execute, Side::Buy, 99, 100, 1, fills);   // different price
  m.on_book_event(EventType::Execute, Side::Sell, 100, 100, 2, fills); // different side
  REQUIRE(fills.empty());
}

TEST_CASE("post rejects duplicates and a second order at the same level",
          "[match]") {
  MatchEngine m;
  REQUIRE(m.post(1, Side::Buy, 100, 10, 0, 0));
  REQUIRE_FALSE(m.post(1, Side::Buy, 101, 5, 0, 0));   // duplicate id
  REQUIRE_FALSE(m.post(2, Side::Buy, 100, 5, 0, 0));   // same level
  REQUIRE_FALSE(m.post(3, Side::Buy, 100, 0, 0, 0));   // non-positive size
  REQUIRE(m.post(4, Side::Sell, 100, 5, 0, 0));        // same price, other side: ok
  REQUIRE(m.live_count() == 2);
  REQUIRE(m.cancel(1));
  REQUIRE_FALSE(m.cancel(999));
  REQUIRE(m.live_count() == 1);
}
