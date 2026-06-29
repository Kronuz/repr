# repr

A small C++20 utility that turns an arbitrary byte buffer into a single-line,
printable, copy-pasteable string. Extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).

## What it is

One function, `repr()`, plus a few convenience overloads. You hand it raw bytes
(a pointer and a size, a `std::string`, a `std::string_view`, a string literal,
or a `[begin, end)` range) and it gives you back a `std::string` that is safe to
print: printable ASCII passes through untouched, `\n` / `\r` / `\t` become the
readable two-character escapes, a literal backslash is doubled, the active quote
character is escaped, and every other control or high byte is rendered as `\xNN`
in lowercase hex. The result is wrapped in quotes by default.

It is what you reach for when you need to log or compare a buffer that might
contain control characters, binary data, or embedded NULs, and you don't want
your terminal or log file mangled. It is size-based, not NUL-terminated, so it
renders the whole buffer including any `\0` in the middle.

## Install

This is not header-only. It ships a compiled translation unit, `repr.cc`, plus
the header `repr.hh`. You build and link the `.cc`; the header alone only
declares the overloads. It requires C++20.

It has one dependency: [char-classify](https://github.com/Kronuz/char-classify),
the sibling header that provides `chars::char_repr` (the byte-to-hex writer).
CMake pulls it in for you.

With CMake `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  repr
  GIT_REPOSITORY https://github.com/Kronuz/repr.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(repr)

target_link_libraries(your_target PRIVATE repr::repr)
```

`CMakeLists.txt` requests `cxx_std_20` `PUBLIC` and links `char-classify`
`PUBLIC`, so `"chars.hh"` resolves on the include path with no extra wiring on
your side. Then:

```cpp
#include "repr.hh"
```

The header keeps its original filename, so a codebase that already
`#include "repr.hh"` just needs this repo on its include path.

## Usage

```cpp
#include "repr.hh"

// Printable ASCII passes through, wrapped in single quotes by default.
repr(std::string_view("hello"));          // 'hello'

// Control bytes and high bytes become \xNN; \n \r \t get readable escapes.
repr(std::string_view("a\nb\x1b"));       // 'a\nb\x1b'

// It is size-based, so an embedded NUL is rendered, not a terminator.
repr(std::string("x\0y", 3));             // 'x\x00y'

// Pick the quote character, or '\0' for no surrounding quotes.
repr(std::string_view("it's"), true, '"');   // "it's"
repr(std::string_view("it's"), true, '\0');  // it's

// friendly = false dumps every byte as hex, nothing interpreted.
repr(std::string_view("AB"), false);      // '\x41\x42'

// max_size truncates a long buffer with "..." in the middle.
repr(big_buffer, true, '\'', 24);         // 'xxxxxxxxxxxxxxx...xxxxTAIL'
```

## API reference

```cpp
std::string repr(const void* p, std::size_t size,
                 bool friendly = true, char quote = '\'', std::size_t max_size = 0);
```

The core overload. `p`/`size` is the byte buffer. The convenience overloads all
forward to it:

- `repr(const void* p, const void* e, ...)` — a `[p, e)` range; `e` is one past
  the last byte.
- `repr(const std::string& / std::string_view, ...)` — uses `.data()` / `.size()`.
- `repr(T (&&literal)[N], ...)` — a string-literal array; drops the trailing NUL
  (`N - 1`).

Parameters:

- **`friendly`** (default `true`) — when true, printable ASCII passes through and
  `\n` / `\r` / `\t` / `\\` / the active quote get readable escapes; everything
  else is `\xNN`. When false, *every* byte is rendered as `\xNN`.
- **`quote`** (default `'\''`) — the surrounding quote character, also escaped
  when it appears inside the text. `'\0'` means no surrounding quotes (and the
  quote char is then never escaped). `'\1'` is an alias for `'\''`, so a caller
  can ask for single-quoting without writing a literal quote.
- **`max_size`** (default `0`, meaning no limit) — when non-zero, long buffers are
  truncated to roughly this many output bytes, keeping a head and a tail with
  `...` between them. Short inputs under the budget are left untouched.

## Build & test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first configure fetches `char-classify` over the network. The test exercises
printable passthrough, the friendly escapes (`\n` `\r` `\t` `\\` and the active
quote), `\xNN` for control and high bytes, the quote modes (default, double,
`'\0'`, the `'\1'` alias), the non-friendly all-hex mode, embedded NULs, the
range / array / string overloads, and `max_size` truncation. It prints
`all repr tests passed` and exits 0.

## Examples

[`examples/demo.cc`](examples/demo.cc) is a runnable tour. A top-level CMake build
produces it next to the test:

```sh
cmake -B build && cmake --build build && ./build/repr_demo
```

It renders a printable line, dumps a buffer with ESC / NUL / a high byte as
`\xNN`, shows the three quoting modes side by side, dumps a buffer in
non-friendly all-hex mode, and truncates a long buffer with `max_size`.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand). `repr.hh` /
`repr.cc` were copied verbatim; the only change was wiring its one local include,
`"chars.hh"`, to the standalone
[char-classify](https://github.com/Kronuz/char-classify) library through CMake.
No source edits, no decoupling delta. See [ARCHITECTURE.md](ARCHITECTURE.md) for
the design and [AGENTS.md](AGENTS.md) for the repo map and invariants.

## License

MIT, Copyright (c) 2015-2019 Dubalu LLC. See [LICENSE](LICENSE).
