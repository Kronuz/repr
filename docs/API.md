# repr API reference

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
