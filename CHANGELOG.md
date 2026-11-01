# Changelog

Notable changes to `repr`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## 2026-10-31

### Changed

- **`repr` tests sixteen bytes at a time on x86 as well as arm64.** The measuring and
  writing loops moved into char-classify's `chars_escape.hh`, shared with `escape`: a
  16-byte block with NEON or SSE2 (8 bytes with the portable fallback), one store for a
  clean block, the table for a block with up to five escapes, a byte loop for a block
  with more. The head and tail of a truncated result go through the same code. Output is
  identical to the previous implementation.

  Speedup over the previous 8-byte SWAR kernel (minimum over three rounds of nine
  interleaved samples, `-O3`, Apple clang 17; a ratio under 1.00x is slower):

  | workload | M4 Pro (arm64) | i9-9980HK (x86) |
  |---|---:|---:|
  | printable 1 B | 1.22x | 1.05x |
  | printable 8 B | 1.16x | 1.06x |
  | printable 16 B | 1.14x | 1.09x |
  | printable 256 B | 1.66x | 1.69x |
  | printable 4 KB | 2.21x | 2.87x |
  | printable 64 KB | 2.03x | 2.32x |
  | mixed 10% 256 B | 1.73x | 1.55x |
  | mixed 10% 64 KB | 2.31x | 3.32x |
  | 1% control bytes, 4 KB | 2.19x | 2.74x |
  | 10% control bytes, 4 KB | 2.29x | 2.47x |
  | 50% control bytes, 4 KB | 2.04x | 1.89x |
  | binary 64 B | 1.33x | 1.16x |
  | binary 4 KB | 1.77x | 1.77x |
  | `max_size = 64`, 4 KB input | 1.03x | 1.06x |
  | `max_size = 64`, 64 KB input | 1.03x | 1.03x |

    The 2026-10-30 gains were measured on Apple silicon only; on x86 the same 8-byte
  kernel in `escape` gained nothing on 64 KB mixed input (audit CR5). Against
  `absl::CEscape` (another format), `repr` is 1.3x (binary 4 KB, 64 KB mixed on x86)
  to 5.0x (printable 4 KB) faster for inputs of 256 bytes and up, and slower for
  inputs of a few bytes (up to 16 on the M4 Pro, 0.57x at 1 B; up to about 40 on the
  i9, 0.49x at 1 B).

### Added

- `test/diff_test.cc` compares windows of every length from 1 to 70 bytes against the
  frozen copy, with every byte value at every position, so the vector loop and its tail
  are exercised (the earlier cases stopped at 17 bytes), plus the head and tail windows of
  truncated results. `repr_swar` and `repr_diff_swar` run the same tests with
  `CHARS_NO_SIMD`.
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
