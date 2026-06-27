#pragma once
// eib::clock: a portable cycle-count timer for latency measurement.
//
// CORRECTNESS WARNING (read docs/LATENCY.md before quoting any number):
//   The cycle counter below is a valid *nanosecond-latency* source ONLY on a
//   tuned, bare-metal x86 host: invariant TSC, cores pinned, turbo/C-states
//   disabled, run OUTSIDE any VM or container.
//   On Apple Silicon (arm64), inside Docker, inside WSL2, or on any virtualized
//   cloud instance (GCE/EC2 VM), treat these values as developer-facing timing
//   only, NEVER as a reported latency claim. is_invariant_tsc() tells you
//   whether you are on the trustworthy path for *this build*; you are still
//   responsible for verifying the host is bare metal (see docs/LATENCY.md).

#include <chrono>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  #define EIB_X86 1
  #if defined(_MSC_VER)
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
#else
  #define EIB_X86 0
#endif

namespace eib {

// True only when the timestamp counter is a meaningful low-latency clock for
// this build target. (An x86 build assumes a modern invariant TSC; the operator
// must still confirm `constant_tsc nonstop_tsc` and bare metal, docs/LATENCY.md.)
constexpr bool is_invariant_tsc() { return EIB_X86 == 1; }

// Read the cycle counter.
//   x86  -> rdtscp (waits for prior instructions to retire; returns the TSC).
//   else -> steady_clock ticks, so the codebase stays portable for development.
//           These fallback values are NOT cycle-accurate and must not be quoted.
inline std::uint64_t now_cycles() {
#if EIB_X86
  unsigned aux = 0;
  return __rdtscp(&aux);
#else
  return static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

// Serializing variant for the boundaries of a measured interval on x86: the
// lfence pair keeps out-of-order execution from leaking work across the read.
inline std::uint64_t now_cycles_serialized() {
#if EIB_X86
  _mm_lfence();
  unsigned aux = 0;
  const std::uint64_t t = __rdtscp(&aux);
  _mm_lfence();
  return t;
#else
  return now_cycles();
#endif
}

}  // namespace eib
