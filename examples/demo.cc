// A runnable tour of located-exception.
//
// Build (when this repo is the top-level project):
//   cmake -B build && cmake --build build && ./build/located_exception_demo
//
// The one idea worth taking away: a plain std::exception tells you WHAT went
// wrong (what()), but never WHERE. located-exception captures the throw site
// (function, file, line) at the point you THROW, formats the message with
// std::format, and stays catchable as both your own type and the matching std::
// exception. This demo throws one, catches it, prints where it came from,
// contrasts that against a bare std::exception, shows the std::-exception catch
// path, and shows a RETHROW chain keeping the ORIGINAL throw site.
#include <cstdio>
#include <stdexcept>

#include "exception.h"  // BaseException, Error, OutOfRange, THROW, RETHROW

static void rule(const char* title) {
	std::printf("\n\033[1m── %s ──\033[0m\n", title);
}

// A function that fails, so the captured site points somewhere meaningful (not
// inside main). THROW records THIS line as the origin.
static void open_widget(int id) {
	THROW(Error, "no widget with id {}", id);
}

// A leaf that throws a formatted OutOfRange, to show std::format interpolation
// and the dual std:: identity.
static int nth_item(int n, int count) {
	if (n >= count) {
		THROW(OutOfRange, "index {} out of range [0, {})", n, count);
	}
	return n;
}

// A mid-level handler that catches and RETHROWs with higher-level context. The
// caught variable MUST be named `exc` for RETHROW to find it.
static void handle_request(int id) {
	try {
		open_widget(id);
	} catch (const BaseException& exc) {
		// RETHROW inherits open_widget's ORIGINAL throw location, not this line.
		RETHROW(Error, "while handling request for id {}", id);
	}
}

int main() {
	std::puts("located-exception demo");

	// --- 1. throw, catch, and print WHERE it came from ------------------------
	rule("capture the throw site: function, file, line");
	try {
		open_widget(42);
	} catch (const Error& e) {
		// The raw captured site: these point at open_widget(), not at this catch.
		std::printf("  message  : %s\n", e.get_message());
		std::printf("  where    : %s:%d (%s)\n", e.filename, e.line, e.function);
		// get_context() pre-formats "file:line at func: message" for a log line.
		std::printf("  context  : %s\n", e.get_context());
	}

	// --- 2. what a plain std::exception leaves out ----------------------------
	rule("the gap it fills: std::exception carries no location");
	try {
		throw std::runtime_error("no widget with id 42");
	} catch (const std::exception& e) {
		// All you get is the message. No file, no line, no function.
		std::printf("  std::runtime_error::what() : %s\n", e.what());
		std::puts("  (where was it thrown? std::exception cannot say)");
	}
	try {
		open_widget(42);
	} catch (const BaseException& e) {
		std::printf("  located-exception          : %s\n", e.get_context());
		std::puts("  (same message, plus the origin baked in)");
	}

	// --- 3. one throw, two catchable identities -------------------------------
	rule("dual identity: catchable as the std:: type too");
	try {
		nth_item(7, 5);  // throws OutOfRange, which IS a std::out_of_range
	} catch (const std::out_of_range& e) {
		// Caught through the standard base. what() is the std::format'd message.
		std::printf("  caught as std::out_of_range : %s\n", e.what());
		std::puts("  (the same object is also an OutOfRange and a BaseException)");
	}

	// --- 4. RETHROW keeps the ORIGINAL site -----------------------------------
	rule("RETHROW: add context, keep the original origin");
	try {
		handle_request(99);
	} catch (const Error& e) {
		// The message is the OUTER (rethrow) context, but the location still
		// points back to open_widget(), where the failure first happened.
		std::printf("  message  : %s\n", e.get_message());
		std::printf("  where    : %s:%d (%s)  <- still open_widget, not handle_request\n",
			e.filename, e.line, e.function);
		std::printf("  context  : %s\n", e.get_context());
	}

	std::puts("\ndone.");
	return 0;
}
