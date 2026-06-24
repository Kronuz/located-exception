# located-exception

A C++20 exception base that remembers where it was thrown and formats its message with `std::format`, no third-party dependency.

## What it is

`located-exception` is a small exception hierarchy that captures the throw site (function, file, line) at the point you throw, and formats the message with `std::format`-style interpolation. On C++20 it uses `std::format` and pulls in nothing extra. The standard library hands you the message back through `what()`; this adds *where* it came from, plus typed, formatted messages, without you having to thread `__FILE__`/`__LINE__` through by hand. When you rethrow to add context, it keeps pointing at the original throw site instead of the rethrow.

## When to use it / when not

Use it when you want exceptions that carry their origin (handy for logging and error reports), when you want formatted messages without building strings at every throw site, and when you want one throw to be catchable as both your own type and the matching `std::` exception. It is a good fit for servers and tools where an error needs to say where it happened, not just what happened.

It is not a stack-trace library. It captures a single site (the throw, or the original throw across a rethrow chain), not a full backtrace. If you need the whole stack, reach for a proper backtrace facility.

## Install

This is not header-only. It ships a compiled translation unit, `exception.cc`, plus the header `exception.h`. You build and link the `.cc`; including the header alone won't give you the out-of-line constructor, `get_message()`, or `get_context()`. It requires C++20 for the default `std::format` backend.

With CMake `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  located_exception
  GIT_REPOSITORY https://github.com/Kronuz/located-exception.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(located_exception)

target_link_libraries(your_target PRIVATE located_exception)
```

`CMakeLists.txt` requests `cxx_std_20` `PUBLIC`, so consumers compile with C++20 too and `std::format` is selected on both sides with no extra dependency.

### Backend options

The formatting backend is picked automatically in `exception.h` (exception.h:36-50): `std::format` when `<format>` and `__cpp_lib_format` are available (the C++20 default), else a no-op passthrough that returns the format string uninterpolated. There is no third-party dependency.

Define `WITHOUT_FORMAT` `PUBLIC` to force the passthrough and drop interpolation entirely (the format string is returned literally, args ignored). The passthrough is also selected automatically on a pre-C++20 toolchain that lacks `<format>`/`__cpp_lib_format`.

The chosen backend changes the declared types in `exception.h` (`format_args` is `std::format_args` or `const void*`, see exception.h:36-50), so the library and every consumer must agree on it or you get ABI mismatches. Keeping the C++ standard `PUBLIC` (and the `WITHOUT_FORMAT` setting consistent) is what keeps the choice consistent; let CMake set it, do not flip it per-consumer.

## Usage

```cpp
#include "exception.h"

void f() {
    // Plain message.
    THROW(Error, "boom");

    // With interpolation (std::format on C++20).
    THROW(InvalidArgument, "expected {} args, got {}", 2, n);
}

void g() {
    try {
        f();
    } catch (const BaseException& exc) {
        // RETHROW chains: it inherits f()'s ORIGINAL throw location,
        // not this rethrow site. The in-scope variable MUST be named `exc`.
        RETHROW(Error, "while handling request");
    }
}
```

Catching, as the typed exception or as a plain `std::exception`:

```cpp
try {
    g();
} catch (const Error& e) {
    // Typed: read the captured site and the formatted message.
    std::printf("%s\n", e.get_context());   // "file:line at func: while handling request"
    std::printf("%s\n", e.get_message());   // "while handling request"
    std::printf("%s:%d (%s)\n", e.filename, e.line, e.function);
} catch (const std::exception& e) {
    // Error is also a std::runtime_error, so this catch works too.
    std::printf("%s\n", e.what());
}
```

`get_message()` returns the formatted message (falling back to the type name if the message is empty, see exception.cc:69-76). `get_context()` lazily builds `"file:line at func: message"` (exception.cc:79-92). `.function`, `.filename`, and `.line` are the raw captured site.

## API reference

### `BaseException`

The root of the hierarchy. Public members:

- `const char* function` — capturing function (`__func__` at the throw site).
- `const char* filename` — capturing file (`__FILE__`).
- `const int line` — capturing line (`__LINE__`).
- `const char* get_message() const` — the formatted message, or the type name if empty.
- `const char* get_context() const` — `"file:line at func: message"`, built lazily and cached.
- `bool empty() const` — true when the exception carries no type (a default-constructed sentinel).

### Derived types

- `Exception` — also a `std::runtime_error`.
- `Error` — derives from `Exception` (so also a `std::runtime_error`).
- `InvalidArgument` — also a `std::invalid_argument`.
- `OutOfRange` — also a `std::out_of_range`.
- `SystemExit` — also a `std::runtime_error`; carries an `int code` exit code. Constructed directly as `SystemExit(code)`, not via `THROW`.

Because each type multiply-inherits both `BaseException` and a `std::` exception, a single throw is catchable as your custom type *or* as the matching standard exception (or `std::exception`).

### Macros

- `THROW(type, ...)` — throws `type`, constructed with `__func__`, `__FILE__`, `__LINE__`, the stringized type name (`#type`), and the format string plus args. See exception.h:148.
- `RETHROW(type, ...)` — same, but passes the in-scope `exc` first so the new exception inherits the *original* throw location instead of the rethrow site. See exception.h:149. The variable in scope must be named `exc`.

## Build & test

```sh
# Configure and build, running the smoke test when this is the top-level project.
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

You can also build the smoke test directly (the commands are in the header comment of test/test.cc:2-3):

```sh
# Default: std::format on C++20, no dependency.
c++ -std=c++20 -I.. test/test.cc exception.cc -o test && ./test

# Zero-interpolation passthrough (pre-C++20, or to drop interpolation).
c++ -std=c++17 -DWITHOUT_FORMAT -I.. test/test.cc exception.cc -o test && ./test
```

The test asserts the captured filename/line/function, the message, the `" at "` context format, and that the type is also a real `std::runtime_error` (test/test.cc:10-35).

## Notes & caveats

- Not header-only. You must compile and link `exception.cc`.
- The formatting backend is part of the ABI. It changes the declared types in the header, so it must be identical across the library and all consumers. The choice is keyed on the C++ standard and header availability (`std::format` on C++20, else passthrough), and CMake keeps the standard `PUBLIC` for exactly this reason; don't compile the library and a consumer with different standards or `WITHOUT_FORMAT` settings.
- Under the passthrough backend (`WITHOUT_FORMAT`, or no `std::format` available) the format string is passed through verbatim and the args are dropped. No interpolation happens, so `"{}"` stays literal.
- `RETHROW` reads a hardcoded `exc` from the enclosing scope. Name your caught exception `exc` or it won't compile.
- The captured pointers (`function`, `filename`) come from `__func__`/`__FILE__` string literals, which have static storage, so they stay valid for the lifetime of the exception.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand) and turned into a standalone, generic library. Compared to the original: the formatting backend now defaults to `std::format` on C++20 with no third-party dependency, falling back to a passthrough; the previous {fmt} fallback was dropped entirely. The Xapiand-specific subclasses (`ClientError`, `MissingTypeError`, `QueryDslError`) were dropped to keep it generic, and the constructors pass lvalues into `make_format_args` (required by `std::format`), so they no longer `std::forward` into it. Verified in both modes: `std::format` (C++20, default) interpolates `"value 7 out of range [0, 5]"`, and the `WITHOUT_FORMAT` passthrough returns the literal format string.

## License

MIT. Copyright (c) 2015-2019 Dubalu LLC.
