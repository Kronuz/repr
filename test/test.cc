// Smoke test for the standalone repr library.
//
// Exercises repr(): printable ASCII passes through, the friendly escapes
// (\n \r \t \\ and the active quote), \xNN for control / high bytes, the quote
// modes (default ', "1" -> ', '\0' for no quotes), the non-friendly all-hex
// mode, embedded NULs (it is size-based, not NUL-terminated), the void*/range
// and array overloads, and max_size truncation with "...".
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>

#include "repr.hh"


// ---------------------------------------------------------------------------
// Printable ASCII passes through untouched (inside the quotes).
// ---------------------------------------------------------------------------

static void test_printable_passthrough() {
	// Default quote is a single quote, so the result is wrapped in '...'.
	assert(repr(std::string_view("hello")) == "'hello'");
	assert(repr(std::string_view("AZaz09 ~!@#")) == "'AZaz09 ~!@#'");

	// Empty input still produces the two quotes and nothing between them.
	assert(repr(std::string_view("")) == "''");

	std::printf("repr passthrough OK: printable ASCII, empty\n");
}


// ---------------------------------------------------------------------------
// Friendly escapes: \n \r \t, backslash, and the active quote.
// ---------------------------------------------------------------------------

static void test_friendly_escapes() {
	// The named control escapes.
	assert(repr(std::string_view("a\nb")) == "'a\\nb'");
	assert(repr(std::string_view("a\rb")) == "'a\\rb'");
	assert(repr(std::string_view("a\tb")) == "'a\\tb'");

	// A literal backslash is doubled.
	assert(repr(std::string_view("a\\b")) == "'a\\\\b'");

	// The active quote inside the text is escaped (default quote is ').
	assert(repr(std::string_view("it's")) == "'it\\'s'");

	// With a double-quote requested, " is the escaped char and ' is left alone.
	assert(repr(std::string_view("say \"hi\""), true, '"') == "\"say \\\"hi\\\"\"");
	assert(repr(std::string_view("it's"), true, '"') == "\"it's\"");

	std::printf("repr friendly escapes OK: \\n \\r \\t \\\\ and active quote\n");
}


// ---------------------------------------------------------------------------
// Control / high bytes become \xNN (lowercase two-digit hex).
// ---------------------------------------------------------------------------

static void test_hex_escapes() {
	// 0x1b (ESC) is a control byte -> \x1b. 0x7f (DEL) is >= 0x7f -> \x7f.
	assert(repr(std::string_view("\x1b")) == "'\\x1b'");
	assert(repr(std::string_view("\x7f")) == "'\\x7f'");

	// A high (non-ASCII) byte. 0xff -> \xff.
	const unsigned char hi[] = {0xff};
	assert(repr(hi, sizeof(hi)) == "'\\xff'");

	// Mixed printable and control in one buffer.
	assert(repr(std::string_view("a\x01z")) == "'a\\x01z'");

	std::printf("repr hex escapes OK: control and high bytes -> \\xNN\n");
}


// ---------------------------------------------------------------------------
// Quote modes: default ', the '\1' alias for ', and '\0' for no quotes.
// ---------------------------------------------------------------------------

static void test_quote_modes() {
	// quote == '\0': no surrounding quotes, and the quote char is never escaped.
	assert(repr(std::string_view("it's"), true, '\0') == "it's");
	assert(repr(std::string_view("hello"), true, '\0') == "hello");

	// quote == '\1' is an alias for ' (single quote): used so a caller can ask
	// for quoting without writing a literal quote char.
	assert(repr(std::string_view("hi"), true, '\1') == "'hi'");

	std::printf("repr quote modes OK: default ', '\\1' alias, '\\0' none\n");
}


// ---------------------------------------------------------------------------
// Non-friendly mode: every byte is rendered as \xNN.
// ---------------------------------------------------------------------------

static void test_unfriendly_all_hex() {
	// friendly == false: even printable characters become \xNN.
	assert(repr(std::string_view("AB"), false) == "'\\x41\\x42'");
	assert(repr(std::string_view("\n"), false) == "'\\x0a'");

	std::printf("repr non-friendly OK: every byte -> \\xNN\n");
}


// ---------------------------------------------------------------------------
// Embedded NULs: repr is size-based, not NUL-terminated, so it does not stop
// at a \0 and renders it as \x00.
// ---------------------------------------------------------------------------

static void test_embedded_nuls() {
	const char buf[] = {'a', '\0', 'b'};
	assert(repr(buf, sizeof(buf)) == "'a\\x00b'");

	// A std::string that itself contains a NUL keeps its full length.
	std::string s("x\0y", 3);
	assert(repr(s) == "'x\\x00y'");

	std::printf("repr embedded NULs OK: \\x00 rendered, length preserved\n");
}


// ---------------------------------------------------------------------------
// The pointer/range and array overloads agree with the size-based one.
// ---------------------------------------------------------------------------

static void test_overloads() {
	const char* data = "abc\n";

	// (void* p, size_t size)
	std::string by_size = repr(data, std::size_t{4});
	// (void* p, void* e) range form: e points one past the last byte.
	std::string by_range = repr(static_cast<const void*>(data),
	                            static_cast<const void*>(data + 4));
	assert(by_size == by_range);
	assert(by_size == "'abc\\n'");

	// Array overload: a string literal's trailing NUL is dropped (N - 1).
	assert(repr("abc\n") == "'abc\\n'");

	// std::string and std::string_view overloads.
	std::string str("abc\n");
	assert(repr(str) == "'abc\\n'");
	assert(repr(std::string_view("abc\n")) == "'abc\\n'");

	std::printf("repr overloads OK: size/range/array/string/string_view agree\n");
}


// ---------------------------------------------------------------------------
// max_size truncation: long inputs are shortened with "..." in the middle.
// ---------------------------------------------------------------------------

static void test_truncation() {
	// With a small max_size, a long all-printable input keeps a head and a tail
	// with "..." between them, and stays well under the untruncated length.
	std::string full = repr(std::string_view("abcdefghijklmnopqrstuvwxyz"));
	std::string cut = repr(std::string_view("abcdefghijklmnopqrstuvwxyz"), true, '\'', 9);

	assert(cut.find("...") != std::string::npos);
	assert(cut.size() < full.size());
	// Still quoted and still anchored on the original head.
	assert(cut.front() == '\'' && cut.back() == '\'');
	assert(cut[1] == 'a');

	// A short input under the budget is not truncated at all.
	std::string small = repr(std::string_view("abc"), true, '\'', 64);
	assert(small == "'abc'");
	assert(small.find("...") == std::string::npos);

	std::printf("repr truncation OK: max_size inserts \"...\", short input untouched\n");
}


int main() {
	test_printable_passthrough();
	test_friendly_escapes();
	test_hex_escapes();
	test_quote_modes();
	test_unfriendly_all_hex();
	test_embedded_nuls();
	test_overloads();
	test_truncation();
	std::printf("all repr tests passed\n");
	return 0;
}
