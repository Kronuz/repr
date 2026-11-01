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

namespace {

inline char* emit_hex(char* d, const unsigned char* b, const unsigned char* e) {
	while (b != e) {
		*d++ = '\\';
		*d++ = 'x';
		chars::char_repr(static_cast<char>(*b++), &d);
	}
	return d;
}

}  // namespace


std::string repr(const void* p, size_t size, bool friendly, char quote, size_t max_size) {
	assert(quote == '\0' || quote == '\1' || quote == '\'' || quote == '"');
	const auto* b = static_cast<const unsigned char *>(p);
	if (quote == '\1') { quote = '\''; }

	// Elided window, in 1-based positions k = index + 1: bytes with
	// lo <= k <= hi are skipped, and "..." is written in place of position lo
	// when lo >= 1.
	std::size_t head = size;  // bytes kept from the start
	std::size_t tail = size;  // index where the kept tail starts
	bool dots = false;
	if (max_size != 0u) {
		long long lo = static_cast<long long>(max_size * 2 / 3);
		long long hi = static_cast<long long>(size) - static_cast<long long>(max_size / 3);
		long long es = lo < 1 ? 1 : lo;
		long long ee = hi > static_cast<long long>(size) ? static_cast<long long>(size) : hi;
		if (es <= ee) {
			head = static_cast<std::size_t>(es - 1);
			tail = static_cast<std::size_t>(ee);
			dots = lo >= 1;
		}
	}

	chars::EscapeTable custom;
	const chars::EscapeTable& t = chars::escape_table(friendly ? quote : '\0', custom);  // custom: not reachable under the assert

	const unsigned char* const e = b + size;
	std::size_t n = (quote != '\0' ? 2 : 0) + (dots ? 3 : 0);
	if (friendly) {
		n += chars::escape_measure(b, b + head, t) + chars::escape_measure(b + tail, e, t);
	} else {
		n += (head + (size - tail)) * 4;
	}

	std::string ret;
	ret.resize(n);
	char* d = &ret[0];
	if (quote != '\0') { *d++ = quote; }
	if (friendly) {
		d = chars::escape_emit(d, b, b + head, t);
		if (dots) { *d++ = '.'; *d++ = '.'; *d++ = '.'; }
		d = chars::escape_emit(d, b + tail, e, t);
	} else {
		d = emit_hex(d, b, b + head);
		if (dots) { *d++ = '.'; *d++ = '.'; *d++ = '.'; }
		d = emit_hex(d, b + tail, e);
	}
	if (quote != '\0') { *d++ = quote; }
	assert(static_cast<std::size_t>(d - &ret[0]) == n);
	return ret;
}
