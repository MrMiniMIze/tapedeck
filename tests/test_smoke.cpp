#include <catch2/catch_test_macros.hpp>

#include <string>

#include "eib/affinity.hpp"
#include "eib/clock.hpp"
#include "eib/version.hpp"

TEST_CASE("cycle clock advances monotonically", "[clock]") {
  const auto a = eib::now_cycles();
  const auto b = eib::now_cycles();
  REQUIRE(b >= a);
}

TEST_CASE("serialized clock read is usable", "[clock]") {
  const auto a = eib::now_cycles_serialized();
  const auto b = eib::now_cycles_serialized();
  REQUIRE(b >= a);
}

TEST_CASE("thread pinning never crashes and reports support honestly",
          "[affinity]") {
  const bool ok = eib::pin_this_thread_to_core(0);
  if (eib::affinity_supported()) {
    // On Linux/Windows CI runners pinning to core 0 normally succeeds, but we
    // only assert that the call is well-behaved, not that the OS granted it.
    INFO("pin result on a supported platform: " << ok);
    SUCCEED();
  } else {
    // macOS / Apple Silicon: pinning is unsupported and must report false.
    REQUIRE_FALSE(ok);
  }
}

TEST_CASE("version string is present", "[meta]") {
  REQUIRE(std::string(eib::kVersion).size() > 0);
}
