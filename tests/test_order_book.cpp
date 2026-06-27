#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "eib/order_book.hpp"

using namespace eib;

TEST_CASE("add builds levels and top of book", "[book]") {
  OrderBook b;
  REQUIRE(b.apply(MarketEvent::add(1, Side::Buy, 100, 10)));
  REQUIRE(b.apply(MarketEvent::add(2, Side::Sell, 102, 5)));
  REQUIRE(b.has_bids());
  REQUIRE(b.has_asks());
  REQUIRE(b.best_bid() == 100);
  REQUIRE(b.best_ask() == 102);
  REQUIRE(b.best_bid() < b.best_ask());  // spread invariant
  REQUIRE(b.qty_at(Side::Buy, 100) == 10);
  REQUIRE(b.order_count() == 2);
}

TEST_CASE("best bid improves with a higher buy", "[book]") {
  OrderBook b;
  b.apply(MarketEvent::add(1, Side::Buy, 100, 10));
  b.apply(MarketEvent::add(2, Side::Buy, 101, 7));
  REQUIRE(b.best_bid() == 101);
  REQUIRE(b.qty_at(Side::Buy, 101) == 7);
  REQUIRE(b.level_count(Side::Buy) == 2);
}

TEST_CASE("aggregate qty at a level sums multiple orders", "[book]") {
  OrderBook b;
  b.apply(MarketEvent::add(1, Side::Buy, 100, 10));
  b.apply(MarketEvent::add(2, Side::Buy, 100, 4));
  REQUIRE(b.qty_at(Side::Buy, 100) == 14);
  REQUIRE(b.level_count(Side::Buy) == 1);
}

TEST_CASE("cancel removes the order and empties the level", "[book]") {
  OrderBook b;
  b.apply(MarketEvent::add(1, Side::Buy, 100, 10));
  REQUIRE(b.apply(MarketEvent::cancel(1)));
  REQUIRE_FALSE(b.has_bids());
  REQUIRE(b.qty_at(Side::Buy, 100) == 0);
  REQUIRE(b.order_count() == 0);
}

TEST_CASE("reduce decreases size; full reduce removes the order", "[book]") {
  OrderBook b;
  b.apply(MarketEvent::add(1, Side::Sell, 102, 10));
  REQUIRE(b.apply(MarketEvent::reduce(1, 4)));
  REQUIRE(b.qty_at(Side::Sell, 102) == 6);
  REQUIRE(b.apply(MarketEvent::reduce(1, 6)));
  REQUIRE_FALSE(b.has_asks());
}

TEST_CASE("execute decreases resting size", "[book]") {
  OrderBook b;
  b.apply(MarketEvent::add(1, Side::Sell, 102, 10));
  REQUIRE(b.apply(MarketEvent::execute(1, 3)));
  REQUIRE(b.qty_at(Side::Sell, 102) == 7);
}

TEST_CASE("inconsistent events fail loudly without desync", "[book]") {
  OrderBook b;
  REQUIRE(b.apply(MarketEvent::add(1, Side::Buy, 100, 10)));
  REQUIRE_FALSE(b.apply(MarketEvent::add(1, Side::Buy, 100, 5)));  // duplicate id
  REQUIRE_FALSE(b.apply(MarketEvent::cancel(999)));                // unknown id
  REQUIRE_FALSE(b.apply(MarketEvent::reduce(1, 999)));             // over-reduce
  REQUIRE_FALSE(b.apply(MarketEvent::add(2, Side::Buy, 100, 0)));  // non-positive size
  // The book is unchanged by the rejected events.
  REQUIRE(b.qty_at(Side::Buy, 100) == 10);
  REQUIRE(b.order_count() == 1);
}

TEST_CASE("replace moves an order to a new id/price/size, inheriting side",
          "[book]") {
  OrderBook b;
  b.apply(MarketEvent::add(1, Side::Buy, 100, 10));
  REQUIRE(b.apply(MarketEvent::replace(1, 2, 101, 8)));
  REQUIRE(b.best_bid() == 101);
  REQUIRE(b.qty_at(Side::Buy, 101) == 8);
  REQUIRE(b.qty_at(Side::Buy, 100) == 0);
  REQUIRE(b.order_count() == 1);
  REQUIRE_FALSE(b.apply(MarketEvent::replace(999, 3, 100, 5)));  // unknown original
}

TEST_CASE("out-of-window prices are rejected without corrupting the book",
          "[book][window]") {
  const std::size_t cap = 256;
  OrderBook b(cap, 256);
  // The first add fixes base = 1000 - cap/2 = 872; the window is [872, 1128).
  REQUIRE(b.apply(MarketEvent::add(1, Side::Buy, 1000, 5)));
  const Ticks base = 1000 - static_cast<Ticks>(cap / 2);

  // Both window edges must be accepted.
  REQUIRE(b.apply(MarketEvent::add(2, Side::Buy, base, 1)));                       // 872
  REQUIRE(b.apply(MarketEvent::add(3, Side::Sell, base + static_cast<Ticks>(cap) - 1, 1)));  // 1127

  const std::uint64_t snapshot = b.state_hash();
  // Just outside each edge must be rejected and must NOT change the book.
  REQUIRE_FALSE(b.apply(MarketEvent::add(4, Side::Buy, base - 1, 1)));             // 871
  REQUIRE_FALSE(b.apply(MarketEvent::add(5, Side::Buy, base + static_cast<Ticks>(cap), 1)));  // 1128
  // An extreme price must reject cleanly, with no signed-overflow UB.
  REQUIRE_FALSE(b.apply(MarketEvent::add(6, Side::Buy, std::numeric_limits<Ticks>::max(), 1)));
  REQUIRE(b.state_hash() == snapshot);
  REQUIRE(b.order_count() == 3);

  // Replace to an out-of-window price is rejected; the original order survives.
  REQUIRE_FALSE(b.apply(MarketEvent::replace(1, 7, base - 1, 9)));
  REQUIRE(b.qty_at(Side::Buy, 1000) == 5);
  REQUIRE(b.order_count() == 3);
}

TEST_CASE("determinism: same events produce an identical state hash",
          "[book][determinism]") {
  const auto build = [] {
    OrderBook b;
    b.apply(MarketEvent::add(1, Side::Buy, 100, 10));
    b.apply(MarketEvent::add(2, Side::Sell, 102, 5));
    b.apply(MarketEvent::add(3, Side::Buy, 99, 8));
    b.apply(MarketEvent::reduce(1, 3));
    b.apply(MarketEvent::cancel(2));
    return b.state_hash();
  };
  REQUIRE(build() == build());
}

TEST_CASE("different book states produce different hashes",
          "[book][determinism]") {
  OrderBook a;
  a.apply(MarketEvent::add(1, Side::Buy, 100, 10));
  OrderBook c;
  c.apply(MarketEvent::add(1, Side::Buy, 100, 11));
  REQUIRE(a.state_hash() != c.state_hash());
}
