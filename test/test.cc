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

		std::printf("located-exception OK: %s\n", e.get_context());
		return 0;
	}
	std::printf("FAIL: exception was not thrown/caught\n");
	return 1;
}
