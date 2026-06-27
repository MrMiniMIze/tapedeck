// eib_research: run the OFI/microprice strategy through the backtester once per
// fill model and print a per-period returns CSV (one column per model). Pipe it
// into research/killcurve.py to get the kill curve and the honest diagnostics.
//
//   eib_research <itch-5.0-file>   run on real data
//   eib_research --synth           run on a synthetic stream (plumbing check)
//
// The strategy rests a passive order at the touch in the signal direction and is
// filled by the MatchEngine per the fill model, so the returns genuinely differ
// across Conservative and Optimistic. That difference is the kill curve.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <span>
#include <vector>

#include "eib/backtester.hpp"
#include "eib/itch_parser.hpp"
#include "eib/order_book.hpp"
#include "eib/signal.hpp"

using namespace eib;

namespace {

// Passive OFI strategy: quote at the touch on the heavier side, capped inventory,
// marked to market. It samples equity every `sample_every` events so the driver
// can difference the samples into a return series.
struct PassiveOfi {
  Qty cap = 5;
  Qty qty = 1;
  int sample_every = 50;

  std::uint64_t next_id = (1ull << 40);  // our id space, separate from venue ids
  OrderId resting_id = 0;
  Side resting_side = Side::Buy;
  Ticks resting_price = 0;
  int counter = 0;
  std::int64_t last_eq = 0;
  std::vector<std::int64_t> equity;

  void on_event(Backtester& bt, const MarketEvent&) {
    const Bbo q = top_of_book(bt.book());
    // Carry the last valid mark to market so a sample never lands on a
    // momentarily one-sided book (which would spike the return series).
    if (q.valid) last_eq = bt.equity2();
    if (++counter % sample_every == 0) equity.push_back(last_eq);
    if (!q.valid) return;

    const int sig = microprice_direction(q);
    const Qty pos = bt.portfolio().position;

    bool want = false;
    Side wside = Side::Buy;
    Ticks wprice = 0;
    if (sig > 0 && pos < cap) {
      want = true;
      wside = Side::Buy;
      wprice = q.bid_px;
    } else if (sig < 0 && pos > -cap) {
      want = true;
      wside = Side::Sell;
      wprice = q.ask_px;
    }

    if (resting_id != 0) {
      if (!bt.passive_live(resting_id)) {
        resting_id = 0;  // filled or otherwise gone
      } else if (!want || wside != resting_side || wprice != resting_price) {
        bt.cancel_passive(resting_id);
        resting_id = 0;
      }
    }
    if (resting_id == 0 && want) {
      const OrderId id = next_id++;
      if (bt.post_passive(id, wside, wprice, qty)) {
        resting_id = id;
        resting_side = wside;
        resting_price = wprice;
      }
    }
  }
};

std::vector<MarketEvent> load_itch(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
  std::vector<MarketEvent> events;
  parse_itch_stream(std::span<const std::uint8_t>(buf.data(), buf.size()),
                    [&](const MarketEvent& e) { events.push_back(e); });
  return events;
}

// Synthetic market with trades, so passive orders actually fill. Asymmetric best
// sizes give the microprice a direction; an aggressor sweeps one side each bar.
std::vector<MarketEvent> synth_stream(int bars, unsigned seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> drift(-1, 1);
  std::uniform_int_distribution<int> sz(10, 30);
  std::uniform_int_distribution<int> coin(0, 1);

  std::vector<MarketEvent> s;
  OrderId id = 1;
  Ticks mid = 100000;
  OrderId pb1 = 0, pa1 = 0, pb2 = 0, pa2 = 0;

  for (int b = 0; b < bars; ++b) {
    if (pb1) s.push_back(MarketEvent::cancel(pb1));
    if (pa1) s.push_back(MarketEvent::cancel(pa1));
    if (pb2) s.push_back(MarketEvent::cancel(pb2));
    if (pa2) s.push_back(MarketEvent::cancel(pa2));

    const Ticks bid = mid - 1;
    const Ticks ask = mid + 1;
    const OrderId b1 = id++, a1 = id++, b2 = id++, a2 = id++;
    s.push_back(MarketEvent::add(b1, Side::Buy, bid, sz(rng)));
    s.push_back(MarketEvent::add(a1, Side::Sell, ask, sz(rng)));
    s.push_back(MarketEvent::add(b2, Side::Buy, bid, 20));
    s.push_back(MarketEvent::add(a2, Side::Sell, ask, 20));

    if (coin(rng)) {  // aggress the bid: trades print down into the bid queue
      s.push_back(MarketEvent::execute(b1, 10));
      s.push_back(MarketEvent::execute(b2, 18));
    } else {          // aggress the ask
      s.push_back(MarketEvent::execute(a1, 10));
      s.push_back(MarketEvent::execute(a2, 18));
    }

    pb1 = b1; pa1 = a1; pb2 = b2; pa2 = a2;
    mid += drift(rng);
    if (mid < 60000) mid = 60000;
  }
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<MarketEvent> stream;
  if (argc >= 2 && std::strcmp(argv[1], "--synth") == 0) {
    stream = synth_stream(3000, 42);
  } else if (argc >= 2) {
    stream = load_itch(argv[1]);
    if (stream.empty()) {
      std::fprintf(stderr, "no events parsed from %s\n", argv[1]);
      return 1;
    }
  } else {
    std::fprintf(stderr, "usage: %s <itch-5.0-file> | --synth\n", argv[0]);
    return 2;
  }

  struct Run { const char* name; FillModel fm; };
  const Run runs[] = {{"conservative", FillModel::Conservative},
                      {"optimistic", FillModel::Optimistic}};

  std::vector<std::vector<std::int64_t>> equities;
  for (const auto& r : runs) {
    Backtester bt(1u << 16, 1u << 16, r.fm);
    PassiveOfi strat;
    bt.run(stream, strat);
    equities.push_back(strat.equity);
  }

  std::size_t n = equities[0].size();
  for (const auto& e : equities) n = std::min(n, e.size());

  std::printf("conservative,optimistic\n");
  for (std::size_t i = 1; i < n; ++i) {
    std::printf("%lld,%lld\n",
                static_cast<long long>(equities[0][i] - equities[0][i - 1]),
                static_cast<long long>(equities[1][i] - equities[1][i - 1]));
  }
  return 0;
}
