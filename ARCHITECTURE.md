# Architecture

The internals of `repr`: how one pass over the input buffer produces a
printable, escaped string, why the output buffer is pre-sized, and how
truncation, quoting, and the hex escape work. For usage see `README.md`; for the
repo map and invariants see `AGENTS.md`.

## Shape

A header of overloads and a single compiled function:

```
  repr.hh   declares repr() and its convenience overloads (range, string, array)
  repr.cc   the one real function: a pre-sized, single-pass escaping loop
```

All the overloads in `repr.hh` are thin wrappers that normalize their argument to
`(const void* p, std::size_t size)` and call the core function in `repr.cc`. The
range form subtracts the two pointers; the `std::string` / `std::string_view`
forms pass `.data()` / `.size()`; the array form passes `N - 1` to drop a string
literal's trailing NUL.

## The escaping pass

The core function walks the buffer once, byte by byte, writing into a
pre-allocated output buffer. There is no reallocation in the loop: the output is
sized up front to the worst case before the first byte is written.

The worst case is four output bytes per input byte (`\xNN`), plus the two quotes:

```cpp
size = size * 4 + 2;            // every byte as \xNN, plus two quotes
```

When `max_size` is set, the budget is split into a head and a tail and the sizing
accounts for the `...` and the dropped middle instead. The buffer is allocated
with `ret.resize(size)`, written through a raw `char*` cursor, then trimmed with a
final `ret.resize(d - buff)` to the bytes actually produced. Pre-sizing to the
worst case is what lets the loop use a bare pointer with no bounds checks.

## What each byte becomes

For each byte, with `friendly` true:

- `\n`, `\r`, `\t` get their readable two-character escapes (`\n`, `\r`, `\t`).
- A literal backslash is doubled (`\\`).
- The active quote character, if it appears in the text, is escaped (`\'` or
  `\"`).
- Anything below `' '` (0x20) or at/above `0x7f` is a control or high byte and is
  written as `\x` followed by two lowercase hex digits.
- Everything else is printable ASCII and is copied through unchanged.

With `friendly` false, the switch is skipped entirely and *every* byte is written
as `\xNN`, which gives a pure hex view with nothing interpreted.

The two hex digits come from `chars::char_repr` in the
[char-classify](https://github.com/Kronuz/char-classify) sibling library, called
as `chars::char_repr(c, &d)`: it writes the lowercase hex of the byte through the
write cursor and advances it by two. That is the one external dependency, and the
only reason this library is not header-only-trivial.

## Quoting

`quote` controls the surrounding character and which byte gets escaped inside the
text. The default is a single quote. Two sentinel values shape the behavior:

- `'\0'` means no surrounding quotes are written, and because there is no active
  quote, no quote byte is escaped inside the text.
- `'\1'` is remapped to `'\''` at the top of the function. It exists so a caller
  can request single-quoting through a flag-like value without writing a literal
  quote character.

An `assert` at the entry pins `quote` to one of `'\0'`, `'\1'`, `'\''`, or `'"'`;
those are the only values the escaping logic is written for.

## Truncation

When `max_size` is non-zero, the function keeps a head and a tail of the input and
drops the middle, inserting a literal `...` at the cut. The split is roughly
two-thirds of the budget to the head and one-third to the tail, computed as two
pointers (`max_a`, `max_b`) into the input. The main loop checks the read cursor
against that window: bytes inside the window are skipped, and at the moment the
cursor first crosses `max_a` it emits the three dots. So the head is rendered
normally, the dropped span produces a single `...`, and the tail is rendered
normally. With `max_size == 0` the window is set so nothing is ever skipped.

## Why this shape

The function exists to make a byte buffer safe to print on one line, for logs and
diagnostics, without dragging in a formatting library. A single pre-sized pass
keeps it cheap: one allocation, one walk, one trim, no per-byte bounds checks.
The hex rendering is delegated to `char-classify` so this library and that one
agree on the lowercase two-digit format, and so neither reimplements a hex table.
It is ASCII-oriented by design: high bytes are escaped rather than interpreted,
because the point is a faithful, unambiguous, printable rendering of raw bytes,
not a Unicode-aware display.
