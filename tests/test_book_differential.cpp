#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <random>
#include <vector>

#include "eib/order_book.hpp"
#include "eib/reference_book.hpp"

using namespace eib;

namespace {

// Compare the full observable state of the flat book against the reference.
bool agree(const OrderBook& a, const ReferenceOrderBook& b) {
  if (a.order_count() != b.order_count()) return false;
  if (a.has_bids() != b.has_bids()) return false;
  if (a.has_asks() != b.has_asks()) return false;
  if (a.has_bids() && a.best_bid() != b.best_bid()) return false;
  if (a.has_asks() && a.best_ask() != b.best_ask()) return false;
  if (a.level_count(Side::Buy) != b.level_count(Side::Buy)) return false;
  if (a.level_count(Side::Sell) != b.level_count(Side::Sell)) return false;
  return a.state_hash() == b.state_hash();
}

}  // namespace

// The flat OrderBook is fast but fiddly; the ReferenceOrderBook is obviously
// correct. Drive identical random histories through both and require they agree
// after every event. Run under ASan/UBSan in CI, this is the load-bearing
// correctness guard for the flat implementation and for the IdIndex's
// backward-shift deletion in particular.
TEST_CASE("flat OrderBook matches the reference over random histories",
          "[book][differential]") {
  std::mt19937_64 rng(0xC0FFEEull);
  OrderBook flat(1u << 13, 1u << 17);
  ReferenceOrderBook ref;

  std::vector<OrderId> live;
  OrderId next_id = 1;
  std::uniform_int_distribution<int> price_d(1000, 1100);
  std::uniform_int_distribution<int> qty_d(1, 50);
  std::uniform_int_distribution<int> op_d(0, 9);
  std::uniform_int_distribution<int> side_d(0, 1);

  constexpr int kSteps = 20000;
  for (int step = 0; step < kSteps; ++step) {
    const int op = op_d(rng);
    MarketEvent ev{};
    if (op <= 4 || live.empty()) {
      const Side s = side_d(rng) ? Side::Sell : Side::Buy;
      ev = MarketEvent::add(next_id++, s, price_d(rng), qty_d(rng));
    } else {
      const OrderId id = live[rng() % live.size()];
      switch (op) {
        case 5: ev = MarketEvent::cancel(id); break;
        case 6: ev = MarketEvent::reduce(id, qty_d(rng)); break;
        case 7: ev = MarketEvent::execute(id, qty_d(rng)); break;
        case 8: ev = MarketEvent::replace(id, next_id++, price_d(rng), qty_d(rng)); break;
        default: ev = MarketEvent::cancel(rng()); break;  // usually unknown id
      }
    }
    const bool fa = flat.apply(ev);
    const bool ra = ref.apply(ev);
    REQUIRE(fa == ra);
    REQUIRE(agree(flat, ref));
    if (fa) {
      if (ev.type == EventType::Add) live.push_back(ev.id);
      else if (ev.type == EventType::Replace) live.push_back(ev.new_id);
    }
  }
  REQUIRE(flat.order_count() == ref.order_count());
}
