// repr() benchmark. Columns:
//   repr     this library
//   loop     the obvious hand-written loop (naive_repr below): one append per byte to a std::string
//            with reserve(size + 2), the same escapes; its output must equal repr()'s, checked on
//            every input before it is timed
//   old      Xapiand's original implementation, frozen in test/repr_old.cc (same output)
//   absl     absl::CEscape, when abseil is linked (HAVE_ABSL): it writes a different format
//            (octal escapes, no quotes), so it is a speed reference only
// Reports ns per call, the best of seven samples of at least 8 ms; the implementations are sampled
// in turn, so a drift of the host or of the clock hits all of them. "A/A %" is how far apart two
// sets of samples of repr() are. Build with -DREPR_BUILD_BENCH=ON.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "repr.hh"
#ifdef HAVE_ABSL
#include "absl/base/config.h"
#include "absl/strings/escaping.h"
#endif

std::string repr_old(const void* p, size_t size, bool friendly, char quote, size_t max_size);

// One sample: ns per call over at least 8 ms.
template <typename F>
static double sample_ns(F&& f) {
	using clk = std::chrono::steady_clock;
	long iters = 0;
	auto t0 = clk::now();
	auto t1 = t0;
	do {
		for (int i = 0; i < 16; ++i) { f(); }
		iters += 16;
		t1 = clk::now();
	} while (t1 - t0 < std::chrono::milliseconds(8));
	return std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
}

template <typename F>
static double best_ns(F&& f) {
	double best = 1e18;
	for (int s = 0; s < 7; ++s) { best = std::min(best, sample_ns(f)); }
	return best;
}

// The best of seven samples of each of `f`, `g` and `h`, taken in turn, and of a second set of samples
// of `f`: `aa` is how far apart the two sets of `f` are, in percent (the noise of the row).
template <typename F, typename G, typename H>
static void best_trio(F&& f, G&& g, H&& h, double& best_f, double& best_g, double& best_h, double& aa) {
	best_f = best_g = best_h = 1e18;
	double best_f2 = 1e18;
	for (int s = 0; s < 7; ++s) {
		best_f = std::min(best_f, sample_ns(f));
		best_g = std::min(best_g, sample_ns(g));
		best_h = std::min(best_h, sample_ns(h));
		best_f2 = std::min(best_f2, sample_ns(f));
	}
	aa = 100.0 * (best_f2 > best_f ? best_f2 - best_f : best_f - best_f2) / best_f;
}

// The loop most code would write: append to a reserved std::string one byte at a time.
static std::string naive_repr(const std::string& s, bool friendly, char quote) {
	static const char digits[] = "0123456789abcdef";
	std::string r;
	r.reserve(s.size() + 2);
	if (quote != '\0') { r.push_back(quote); }
	for (unsigned char c : s) {
		if (friendly && c == '\n') { r += "\\n"; }
		else if (friendly && c == '\r') { r += "\\r"; }
		else if (friendly && c == '\t') { r += "\\t"; }
		else if (friendly && c == '\\') { r += "\\\\"; }
		else if (friendly && quote != '\0' && c == static_cast<unsigned char>(quote)) { r.push_back('\\'); r.push_back(quote); }
		else if (!friendly || c < ' ' || c >= 0x7f) { r += "\\x"; r.push_back(digits[c >> 4]); r.push_back(digits[c & 15]); }
		else { r.push_back(static_cast<char>(c)); }
	}
	if (quote != '\0') { r.push_back(quote); }
	return r;
}

static int bad = 0;
static void check_same(const char* name, const std::string& s, bool friendly, char quote) {
	if (naive_repr(s, friendly, quote) != repr(s.data(), s.size(), friendly, quote, 0)) {
		std::printf("MISMATCH between repr and the hand-written loop on %s\n", name);
		++bad;
	}
}

static volatile size_t sink;

static std::string make(size_t n, int dirty_pct) {
	std::mt19937 rng(1);
	std::string s(n, 'a');
	for (auto& c : s) {
		c = (dirty_pct && static_cast<int>(rng() % 100) < dirty_pct) ? static_cast<char>(rng()) : static_cast<char>(0x20 + rng() % 95);
		if (dirty_pct == 100) { c = static_cast<char>(rng() | 0x80); }
	}
	return s;
}

// Printable text where `pct` percent of the bytes are control characters (every one needs an escape).
static std::string make_ctl(size_t n, int pct) {
	std::mt19937 rng(2);
	std::string s(n, 'a');
	for (auto& c : s) { c = (static_cast<int>(rng() % 100) < pct) ? static_cast<char>(rng() % 32) : static_cast<char>(0x20 + rng() % 95); }
	return s;
}

int main() {
#ifdef HAVE_ABSL
	std::printf("# absl::CEscape from abseil %d.%d\n", ABSL_LTS_RELEASE_VERSION, ABSL_LTS_RELEASE_PATCH_LEVEL);
#endif
	std::printf("%-28s %10s %10s %10s %10s %7s\n", "workload", "repr ns", "loop ns", "old ns", "absl ns", "A/A %");
	struct W { const char* name; size_t n; int dirty; size_t max; int ctl; };
	const W ws[] = {
		{"printable 1 B", 1, 0, 0, 0}, {"printable 4 B", 4, 0, 0, 0}, {"printable 7 B", 7, 0, 0, 0}, {"printable 8 B", 8, 0, 0, 0},
		{"printable 15 B", 15, 0, 0, 0},
		{"printable 16 B", 16, 0, 0, 0}, {"printable 256 B", 256, 0, 0, 0}, {"printable 4 KB", 4096, 0, 0, 0},
		{"printable 64 KB", 65536, 0, 0, 0}, {"mixed 10% 256 B", 256, 10, 0, 0}, {"mixed 10% 64 KB", 65536, 10, 0, 0},
		{"ctl 1% 4 KB", 4096, 0, 0, 1}, {"ctl 10% 256 B", 256, 0, 0, 10}, {"ctl 10% 4 KB", 4096, 0, 0, 10},
		{"ctl 10% 64 KB", 65536, 0, 0, 10}, {"ctl 50% 4 KB", 4096, 0, 0, 50},
		{"binary 4 KB", 4096, 100, 0, 0}, {"max_size=64 on 4 B", 4, 0, 64, 0}, {"max_size=64 on 4 KB", 4096, 0, 64, 0},
		{"max_size=64 on 64 KB", 65536, 0, 64, 0},
	};
	for (const auto& w : ws) {
		std::string s = w.ctl ? make_ctl(w.n, w.ctl) : make(w.n, w.dirty);
		if (w.max == 0) { check_same(w.name, s, true, '\''); }
		double r = 0, l = 0, o = 0, aa = 0;
		best_trio([&] { sink = repr(s.data(), s.size(), true, '\'', w.max).size(); },
			[&] { sink = w.max == 0 ? naive_repr(s, true, '\'').size() : 0; },
			[&] { sink = repr_old(s.data(), s.size(), true, '\'', w.max).size(); }, r, l, o, aa);
		double a = 0;
#ifdef HAVE_ABSL
		if (w.max == 0) { a = best_ns([&] { sink = absl::CEscape(s).size(); }); }
#endif
		std::printf("%-28s %10.1f %10.1f %10.1f %10.1f %7.1f\n", w.name, r, w.max == 0 ? l : 0.0, o, a, aa);
	}

	// The rows above leave the options at their defaults and start at one byte of printable text.
	// These are the inputs where a fixed cost shows: one to sixteen printable bytes without a
	// quote, and friendly=false, where every byte becomes \xNN.
	struct Edge { std::string name; std::string s; bool friendly; char quote; };
	std::vector<Edge> edges;
	for (size_t n : {1, 4, 8, 16}) {
		std::string s(n, 'a');
		for (size_t i = 0; i < n; ++i) { s[i] = static_cast<char>('a' + i % 26); }
		edges.push_back({"printable " + std::to_string(n) + " B, no quote", s, true, '\0'});
	}
	for (size_t n : {1, 16, 4096}) {
		std::string s(n, '\0');
		for (size_t i = 0; i < n; ++i) { s[i] = static_cast<char>(i * 37); }
		edges.push_back({"friendly=false " + std::to_string(n) + " B", s, false, '\''});
	}
	for (const auto& e : edges) {
		check_same(e.name.c_str(), e.s, e.friendly, e.quote);
		double r = 0, l = 0, o = 0, aa = 0;
		best_trio([&] { sink = repr(e.s.data(), e.s.size(), e.friendly, e.quote, 0).size(); },
			[&] { sink = naive_repr(e.s, e.friendly, e.quote).size(); },
			[&] { sink = repr_old(e.s.data(), e.s.size(), e.friendly, e.quote, 0).size(); }, r, l, o, aa);
		std::printf("%-28s %10.1f %10.1f %10.1f %10.1f %7.1f\n", e.name.c_str(), r, l, o, 0.0, aa);
	}
	if (bad) { std::printf("%d inputs where repr and the hand-written loop differ\n", bad); }
	return bad ? 1 : 0;
}
