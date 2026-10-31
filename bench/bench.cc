// repr/escape benchmark: new repr() against the frozen old implementation
// (test/repr_old.cc) and, when abseil is available, absl::CEscape (whose output
// differs, so it is a speed reference only). Build with -DREPR_BUILD_BENCH=ON.
// Reports the best of several samples in ns per call.
#include <chrono>
#include <cstdio>
#include <random>
#include <string>

#include "repr.hh"
#ifdef HAVE_ABSL
#include "absl/strings/escaping.h"
#endif

std::string repr_old(const void* p, size_t size, bool friendly, char quote, size_t max_size);

template <typename F>
static double best_ns(F&& f) {
	using clk = std::chrono::steady_clock;
	double best = 1e18;
	for (int s = 0; s < 7; ++s) {
		long iters = 0;
		auto t0 = clk::now();
		auto t1 = t0;
		do {
			for (int i = 0; i < 16; ++i) { f(); }
			iters += 16;
			t1 = clk::now();
		} while (t1 - t0 < std::chrono::milliseconds(8));
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
		if (ns < best) { best = ns; }
	}
	return best;
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

int main() {
	std::printf("%-28s %12s %12s %8s %12s\n", "workload", "old ns", "new ns", "x", "absl ns");
	struct W { const char* name; size_t n; int dirty; size_t max; };
	const W ws[] = {
		{"printable 16 B", 16, 0, 0}, {"printable 256 B", 256, 0, 0}, {"printable 4 KB", 4096, 0, 0},
		{"printable 64 KB", 65536, 0, 0}, {"mixed 10% 256 B", 256, 10, 0}, {"mixed 10% 64 KB", 65536, 10, 0},
		{"binary 4 KB", 4096, 100, 0}, {"max_size=64 on 4 KB", 4096, 0, 64}, {"max_size=64 on 64 KB", 65536, 0, 64},
	};
	for (const auto& w : ws) {
		std::string s = make(w.n, w.dirty);
		double o = best_ns([&] { sink = repr_old(s.data(), s.size(), true, '\'', w.max).size(); });
		double n = best_ns([&] { sink = repr(s.data(), s.size(), true, '\'', w.max).size(); });
		double a = 0;
#ifdef HAVE_ABSL
		if (w.max == 0) { a = best_ns([&] { sink = absl::CEscape(s).size(); }); }
#endif
		std::printf("%-28s %12.1f %12.1f %8.2f %12.1f\n", w.name, o, n, o / n, a);
	}
	return 0;
}
