/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "repr.hh"

#include <cassert>               // for assert

#include "chars.hh"              // for chars::char_repr
#include "chars_escape.hh"       // for chars::escape_measure, chars::escape_emit


// How repr works (see ARCHITECTURE.md; the escaping kernel is char-classify's
// chars_escape.hh, shared with escape()):
//
// 1. A 256-entry table gives the output length of each byte (1 = copied as is,
//    2 = backslash escape, 4 = \xNN), per quote mode.
// 2. A first pass sums the lengths of the kept bytes (head and tail), so the
//    result is allocated once at its exact size.
// 3. A second pass writes it. Blocks of plain printable bytes are tested with SIMD
//    (SWAR where there is none) and bulk-copied; the bytes that need an escape go
//    through the table.
// 4. With max_size, the elided middle is never visited: head and tail are
//    located by arithmetic, so the cost is O(max_size) plus the output.
//
// The entry point picks one of three paths. Input of fewer than chars::escape_scalar_max bytes
// with nothing elided is written in the entry point itself, a byte at a time: friendly=true in
// the same two passes, or one pass and a trim when even the worst case (four bytes for each)
// fits a small string, and friendly=false as a loop of \xNN. The rest goes to repr_hex
// (friendly=false, no table) or repr_blocks. Each of those is out of line and works out the
// window itself, so the entry point is a size test and a tail call for them: the table scratch
// and the registers the block loops use are a frame that the short path does not pay for.

namespace {

// The part of the input that is kept: bytes [0, head) and [tail, size), and "..." between
// them when dots. Nothing is elided when head == tail == size.
struct Window {
	std::size_t head;
	std::size_t tail;
	bool dots;
};

// In 1-based positions k = index + 1, the bytes with lo <= k <= hi are skipped, and "..."
// is written in place of position lo when lo >= 1.
inline Window window(std::size_t size, std::size_t max_size) {
	Window w{size, size, false};
	if (max_size != 0u) {
		long long lo = static_cast<long long>(max_size * 2 / 3);
		long long hi = static_cast<long long>(size) - static_cast<long long>(max_size / 3);
		long long es = lo < 1 ? 1 : lo;
		long long ee = hi > static_cast<long long>(size) ? static_cast<long long>(size) : hi;
		if (es <= ee) {
			w.head = static_cast<std::size_t>(es - 1);
			w.tail = static_cast<std::size_t>(ee);
			w.dots = lo >= 1;
		}
	}
	return w;
}

inline char* emit_hex(char* d, const unsigned char* b, const unsigned char* e) {
	while (b != e) {
		*d++ = '\\';
		*d++ = 'x';
		chars::char_repr(static_cast<char>(*b++), &d);
	}
	return d;
}

CHARS_NOINLINE std::string repr_hex(const unsigned char* b, std::size_t size, char quote, std::size_t max_size) {
	const Window w = window(size, max_size);
	const unsigned char* const e = b + size;
	const std::size_t n = (quote != '\0' ? 2 : 0) + (w.dots ? 3 : 0) + (w.head + (size - w.tail)) * 4;
	std::string ret;
	ret.resize(n);
	char* d = &ret[0];
	if (quote != '\0') { *d++ = quote; }
	d = emit_hex(d, b, b + w.head);
	if (w.dots) { *d++ = '.'; *d++ = '.'; *d++ = '.'; }
	d = emit_hex(d, b + w.tail, e);
	if (quote != '\0') { *d++ = quote; }
	assert(static_cast<std::size_t>(d - &ret[0]) == n);
	return ret;
}

CHARS_NOINLINE std::string repr_blocks(const unsigned char* b, std::size_t size, char quote, std::size_t max_size) {
	const Window w = window(size, max_size);
	chars::EscapeTable custom;
	const chars::EscapeTable& t = chars::escape_table(quote, custom);  // custom: not reachable under the assert

	const unsigned char* const e = b + size;
	const std::size_t n = (quote != '\0' ? 2 : 0) + (w.dots ? 3 : 0) +
		chars::escape_measure(b, b + w.head, t) + chars::escape_measure(b + w.tail, e, t);

	std::string ret;
	ret.resize(n);
	char* d = &ret[0];
	if (quote != '\0') { *d++ = quote; }
	d = chars::escape_emit(d, b, b + w.head, t);
	if (w.dots) { *d++ = '.'; *d++ = '.'; *d++ = '.'; }
	d = chars::escape_emit(d, b + w.tail, e, t);
	if (quote != '\0') { *d++ = quote; }
	assert(static_cast<std::size_t>(d - &ret[0]) == n);
	return ret;
}

}  // namespace


std::string repr(const void* p, size_t size, bool friendly, char quote, size_t max_size) {
	assert(quote == '\0' || quote == '\1' || quote == '\'' || quote == '"');
	const auto* b = static_cast<const unsigned char *>(p);
	if (quote == '\1') { quote = '\''; }

	// Nothing is elided when the window keeps every byte (head == size).
	if (size < chars::escape_scalar_max && (max_size == 0u || window(size, max_size).head == size)) {
		const unsigned char* const e = b + size;
		if (!friendly) {
			const std::size_t n = (quote != '\0' ? 2 : 0) + size * 4;
			std::string ret;
			ret.resize(n);
			char* d = &ret[0];
			if (quote != '\0') { *d++ = quote; }
			d = emit_hex(d, b, e);
			if (quote != '\0') { *d++ = quote; }
			assert(static_cast<std::size_t>(d - &ret[0]) == n);
			return ret;
		}
		if (const chars::EscapeTable* t = chars::escape_table_shared(quote)) {
			std::string ret;
			// A result that fits the small-string buffer even if every byte takes four is written in
			// one pass over that worst case, then trimmed. Any other is measured first and allocated
			// at its exact size.
			const std::size_t most = size * 4 + 2;
			if (most <= std::string().capacity()) {
				ret.resize(most);
				char* const s = &ret[0];
				char* d = s;
				if (quote != '\0') { *d++ = quote; }
				d = chars::escape_emit_scalar(d, b, e, *t);
				if (quote != '\0') { *d++ = quote; }
				ret.resize(static_cast<std::size_t>(d - s));
				return ret;
			}
			const std::size_t n = (quote != '\0' ? 2 : 0) + chars::escape_measure_scalar(b, e, *t);
			ret.resize(n);
			char* d = &ret[0];
			if (quote != '\0') { *d++ = quote; }
			d = chars::escape_emit_scalar(d, b, e, *t);
			if (quote != '\0') { *d++ = quote; }
			assert(static_cast<std::size_t>(d - &ret[0]) == n);
			return ret;
		}
	}
	if (!friendly) { return repr_hex(b, size, quote, max_size); }
	return repr_blocks(b, size, quote, max_size);
}

