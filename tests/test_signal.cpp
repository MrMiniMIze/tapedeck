#include <catch2/catch_test_macros.hpp>

#include "eib/order_book.hpp"
#include "eib/signal.hpp"

using namespace eib;

namespace {
Bbo bbo(Ticks bp, Qty bs, Ticks ap, Qty as) { return Bbo{true, bp, bs, ap, as}; }
}  // namespace

TEST_CASE("first OFI update has no increment", "[signal][ofi]") {
  OfiSignal s;
  REQUIRE(s.update(bbo(100, 10, 102, 10)) == 0);
  REQUIRE(s.ofi() == 0);
}

TEST_CASE("growing bid size is positive OFI; growing ask size is negative",
          "[signal][ofi]") {
  OfiSignal s;
  s.update(bbo(100, 10, 102, 10));
  REQUIRE(s.update(bbo(100, 15, 102, 10)) == 5);   // +5 bid size at same bid
  REQUIRE(s.update(bbo(100, 15, 102, 14)) == -4);  // +4 ask size at same ask
  REQUIRE(s.ofi() == 1);                           // 5 - 4 accumulated
}

TEST_CASE("a higher bid is strong buying pressure", "[signal][ofi]") {
  OfiSignal s;
  s.update(bbo(100, 10, 102, 10));
  // bid price up to 101 with size 8: e_bid = +8; ask unchanged: e_ask = 0
  REQUIRE(s.update(bbo(101, 8, 102, 10)) == 8);
}

TEST_CASE("a lower bid is selling pressure", "[signal][ofi]") {
  OfiSignal s;
  s.update(bbo(100, 10, 102, 10));
  // bid price down: e_bid = -old_bid_sz = -10; ask unchanged: e_ask = 0
  REQUIRE(s.update(bbo(99, 7, 102, 10)) == -10);
}

TEST_CASE("microprice leans toward the heavier side", "[signal][microprice]") {
  REQUIRE(microprice_direction(bbo(100, 30, 101, 10)) == 1);   // more bid size: up
  REQUIRE(microprice_direction(bbo(100, 10, 101, 30)) == -1);  // more ask size: down
  REQUIRE(microprice_direction(bbo(100, 20, 101, 20)) == 0);   // balanced
  REQUIRE(microprice_direction(Bbo{}) == 0);                   // one-sided / invalid
}

TEST_CASE("top_of_book reads the book's best levels", "[signal]") {
  OrderBook b;
  b.apply(MarketEvent::add(1, Side::Buy, 100, 12));
  b.apply(MarketEvent::add(2, Side::Sell, 103, 7));
  const Bbo q = top_of_book(b);
  REQUIRE(q.valid);
  REQUIRE(q.bid_px == 100);
  REQUIRE(q.bid_sz == 12);
  REQUIRE(q.ask_px == 103);
  REQUIRE(q.ask_sz == 7);
  REQUIRE(microprice_direction(q) == 1);  // heavier bid: up pressure
}
