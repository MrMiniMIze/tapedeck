# Benchmarks

> WARNING: Every number here is **invalid** unless the Environment block below says
> bare-metal x86 Linux. No Docker / WSL2 / VM numbers. See [LATENCY.md](LATENCY.md).

## Environment

- Host: _TODO_ (your dual-booted PC? rented bare metal? **not** a GCE/EC2 VM)
- Virtualized: **must be "no"** for any latency claim, _TODO_
- CPU / cores isolated / turbo+C-states off: _TODO_
- Commit measured: _TODO_
- Reproduce: _TODO (the one command)_

## Results (fill at Phase 4)

| Metric | Value | Notes |
|--------|-------|-------|
| Throughput (real event mix) | _TODO_ msgs/s | not a synthetic loop |
| bytes-received to book-updated | p50/p99/p99.9 _TODO_ | see LATENCY.md |
| book-updated to signal-decision | p50/p99/p99.9 _TODO_ | see LATENCY.md |
| Zero hot-path allocations | _TODO_ | CI operator-new-abort test |
| Determinism | _TODO_ | event-log SHA256 stable across N runs |
| Book correctness | _TODO_ | golden-master diff vs feed executions |
