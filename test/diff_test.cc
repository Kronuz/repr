// Differential test: repr() must match the frozen pre-optimization
// implementation (repr_old, test/repr_old.cc) byte for byte on random and edge
// inputs, across every quote mode, friendly/non-friendly and max_size values
// around the truncation boundaries.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

#include "repr.hh"

std::string repr_old(const void* p, size_t size, bool friendly, char quote, size_t max_size);

static int failures = 0;

static void check(const std::string& in, bool friendly, char quote, size_t max_size) {
	if (max_size == 1 && quote != '\0') {
		// The old code sizes its buffer at one byte here and writes both quotes past it
		// (the result ends in a stray NUL). The new code returns the two quotes.
		char q = quote == '\1' ? '\'' : quote;
		if (repr(in.data(), in.size(), friendly, quote, 1) != std::string(2, q) && !in.empty()) {
			std::fprintf(stderr, "max_size=1 expected two quotes\n");
			++failures;
		}
		return;
	}
	std::string a = repr(in.data(), in.size(), friendly, quote, max_size);
	std::string b = repr_old(in.data(), in.size(), friendly, quote, max_size);
	if (a != b) {
		if (++failures < 10) {
			std::fprintf(stderr, "MISMATCH size=%zu friendly=%d quote=%d max=%zu\n  new=%s\n  old=%s\n",
				in.size(), friendly, quote, max_size, a.c_str(), b.c_str());
		}
	}
}

int main() {
	std::mt19937_64 rng(12345);
	const char quotes[] = {'\0', '\1', '\'', '"'};
	// Byte distributions: printable only, mixed, binary, escape-heavy.
	for (int iter = 0; iter < 6000; ++iter) {
		size_t n = rng() % 200;
		if (iter % 50 == 0) { n = rng() % 5000; }
		int mode = iter % 4;
		std::string s(n, '\0');
		for (auto& c : s) {
			unsigned r = static_cast<unsigned>(rng());
			switch (mode) {
				case 0: c = static_cast<char>(0x20 + r % 95); break;
				case 1: c = (r % 10 == 0) ? static_cast<char>(r >> 8) : static_cast<char>(0x20 + (r >> 8) % 95); break;
				case 2: c = static_cast<char>(r >> 8); break;
				default: {
					const char sp[] = {'\n', '\r', '\t', '\\', '\'', '"', 'a', 0x7f, 0x1f, 0x20, 0x7e, '\0', char(0x80), char(0xff)};
					c = sp[(r >> 8) % sizeof(sp)];
				}
			}
		}
		for (char q : quotes) {
			for (bool f : {true, false}) {
				check(s, f, q, 0);
				size_t ms[] = {1, 2, 3, 4, 5, 6, 7, 9, 10, 63, 64, 65, n, n + 1, n * 3 / 2, rng() % (n + 8) + 1};
				for (size_t m : ms) { check(s, f, q, m); }
			}
		}
	}
	// Exhaustive boundaries on short printable inputs: every size x every max_size.
	for (size_t n = 0; n <= 40; ++n) {
		std::string s(n, 'x');
		for (size_t i = 0; i < n; ++i) { s[i] = static_cast<char>('a' + i % 26); }
		for (size_t m = 0; m <= 70; ++m) {
			for (char q : quotes) { for (bool f : {true, false}) { check(s, f, q, m); } }
		}
	}
	// Every single byte value in every position of an 8..17 byte window.
	for (int c = 0; c < 256; ++c) {
		for (size_t n = 1; n <= 17; ++n) {
			for (size_t pos = 0; pos < n; ++pos) {
				std::string s(n, 'k');
				s[pos] = static_cast<char>(c);
				for (char q : quotes) { check(s, true, q, 0); check(s, false, q, 0); }
			}
		}
	}
	if (failures != 0) {
		std::fprintf(stderr, "%d mismatches\n", failures);
		return 1;
	}
	std::printf("repr differential OK\n");
	return 0;
}
