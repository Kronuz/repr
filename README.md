# repr

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/repr/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/repr/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

Python-style repr() of byte buffers for C++20: one printable line, sized exactly, with head-and-tail truncation.
Printable ASCII passes through and everything else becomes a readable escape, so
control characters, binary data and embedded NULs log as one line you can copy
and paste. A table gives each byte's output length, the result is allocated
once, and clean blocks of 16 bytes are copied whole. Reach for it to log keys,
payloads or protocol bytes you cannot trust to be printable.

## ✨ Highlights

- **Safe to print.** Printable ASCII passes through; `\n`, `\r`, `\t`, `\\` and
  the quote get backslash escapes; every other byte becomes `\xNN`.
- **Size-based.** Embedded NULs are rendered as `\x00`, never taken as the end.
- **Your choice of quotes.** Single (the default), double, or none (`'\0'`).
- **Truncation in O(`max_size`).** `max_size` keeps about two thirds of the budget
  from the head and one third from the tail, with `...` between them; the
  dropped middle is never read.
- **All-hex mode.** `friendly=false` writes every byte as `\xNN`.
- **One allocation per call.** The output is measured with the same table that
  writes it, so the string is sized once.
- **Vector block scan.** 16 bytes per step with NEON or SSE2, 8 with the portable
  backend, through [char-classify](https://github.com/Kronuz/char-classify)'s
  escaping kernel.

## 🚀 Quick start

```cpp
#include "repr.hh"

#include <cstdio>
#include <string>

int main() {
	std::string data("a\nb\x1b\0", 5);                      // embedded ESC and NUL
	std::puts(repr(data).c_str());                          // 'a\nb\x1b\x00'
	std::puts(repr(std::string_view("it's"), true, '"').c_str());   // "it's"
	std::puts(repr(std::string_view("AB"), false).c_str());         // '\x41\x42'

	std::string log;
	for (int i = 0; i < 100; ++i) { log += "0123456789"; }  // 1,000 bytes
	std::puts(repr(log, true, '\0', 20).c_str());           // 012345678901...456789
}
```

[`examples/demo.cc`](examples/demo.cc) shows every quoting mode and truncation.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; CMake fetches
[char-classify](https://github.com/Kronuz/char-classify) and compiles one source
file (`repr.cc`). CI builds it on Linux (x86_64, arm64), macOS (arm64, x86_64),
and FreeBSD, plus ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(repr
  GIT_REPOSITORY https://github.com/Kronuz/repr.git
  GIT_TAG        ad85f544725bd405f8260a57c07a7356541a06c7)  # or main for the tip
FetchContent_MakeAvailable(repr)

target_link_libraries(my_app PRIVATE repr::repr)
```

```cpp
#include "repr.hh"
```

## 🎯 When to use it

Use it wherever bytes of unknown content go into a log line, an error message or
a debugger print.

Not for:

- **A reversible encoding.** There is no unrepr, and a truncated result has lost
  its middle.
- **Escaping for a format.** JSON, shell and URLs have their own rules; use their
  escapers. [escape](https://github.com/Kronuz/escape) writes the same C-style
  escapes without truncation or the all-hex mode.
- **Unicode display.** Bytes from `0x7f` up are written as `\xNN`, so UTF-8 text
  outside ASCII comes out as hex.
- **The fewest nanoseconds on tiny inputs.** `absl::CEscape` is as fast or faster
  up to 16 bytes on the M4 Pro and up to about 40 on the i9.

## ⚡ Performance

Time per call, the best of seven samples of at least 8 ms; `-O3`, Apple clang 17,
abseil 20260817.0. `absl::CEscape` writes a different format, so it is a speed
reference only:

| input | Apple M4 Pro: `repr` / `absl::CEscape` | Intel Core i9-9980HK: `repr` / `absl::CEscape` |
|---|---:|---:|
| printable 16 B | 10 ns / 10 ns | 26 ns / 16 ns |
| printable 256 B | 50 ns / 181 ns | 165 ns / 373 ns |
| printable 4 KB | 450 ns / 2.2 us | 933 ns / 4.0 us |
| printable 64 KB | 7.5 us / 35.2 us | 25.1 us / 63.7 us |
| 10% random bytes, 64 KB | 12.8 us / 35.6 us | 46.5 us / 61.9 us |
| 10% control bytes, 4 KB | 832 ns / 2.2 us | 1.7 us / 4.1 us |
| bytes from `0x80` up, 4 KB | 1.7 us / 2.2 us | 3.2 us / 4.0 us |

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DREPR_BUILD_BENCH=ON
cmake --build build --target repr_bench
./build/repr_bench
```

From 256 bytes up `repr` is ahead on every row. With `max_size` the elided middle
is skipped by arithmetic, so the cost of a truncated call does not grow with the
input. `repr_bench` also times the frozen original (`test/repr_old.cc`) and
`friendly=false`; [CHANGELOG.md](CHANGELOG.md) has those comparisons.

## 📚 Documentation

- [docs/API.md](docs/API.md): the overloads and the `friendly`, `quote` and
  `max_size` parameters.
- [ARCHITECTURE.md](ARCHITECTURE.md): the escaping pass, quoting and truncation.
- [CHANGELOG.md](CHANGELOG.md): what changed, with numbers.
- [AGENTS.md](AGENTS.md): repository map, invariants, and traps.
- [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#repr): the catalog entry.

## 🛠️ Building and testing

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

`./build/repr_demo` runs the demo. `repr_diff` compares every output with the
frozen original, and the `*_swar` tests run both again on the portable backend.
Add `-DREPR_BUILD_BENCH=ON` to build `repr_bench`; with abseil installed it adds
the `absl::CEscape` column.

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) and the
catalog's [rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first; CI must pass on every platform.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo and Copyright (c) 2015-2020 Dubalu
LLC. See [LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).
