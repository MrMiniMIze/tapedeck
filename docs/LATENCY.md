# Latency Measurement Methodology

> **Status: TEMPLATE (filled at Phase 4).** No latency number is valid until it is
> produced on a tuned, bare-metal x86 Linux host per the rules below.

## The one rule

Latency is reported **only** from a tuned, bare-metal **x86 Linux** host.
**Never** from: macOS, Apple Silicon, Docker, WSL2, or any cloud VM (GCE/EC2 VM).
A virtualized clock plus vCPU *steal time* make sub-microsecond numbers fiction.

> If a bare-metal host is unavailable, **do not fake it.** Drop the nanosecond
> framing and headline *throughput + determinism + correctness* instead, with a
> one-paragraph note on what real tick-to-trade (NIC, kernel bypass, the wire)
> would add. Honest scoping beats an invented percentile.

## Host configuration (fill in when measured)

- CPU model / microarchitecture: _TODO_
- Invariant TSC present, `grep -o 'constant_tsc\|nonstop_tsc' /proc/cpuinfo | sort -u`: _TODO_
- Kernel cmdline isolation: `isolcpus=`, `nohz_full=`, `rcu_nocbs=`: _TODO_
- Turbo / C-states disabled (BIOS + `cpupower frequency-set`): _TODO_
- Hot-path core pinned (`taskset` / `SetThreadAffinityMask`): _TODO_
- Hugepages: _TODO_
- Bare metal (not virtualized)?: **must be yes**, _TODO_

## Clock

- Source: `rdtscp` via `eib::now_cycles()` on invariant TSC. cycles to ns factor: _TODO_
- Measured clock-read overhead, **subtracted** from all intervals: _TODO_ ns
- Harness self-noise floor (back-to-back reads): p50 _TODO_ / p99 _TODO_ ns

## Coordinated omission

- Corrected via HdrHistogram with an expected-interval: _TODO_

## What is measured (two separate intervals, never conflated)

1. **bytes-received to book-updated**, p50 / p99 / p99.9 = _TODO_
2. **book-updated to signal-decision**, p50 / p99 / p99.9 = _TODO_

Measured under the **real replayed event mix**, never a synthetic hot loop.
Throughput is reported separately and also under the real event mix, never a
tight empty-loop "TPS" figure.

## The one number I don't trust

_TODO, name the weakest percentile and say exactly why._
