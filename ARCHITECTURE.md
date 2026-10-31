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

The output is allocated once at its exact size and then written through a raw
`char*` cursor. There is no reallocation and no zero-fill of a worst-case buffer.

1. **Length table.** A 256-entry table (one per quote mode: none, `'`, `"`) gives
   the output length of each byte: 1 (copied as is), 2 (`\n`, `\r`, `\t`, `\\`,
   the active quote) or 4 (`\xNN`).
2. **Measure.** The first pass sums the table lengths over the kept bytes, so
   `ret.resize(n)` is exact (plus the two quotes and the `...`, when present).
3. **Write.** The second pass handles eight bytes at a time: one 64-bit load is
   tested (SWAR) for "no byte below 0x20 or at/above 0x7f, no backslash, no
   quote". A clean block is copied with one 8-byte store; any other block goes
   byte by byte through the table. Escape-heavy input therefore pays one extra
   test per eight bytes, and printable text copies at memory speed.

`friendly == false` skips the tables: every kept byte is `\xNN`, so the size is
`4 * kept bytes` and the pass is a straight hex write.

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
two-thirds of the budget to the head and one-third to the tail. The window is
computed with arithmetic on 1-based positions `k = index + 1` (the convention of
the original pointer loop, which pre-incremented its cursor): bytes with
`max_size * 2 / 3 <= k <= size - max_size / 3` are dropped, and the dots are
written where position `max_size * 2 / 3` would have been when that is at least 1.
The head and tail are then measured and written as two ranges, so the dropped
middle is never visited: the cost is O(max_size) plus the output, not O(size).
With `max_size == 0` nothing is dropped. For `max_size == 1` the result is just
the two quotes (the original sized its buffer at one byte here and wrote past it).

## Why this shape

The function exists to make a byte buffer safe to print on one line, for logs and
diagnostics, without dragging in a formatting library. An exact-size result and
block-wise copying keep it cheap: one allocation, two short passes, no per-byte
bounds checks.
The hex rendering is delegated to `char-classify` so this library and that one
agree on the lowercase two-digit format, and so neither reimplements a hex table.
It is ASCII-oriented by design: high bytes are escaped rather than interpreted,
because the point is a faithful, unambiguous, printable rendering of raw bytes,
not a Unicode-aware display.
