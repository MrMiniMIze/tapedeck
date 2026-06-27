// eib_replay: stream a NASDAQ ITCH 5.0 file through the FeedParser + BookBuilder
// and print summary stats. Reads in bounded-memory chunks (carries a partial
// trailing message across reads), so it handles multi-GB files without loading
// them whole. Usage: eib_replay <itch-5.0-file>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <span>
#include <vector>

#include "eib/itch_parser.hpp"
#include "eib/order_book.hpp"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <itch-5.0-file>\n", argv[0]);
    return 2;
  }
  std::ifstream in(argv[1], std::ios::binary);
  if (!in) {
    std::fprintf(stderr, "error: cannot open %s\n", argv[1]);
    return 1;
  }

  eib::OrderBook book;
  std::uint64_t events = 0;
  std::uint64_t rejected = 0;
  std::vector<std::uint8_t> buf;
  char chunk[1 << 16];
  bool terminated = false;

  while (!terminated &&
         in.read(chunk, static_cast<std::streamsize>(sizeof(chunk))).gcount() > 0) {
    const auto got = static_cast<std::size_t>(in.gcount());
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(chunk);
    buf.insert(buf.end(), bytes, bytes + got);

    std::size_t off = 0;
    while (off + 2 <= buf.size()) {
      const std::size_t len = (static_cast<std::size_t>(buf[off]) << 8) |
                              static_cast<std::size_t>(buf[off + 1]);
      if (len == 0) {
        terminated = true;  // framing terminator
        break;
      }
      if (off + 2 + len > buf.size()) break;  // need more bytes for this message
      eib::MarketEvent ev;
      if (eib::decode_itch_message(
              std::span<const std::uint8_t>(buf.data() + off + 2, len), ev) ==
          eib::ItchStatus::Event) {
        if (book.apply(ev)) {
          ++events;
        } else {
          ++rejected;  // inconsistent event: counted, never silently absorbed
        }
      }
      off += 2 + len;
    }
    buf.erase(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(off));
  }

  std::printf("events applied : %llu\n", static_cast<unsigned long long>(events));
  std::printf("events rejected: %llu\n", static_cast<unsigned long long>(rejected));
  std::printf("resting orders : %zu\n", book.order_count());
  if (book.has_bids() && book.has_asks()) {
    std::printf("best bid / ask : %lld / %lld (ticks, 1/10000$)\n",
                static_cast<long long>(book.best_bid()),
                static_cast<long long>(book.best_ask()));
  }
  std::printf("state hash     : %016llx\n",
              static_cast<unsigned long long>(book.state_hash()));
  return 0;
}
