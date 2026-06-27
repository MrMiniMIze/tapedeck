// libFuzzer entry point for the ITCH 5.0 parser.
//
// Feeds arbitrary bytes through the length-framed stream parser into a
// BookBuilder. The parser is zero-copy and reads fixed field offsets after
// length checks, so this exercises exactly the code path that touches untrusted
// input. Built with ASan and UBSan (see fuzz/CMakeLists.txt), so any
// out-of-bounds read, signed-overflow, or other undefined behavior aborts the
// run with a reproducer. Build and run via scripts/fuzz.sh (clang only).
#include <cstddef>
#include <cstdint>
#include <span>

#include "eib/itch_parser.hpp"
#include "eib/order_book.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  eib::OrderBook book;
  eib::parse_itch_stream(std::span<const std::uint8_t>(data, size),
                         [&](const eib::MarketEvent& ev) { book.apply(ev); });
  return 0;
}
