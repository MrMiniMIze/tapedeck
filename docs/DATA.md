# Data: NASDAQ TotalView-ITCH 5.0

The chosen venue spine. Real per-order (L3 / MBO) data, so queue position is real.
**Raw data is git-ignored** (see `.gitignore`), keep files under `data/`.

## Where to get a sample

NASDAQ publishes historical ITCH 5.0 sample files (one trading day each, gzipped,
several GB uncompressed). Search **"NASDAQ TotalView-ITCH 5.0 sample"**, they're
hosted on NASDAQ's public file servers (e.g. `emi.nasdaq.com`), with filenames like
`MMDDYYYY.NASDAQ_ITCH50.gz`.

```bash
# example once downloaded into data/
gunzip data/01302019.NASDAQ_ITCH50.gz
scripts/replay.sh data/01302019.NASDAQ_ITCH50     # bash / WSL2 / macOS
# or:  scripts\replay.ps1 data\01302019.NASDAQ_ITCH50   (Windows)
```

Tip: to iterate fast, work on a smaller slice first (e.g. `head -c 200000000` of the
decompressed file), a full day is large and a slice is plenty for correctness work.

For an independent cross-check of the reconstructed book, **LOBSTER** publishes free
single-day reconstructed-book samples for a few symbols, useful to diff your
`BookBuilder` output against.

## Framing assumption

`parse_itch_stream` / `eib_replay` expect each message to be preceded by a **2-byte
big-endian length**, the layout of NASDAQ's downloadable sample files. If your source
is raw MoldUDP64 (UDP packet framing) instead, strip/adapt the transport framing first.

## Message types handled (book-affecting)

| Type | Meaning | Effect on book |
|------|---------|----------------|
| `A` / `F` | Add Order (with/without MPID) | new resting order |
| `E` | Order Executed | reduce resting size |
| `C` | Order Executed With Price | reduce resting size |
| `X` | Order Cancel (partial) | reduce resting size |
| `D` | Order Delete | remove order |
| `U` | Order Replace | remove old id, add new id (same side) |

All other message types (System Event, Stock Directory, Trade `P`/`Q`, etc.) are
ignored for book reconstruction. Prices are 4-byte integers in 1/10000 of a dollar,
already integer ticks. Timestamps are 48-bit nanoseconds since midnight.
