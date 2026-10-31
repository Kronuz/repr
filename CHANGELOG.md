# Changelog

Notable changes to `repr`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## 2026-10-31

### Added

- `test_tiny_max_size` in `test/test.cc` pins the exact output for `max_size`
  1 and 2, for input sizes 0 to 20, every quote mode and both `friendly` modes.
  It fails against the Xapiand implementation and passes under ASan and UBSan.

## 2026-10-30

### Changed

- `repr()` sizes its result exactly. A per-quote 256-entry length table drives
  a measuring pass, and the result is resized once. The Xapiand implementation
  zero-filled a `4n + 2` buffer and escaped one byte at a time.
- The writer tests eight bytes at a time (SWAR) and copies blocks of plain
  printable bytes with one store.
- With `max_size`, the head and tail are located by arithmetic, keeping the
  1-based window convention of the original pre-incrementing pointer loop. The
  elided middle is no longer walked, so the cost no longer depends on the input
  size.
- Speed against the Xapiand implementation, on Apple silicon (Apple clang 17,
  -O3), from `repr_bench`:

  | Input | Speedup |
  | --- | --- |
  | Printable text | 2.5x to 6.6x (4 KB: 6.4 us to 0.96 us; `absl::CEscape` 2.2 us) |
  | 10% non-printable | 2.8x to 3.4x |
  | All binary | 1.4x |
  | `max_size = 64`, 4 KB input | 54x |
  | `max_size = 64`, 64 KB input | 940x |

### Fixed

- `max_size == 1` with a quote sized the buffer at one byte and wrote both
  quotes past it (the result ended in a stray NUL). It now returns the two
  quotes. Every other output is byte-identical to the Xapiand implementation.

### Added

- `test/diff_test.cc`, a differential test against a frozen copy of the
  Xapiand implementation (`test/repr_old.cc`).
- `bench/bench.cc`, built with `-DREPR_BUILD_BENCH=ON`.
