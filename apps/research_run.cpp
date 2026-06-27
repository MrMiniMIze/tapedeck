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
#include <algorithm>
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

// Aggressive microprice strategy: hold a +/-1 position in the signal direction,
// re-evaluated every `horizon` samples, trading at the touch so the spread is a
// real, paid cost. Marks to market each sample. Sweeping the horizon shows where
// the edge decays and whether it survives the spread. (The passive MatchEngine
// path is tested separately; on a deep, liquid name a size-1 passive quote
// essentially never fills, so it is not the right probe for AAPL.)
struct AggressiveSignal {
  int horizon = 1;
  int sample_every = 50;
  long counter = 0;
  long sample_idx = 0;
  Qty target = 0;
  std::int64_t last_eq = 0;
  std::vector<std::int64_t> equity;

  void on_event(Backtester& bt, const MarketEvent&) {
    const Bbo q = top_of_book(bt.book());
    if (q.valid) {
      bt.set_target(target);  // trade to target at the touch (pays the spread)
      last_eq = bt.equity2();
    }
    if (++counter % sample_every == 0) {
      equity.push_back(last_eq);
      if (sample_idx % horizon == 0 && q.valid) {
        target = static_cast<Qty>(microprice_direction(q));
      }
      ++sample_idx;
    }
  }
};

std::vector<MarketEvent> load_itch(const char* path, const char symbol[8]) {
  std::ifstream in(path, std::ios::binary);
  std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
  std::vector<MarketEvent> events;
  parse_itch_symbol(std::span<const std::uint8_t>(buf.data(), buf.size()), symbol,
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
    char sym[8];
    std::memset(sym, ' ', sizeof(sym));
    const char* s = (argc >= 3) ? argv[2] : "AAPL";
    for (int i = 0; i < 8 && s[i] != '\0'; ++i) sym[i] = s[i];
    stream = load_itch(argv[1], sym);
    std::fprintf(stderr, "parsed %zu events for symbol '%.8s' from %s\n",
                 stream.size(), sym, argv[1]);
    if (stream.empty()) {
      std::fprintf(stderr, "no events for that symbol (check the ticker)\n");
      return 1;
    }
  } else {
    std::fprintf(stderr, "usage: %s <itch-5.0-file> [SYMBOL] | --synth\n", argv[0]);
    return 2;
  }

  // Center the price window on the median add price and drop far stubs, so the
  // window sits at the real market rather than a stray pre-market order.
  Ticks center = 0;
  {
    std::vector<Ticks> px;
    for (const auto& e : stream)
      if (e.type == EventType::Add) px.push_back(e.price);
    if (!px.empty()) {
      std::nth_element(px.begin(), px.begin() + static_cast<std::ptrdiff_t>(px.size() / 2),
                       px.end());
      center = px[px.size() / 2];
      const Ticks band = 120000;  // keep adds within $12 of the median
      std::vector<MarketEvent> filt;
      filt.reserve(stream.size());
      for (const auto& e : stream)
        if (!(e.type == EventType::Add &&
              (e.price < center - band || e.price > center + band)))
          filt.push_back(e);
      stream.swap(filt);
    }
  }

  const int horizons[] = {1, 5, 20, 100};
  std::vector<std::vector<std::int64_t>> equities;
  for (const int h : horizons) {
    Backtester bt(1u << 18, 1u << 19, FillModel::Conservative, center);
    AggressiveSignal strat;
    strat.horizon = h;
    bt.run(stream, strat);
    equities.push_back(strat.equity);
  }

  std::size_t n = equities[0].size();
  for (const auto& e : equities) n = std::min(n, e.size());

  std::printf("h1,h5,h20,h100\n");
  for (std::size_t i = 1; i < n; ++i) {
    std::printf("%lld,%lld,%lld,%lld\n",
                static_cast<long long>(equities[0][i] - equities[0][i - 1]),
                static_cast<long long>(equities[1][i] - equities[1][i - 1]),
                static_cast<long long>(equities[2][i] - equities[2][i - 1]),
                static_cast<long long>(equities[3][i] - equities[3][i - 1]));
  }
  return 0;
}
