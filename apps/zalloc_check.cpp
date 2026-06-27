// Proves the OrderBook steady-state path allocates zero memory. Overrides global
// operator new to abort while a flag is armed, constructs the book and builds a
// workload (allocations there are fine, before arming), then replays the whole
// workload through apply() with the guard armed. If any apply() allocates, the
// process aborts with a nonzero exit. Run as its own CI job.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

#include "eib/event.hpp"
#include "eib/order_book.hpp"

namespace {
bool g_armed = false;
}

void* operator new(std::size_t n) {
  if (g_armed) {
    std::fprintf(stderr, "FAIL: allocation on the steady-state path\n");
    std::abort();
  }
  void* p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main() {
  using namespace eib;
  OrderBook book(1u << 16, 1u << 16);

  // Build the workload up front. Allocations here happen before the guard arms.
  std::vector<MarketEvent> evs;
  evs.reserve(400000);
  OrderId id = 1;
  const Ticks mid = 100000;

  for (int i = 0; i < 1000; ++i) {
    const Side s = (i & 1) ? Side::Sell : Side::Buy;
    const Ticks px = mid + ((i & 1) ? 50 + (i % 20) : -(i % 20));
    evs.push_back(MarketEvent::add(id++, s, px, 10));
  }
  for (int i = 0; i < 100000; ++i) {
    const Side s = (i & 1) ? Side::Sell : Side::Buy;
    const Ticks px = mid + ((i & 1) ? 50 + (i % 30) : -(i % 30));
    const OrderId oid = id++;
    evs.push_back(MarketEvent::add(oid, s, px, 5));
    evs.push_back(MarketEvent::execute(oid, 2));
    evs.push_back(MarketEvent::cancel(oid));
  }

  g_armed = true;
  std::size_t applied = 0;
  for (const auto& e : evs) applied += book.apply(e) ? 1u : 0u;
  g_armed = false;

  std::printf("zero-alloc steady-state: OK (events=%zu applied=%zu resting=%zu)\n",
              evs.size(), applied, book.order_count());
  return 0;
}
