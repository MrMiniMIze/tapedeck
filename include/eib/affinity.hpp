#pragma once
// eib::affinity: pin the calling thread to a single logical core.
//
// Returns true on success. NOTE: affinity alone does not give you an isolated
// core. Real latency work also needs OS-level isolation (Linux kernel cmdline:
// `isolcpus=N nohz_full=N rcu_nocbs=N`) so the scheduler, RCU, and IRQs stop
// sharing the core. See docs/LATENCY.md.

#include <cstddef>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#elif defined(__linux__)
  #include <pthread.h>
  #include <sched.h>
#endif

namespace eib {

// Whether real thread pinning is available on this platform.
constexpr bool affinity_supported() {
#if defined(_WIN32) || defined(__linux__)
  return true;
#else
  return false;  // macOS (incl. Apple Silicon) has no hard thread-affinity API.
#endif
}

inline bool pin_this_thread_to_core(int core) {
#if defined(_WIN32)
  const DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << core);
  return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
#elif defined(__linux__)
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(static_cast<std::size_t>(core), &set);
  return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
  (void)core;
  return false;
#endif
}

}  // namespace eib
