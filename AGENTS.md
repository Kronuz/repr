# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
repr.hh                      Declares repr() and the convenience overloads (void*+size, range, string, string_view, array). Header. Same as Xapiand's.
repr.cc                      The one compiled function: length tables, exact-size result, 8-byte block copy. Includes "chars.hh" for chars::char_repr.
test/test.cc                 Runnable smoke test: passthrough, escapes, hex, quote modes, non-friendly, NULs, overloads, truncation.
test/diff_test.cc            Differential test of repr() against test/repr_old.cc.
test/repr_old.cc             Frozen Xapiand implementation, the differential test's oracle. Do not edit.
bench/bench.cc               Benchmark (REPR_BUILD_BENCH=ON): repr() against repr_old and, when available, absl::CEscape.
examples/demo.cc             A runnable tour (not a test).
CMakeLists.txt               STATIC library `repr` (+ alias repr::repr); FetchContent char-classify; CTest test `repr`.
LICENSE                      MIT, Copyright (c) 2015-2019 Dubalu LLC.
README.md                    What it is, install, quick start.
docs/API.md                  The API reference.
docs/repr.d2                 The README diagram's source (docs/repr.svg is rendered from it).
ARCHITECTURE.md              Internal design of the escaping pass, sizing, quoting, truncation.
CHANGELOG.md                 Behavior and performance changes, newest first.
```

This is not header-only. `repr.cc` must be compiled and linked; the header only
declares the overloads. The CMake target is a `STATIC` library that links
`char-classify` so `"chars.hh"` resolves.

## Build and run the test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first configure fetches `char-classify` over the network (FetchContent, at
the commit `CMakeLists.txt` pins). Expected output ends with `all repr tests passed`, exit 0. The
test target is `repr_test`; the registered CTest name is `repr`. The test and demo
are only added when this repo is the top-level project (CMakeLists.txt), so
consumers vendoring it via `FetchContent` won't build them.

## Dependency

`repr.cc` includes `"chars.hh"` for one symbol, `chars::char_repr` (writes the
lowercase two-digit hex of a byte through a `char**`). That header lives in the
sibling [char-classify](https://github.com/Kronuz/char-classify) library, pulled
in by CMake and linked `PUBLIC` so the include path resolves. This is the
library's only dependency; there is no logging or config coupling.

## Conventions

- **C++20.** The target requests `cxx_std_20` `PUBLIC` to stay uniform with the
  sibling libraries. Don't drop the target below it.
- **Filenames are stable.** The header and source keep their original Xapiand
  names (`repr.hh`, `repr.cc`) so a consumer that already `#include`s the header
  just needs this repo on the include path. Don't rename them.
- Tabs for indentation, double quotes in code, no em dashes in prose.
- MIT-licensed; keep the copyright header (Copyright (c) 2015-2019 Dubalu LLC) on
  source files.

## Load-bearing invariants

- **The output buffer is sized exactly, from the same table that writes it.**
  `repr.cc` measures the kept bytes with the length table, calls `ret.resize(n)`
  once and writes through a raw `char*` with no bounds checks (an `assert` checks
  that the writer produced exactly `n` bytes). If you add or change an escape you
  must change the table (`make_tab`) and the writer (`emit_byte`) together, and
  the SWAR test (`dirty8`) if the set of "plain" bytes changes. Writing more than
  the table says is a heap overflow.
- **`repr` is size-based, not NUL-terminated.** It renders exactly `size` bytes,
  including any embedded `\0` (as `\x00`). Don't "optimize" it to stop at a NUL.
  The array overload deliberately passes `N - 1` to drop only a *string literal's*
  trailing NUL.
- **The quote sentinels matter.** `'\0'` means no surrounding quotes and no
  quote-escaping; `'\1'` is remapped to `'\''`. The entry `assert` pins `quote` to
  `'\0'` / `'\1'` / `'\''` / `'"'`. Keep both sentinels and the assert.
- **Hex rendering is delegated to `chars::char_repr`.** Two lowercase hex digits,
  written through the cursor, advancing it by two. Don't inline a second hex table
  here; keep it coming from char-classify so the two libraries agree.
- **Behavior is pinned by `test/diff_test.cc`.** It compares `repr()` with the
  frozen Xapiand implementation (`test/repr_old.cc`) over random and edge inputs,
  every quote mode and `max_size` values around the truncation window. The one
  deliberate difference is `max_size == 1` with a quote: `repr_old` writes past
  its buffer there, so the test checks `repr()` for the two quotes instead.

## How to extend

- **Add a named escape (e.g. `\a`, `\v`).** Give it length 2 and its letter in
  `make_tab`; `emit_byte` writes every length-2 entry as a backslash plus that
  letter, and `dirty8` already flags every byte below 0x20. An escape longer than
  four bytes needs a new case in `emit_byte` as well.
- **Always extend the smoke test.** `test/test.cc` asserts exact strings;
  `test/diff_test.cc` only compares against the oracle. Add a case for any new escape or mode and assert the exact string.

## Traps

- **The truncation window is pointer arithmetic into the input.** `lo` / `hi`
  (in `repr()`) split the budget head/tail; the dots are written where the 1-based
  position `lo` falls. Off-by-one edits here either drop the dots or mis-place
  them. With `max_size == 0` the window is set so nothing is skipped; preserve that.
- **`char_repr` advances the write pointer by two.** It takes a `char**`. The
  measuring pass already counts those bytes (length 4 per `\xNN`), but if you call
  it from a new path make sure the result was sized for it.
- **High bytes (>= 0x7f) are escaped, not interpreted.** This is an ASCII-oriented,
  byte-faithful renderer, not a Unicode display. Don't add multibyte decoding.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). `repr.hh` matches upstream.
`repr.cc` is a faster implementation with the same output, apart from the
`max_size == 1` case above; the Xapiand version is `test/repr_old.cc`, the test
oracle. The one local include, `"chars.hh"`, resolves against the standalone
char-classify library through CMake. Keep the header and the behavior
reconcilable with upstream.
