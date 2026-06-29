// A runnable tour of repr.
//
// Build (when this repo is the top-level project):
//   cmake -B build && cmake --build build && ./build/repr_demo
//
// The one idea worth taking away: repr() turns an arbitrary byte buffer into a
// single-line, printable, copy-pasteable string. Printable ASCII survives as-is;
// control and high bytes become \xNN; \n \r \t and the active quote get the
// readable escapes. It is size-based, so it does not stop at an embedded NUL,
// and it can truncate a long buffer with "..." for log lines.
#include <cstdio>
#include <string>
#include <string_view>

#include "repr.hh"

static void rule(const char* title) {
	std::printf("\n\033[1m── %s ──\033[0m\n", title);
}

int main() {
	std::puts("repr demo");

	// --- 1. printable passes through, the rest is escaped --------------------
	rule("a byte buffer becomes one printable, quoted line");
	// The default wraps the result in single quotes. Printable ASCII is left
	// alone; the newline and tab get the readable \n / \t escapes.
	std::printf("  %s\n", repr(std::string_view("hello, world")).c_str());
	std::printf("  %s\n", repr(std::string_view("line1\nline2\tend")).c_str());

	// --- 2. control and high bytes -> \xNN -----------------------------------
	rule("control and non-ASCII bytes render as \\xNN");
	// 0x1b is ESC, 0x00 is NUL, 0xff is a high byte. repr is size-based, so the
	// NUL in the middle does not cut the string short.
	const unsigned char raw[] = {'a', 0x1b, 0x00, 0xff, 'z'};
	std::printf("  %s\n", repr(raw, sizeof(raw)).c_str());

	// --- 3. quoting modes ----------------------------------------------------
	rule("quote: default ', a double-quote, or none");
	// The active quote is escaped inside the text. Pass '\0' for no quotes at
	// all (handy when you are embedding the result in a larger message).
	std::printf("  default : %s\n", repr(std::string_view("it's \"mixed\"")).c_str());
	std::printf("  dquote  : %s\n", repr(std::string_view("it's \"mixed\""), true, '"').c_str());
	std::printf("  none    : %s\n", repr(std::string_view("it's \"mixed\""), true, '\0').c_str());

	// --- 4. non-friendly: every byte as hex ----------------------------------
	rule("friendly = false dumps every byte as \\xNN");
	// Useful when you want a pure hex view with nothing interpreted.
	std::printf("  %s\n", repr(std::string_view("AB\n"), false).c_str());

	// --- 5. truncation for long buffers --------------------------------------
	rule("max_size shortens long buffers with \"...\"");
	// A log line does not want the whole payload. max_size keeps a head and a
	// tail and drops the middle, so the output stays bounded.
	std::string big(64, 'x');
	big += "TAIL";
	std::printf("  full : %s\n", repr(std::string_view(big)).c_str());
	std::printf("  cut  : %s\n", repr(std::string_view(big), true, '\'', 24).c_str());

	std::puts("\ndone.");
	return 0;
}
