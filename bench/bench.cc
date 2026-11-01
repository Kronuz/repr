// repr/escape benchmark: repr() against the frozen reference implementation
// (test/repr_old.cc) and, when abseil is available, absl::CEscape (whose output
// differs, so it is a speed reference only). Build with -DREPR_BUILD_BENCH=ON.
// Reports ns per call, the best of seven samples of at least 8 ms; the old and the new
// implementation are sampled in turn, so a drift of the host or of the clock hits both.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "repr.hh"
#ifdef HAVE_ABSL
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

// The best of seven samples of each, taken alternately.
template <typename F, typename G>
static void best_pair(F&& f, G&& g, double& best_f, double& best_g) {
	best_f = best_g = 1e18;
	for (int s = 0; s < 7; ++s) {
		best_f = std::min(best_f, sample_ns(f));
		best_g = std::min(best_g, sample_ns(g));
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
	std::printf("%-28s %12s %12s %8s %12s\n", "workload", "old ns", "new ns", "x", "absl ns");
	struct W { const char* name; size_t n; int dirty; size_t max; int ctl; };
	const W ws[] = {
		{"printable 1 B", 1, 0, 0, 0}, {"printable 8 B", 8, 0, 0, 0},
		{"printable 16 B", 16, 0, 0, 0}, {"printable 256 B", 256, 0, 0, 0}, {"printable 4 KB", 4096, 0, 0, 0},
		{"printable 64 KB", 65536, 0, 0, 0}, {"mixed 10% 256 B", 256, 10, 0, 0}, {"mixed 10% 64 KB", 65536, 10, 0, 0},
		{"ctl 1% 4 KB", 4096, 0, 0, 1}, {"ctl 10% 256 B", 256, 0, 0, 10}, {"ctl 10% 4 KB", 4096, 0, 0, 10},
		{"ctl 10% 64 KB", 65536, 0, 0, 10}, {"ctl 50% 4 KB", 4096, 0, 0, 50},
		{"binary 4 KB", 4096, 100, 0, 0}, {"max_size=64 on 4 KB", 4096, 0, 64, 0}, {"max_size=64 on 64 KB", 65536, 0, 64, 0},
	};
	for (const auto& w : ws) {
		std::string s = w.ctl ? make_ctl(w.n, w.ctl) : make(w.n, w.dirty);
		double o = 0, n = 0;
		best_pair([&] { sink = repr_old(s.data(), s.size(), true, '\'', w.max).size(); },
			[&] { sink = repr(s.data(), s.size(), true, '\'', w.max).size(); }, o, n);
		double a = 0;
#ifdef HAVE_ABSL
		if (w.max == 0) { a = best_ns([&] { sink = absl::CEscape(s).size(); }); }
#endif
		std::printf("%-28s %12.1f %12.1f %8.2f %12.1f\n", w.name, o, n, o / n, a);
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
	for (size_t n : {16, 4096}) {
		std::string s(n, '\0');
		for (size_t i = 0; i < n; ++i) { s[i] = static_cast<char>(i * 37); }
		edges.push_back({"friendly=false " + std::to_string(n) + " B", s, false, '\''});
	}
	for (const auto& e : edges) {
		double o = 0, n = 0;
		best_pair([&] { sink = repr_old(e.s.data(), e.s.size(), e.friendly, e.quote, 0).size(); },
			[&] { sink = repr(e.s.data(), e.s.size(), e.friendly, e.quote, 0).size(); }, o, n);
		std::printf("%-28s %12.1f %12.1f %8.2f %12.1f\n", e.name.c_str(), o, n, o / n, 0.0);
	}
	return 0;
}
