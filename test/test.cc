// Smoke test for the standalone located-exception library.
// Build (no fmt):  c++ -std=c++17 -DWITHOUT_FMT -I.. test.cc ../exception.cc -o test && ./test
// Build (w/ fmt):  c++ -std=c++17 -I.. -I<fmt>/include -DFMT_HEADER_ONLY test.cc ../exception.cc -o test && ./test
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "exception.h"

int main() {
	int expected_line = 0;
	try {
		expected_line = __LINE__ + 1;
		THROW(Error, "boom");
	} catch (const Error& e) {
		// Location captured at the throw site.
		assert(std::strstr(e.filename, "test.cc") != nullptr);
		assert(e.line == expected_line);
		assert(std::strstr(e.function, "main") != nullptr);

		// Message and the "file:line at func: message" context string.
		assert(std::strcmp(e.get_message(), "boom") == 0);
		assert(std::strstr(e.get_context(), "boom") != nullptr);
		assert(std::strstr(e.get_context(), " at ") != nullptr);

		// It is also a real std::exception (Error -> Exception -> std::runtime_error).
		const std::runtime_error& re = e;
		assert(std::strcmp(re.what(), "boom") == 0);

#ifndef WITHOUT_FMT
		// Format-string interpolation (std::format by default, or {fmt}).
		try {
			THROW(OutOfRange, "value {} out of range [{}, {}]", 7, 0, 5);
		} catch (const OutOfRange& oe) {
			assert(std::strcmp(oe.get_message(), "value 7 out of range [0, 5]") == 0);
			const std::out_of_range& se = oe;
			assert(std::strcmp(se.what(), "value 7 out of range [0, 5]") == 0);
			std::printf("located-exception OK: %s | %s\n", e.get_context(), oe.get_message());
			return 0;
		}
		std::printf("FAIL: interpolating throw was not caught\n");
		return 1;
#else
		std::printf("located-exception OK (WITHOUT_FMT): %s\n", e.get_context());
		return 0;
#endif
	}
	std::printf("FAIL: exception was not thrown/caught\n");
	return 1;
}
