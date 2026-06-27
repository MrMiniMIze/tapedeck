#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <span>
#include <vector>

#include "eib/itch_parser.hpp"
#include "eib/order_book.hpp"

using namespace eib;

namespace {

// --- builders that emit real ITCH 5.0 byte layouts (big-endian) ---

void put_be(std::vector<std::uint8_t>& v, std::uint64_t value, int nbytes) {
  for (int i = nbytes - 1; i >= 0; --i)
    v.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu));
}

std::vector<std::uint8_t> add_msg(std::uint64_t id, char bs, std::uint32_t shares,
                                  std::uint32_t price, std::uint64_t ts = 0) {
  std::vector<std::uint8_t> m;
  m.push_back(static_cast<std::uint8_t>('A'));
  put_be(m, 0, 2);          // stock locate
  put_be(m, 0, 2);          // tracking number
  put_be(m, ts, 6);         // timestamp
  put_be(m, id, 8);         // order reference
  m.push_back(static_cast<std::uint8_t>(bs));  // buy/sell
  put_be(m, shares, 4);     // shares
  for (int i = 0; i < 8; ++i) m.push_back(static_cast<std::uint8_t>(' '));  // stock
  put_be(m, price, 4);      // price
  return m;                 // 36 bytes
}

std::vector<std::uint8_t> add_sym(std::uint64_t id, char bs, std::uint32_t shares,
                                  std::uint32_t price, const char* stock) {
  std::vector<std::uint8_t> m;
  m.push_back(static_cast<std::uint8_t>('A'));
  put_be(m, 0, 2); put_be(m, 0, 2); put_be(m, 0, 6);
  put_be(m, id, 8);
  m.push_back(static_cast<std::uint8_t>(bs));
  put_be(m, shares, 4);
  for (int i = 0; i < 8; ++i) m.push_back(static_cast<std::uint8_t>(stock[i]));
  put_be(m, price, 4);
  return m;  // 36 bytes
}

std::vector<std::uint8_t> exec_msg(std::uint64_t id, std::uint32_t shares) {
  std::vector<std::uint8_t> m;
  m.push_back(static_cast<std::uint8_t>('E'));
  put_be(m, 0, 2); put_be(m, 0, 2); put_be(m, 0, 6);
  put_be(m, id, 8); put_be(m, shares, 4); put_be(m, 0, 8);  // match number
  return m;  // 31 bytes
}

std::vector<std::uint8_t> delete_msg(std::uint64_t id) {
  std::vector<std::uint8_t> m;
  m.push_back(static_cast<std::uint8_t>('D'));
  put_be(m, 0, 2); put_be(m, 0, 2); put_be(m, 0, 6); put_be(m, id, 8);
  return m;  // 19 bytes
}

std::vector<std::uint8_t> replace_msg(std::uint64_t old_id, std::uint64_t new_id,
                                      std::uint32_t shares, std::uint32_t price) {
  std::vector<std::uint8_t> m;
  m.push_back(static_cast<std::uint8_t>('U'));
  put_be(m, 0, 2); put_be(m, 0, 2); put_be(m, 0, 6);
  put_be(m, old_id, 8); put_be(m, new_id, 8); put_be(m, shares, 4); put_be(m, price, 4);
  return m;  // 35 bytes
}

void frame(std::vector<std::uint8_t>& buf, const std::vector<std::uint8_t>& m) {
  put_be(buf, m.size(), 2);
  buf.insert(buf.end(), m.begin(), m.end());
}

std::span<const std::uint8_t> as_span(const std::vector<std::uint8_t>& v) {
  return std::span<const std::uint8_t>(v.data(), v.size());
}

}  // namespace

TEST_CASE("ITCH add decodes every field big-endian (offset round-trip)", "[itch]") {
  const auto m = add_msg(0x0102030405060708ull, 'B', 1234, 1000000, 42);
  MarketEvent ev;
  REQUIRE(decode_itch_message(as_span(m), ev) == ItchStatus::Event);
  REQUIRE(ev.type == EventType::Add);
  REQUIRE(ev.side == Side::Buy);
  REQUIRE(ev.id == 0x0102030405060708ull);
  REQUIRE(ev.qty == 1234);
  REQUIRE(ev.price == 1000000);
  REQUIRE(ev.ts == 42);
}

TEST_CASE("ITCH stream builds the book", "[itch][book]") {
  std::vector<std::uint8_t> buf;
  frame(buf, add_msg(1, 'B', 100, 1000000));  // buy 100 @ 100.0000
  frame(buf, add_msg(2, 'S', 50, 1000200));   // sell 50 @ 100.0200
  frame(buf, exec_msg(1, 30));                 // execute 30 of order 1

  OrderBook book;
  const auto n = parse_itch_stream(
      as_span(buf), [&](const MarketEvent& ev) { REQUIRE(book.apply(ev)); });

  REQUIRE(n == 3);
  REQUIRE(book.best_bid() == 1000000);
  REQUIRE(book.best_ask() == 1000200);
  REQUIRE(book.qty_at(Side::Buy, 1000000) == 70);
  REQUIRE(book.qty_at(Side::Sell, 1000200) == 50);
}

TEST_CASE("ITCH replace moves the order to a new id/price", "[itch][book]") {
  std::vector<std::uint8_t> buf;
  frame(buf, add_msg(1, 'B', 100, 1000000));
  frame(buf, replace_msg(1, 2, 80, 1000100));

  OrderBook book;
  parse_itch_stream(as_span(buf),
                    [&](const MarketEvent& ev) { REQUIRE(book.apply(ev)); });

  REQUIRE(book.best_bid() == 1000100);
  REQUIRE(book.qty_at(Side::Buy, 1000100) == 80);
  REQUIRE(book.qty_at(Side::Buy, 1000000) == 0);
  REQUIRE(book.order_count() == 1);
}

TEST_CASE("ITCH delete empties the level", "[itch][book]") {
  std::vector<std::uint8_t> buf;
  frame(buf, add_msg(1, 'B', 100, 1000000));
  frame(buf, delete_msg(1));

  OrderBook book;
  parse_itch_stream(as_span(buf),
                    [&](const MarketEvent& ev) { REQUIRE(book.apply(ev)); });
  REQUIRE_FALSE(book.has_bids());
}

TEST_CASE("parse_itch_symbol filters to a single stock", "[itch]") {
  std::vector<std::uint8_t> buf;
  frame(buf, add_sym(1, 'B', 100, 1000000, "AAPL    "));
  frame(buf, add_sym(2, 'B', 50, 999000, "MSFT    "));
  frame(buf, exec_msg(1, 30));  // AAPL order
  frame(buf, exec_msg(2, 10));  // MSFT order
  const char aapl[8] = {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '};
  int seen = 0;
  const auto n =
      parse_itch_symbol(as_span(buf), aapl, [&](const MarketEvent&) { ++seen; });
  REQUIRE(n == 2);  // only the AAPL add and the AAPL execute
  REQUIRE(seen == 2);
}

TEST_CASE("ITCH stream stops cleanly at a truncated tail", "[itch]") {
  std::vector<std::uint8_t> buf;
  frame(buf, add_msg(1, 'B', 100, 1000000));  // one complete message
  put_be(buf, 36, 2);                          // claims 36 bytes...
  for (int i = 0; i < 10; ++i) buf.push_back(0);  // ...but only 10 follow

  int count = 0;
  const auto n =
      parse_itch_stream(as_span(buf), [&](const MarketEvent&) { ++count; });
  REQUIRE(n == 1);      // only the complete message is dispatched
  REQUIRE(count == 1);
}
