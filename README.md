# located-exception

A C++17 exception base that remembers where it was thrown and formats its message with {fmt}.

## What it is

`located-exception` is a small exception hierarchy that captures the throw site (function, file, line) at the point you throw, and formats the message with {fmt}-style interpolation. The standard library hands you the message back through `what()`; this adds *where* it came from, plus typed, formatted messages, without you having to thread `__FILE__`/`__LINE__` through by hand. When you rethrow to add context, it keeps pointing at the original throw site instead of the rethrow.

## When to use it / when not

Use it when you want exceptions that carry their origin (handy for logging and error reports), when you want formatted messages without building strings at every throw site, and when you want one throw to be catchable as both your own type and the matching `std::` exception. It is a good fit for servers and tools where an error needs to say where it happened, not just what happened.

It is not a stack-trace library. It captures a single site (the throw, or the original throw across a rethrow chain), not a full backtrace. If you need the whole stack, reach for a proper backtrace facility. It also pulls in {fmt} unless you opt out, so if you have a hard no-dependency constraint and don't want even the fallback path, this may be more than you need.

## Install

This is not header-only. It ships a compiled translation unit, `exception.cc`, plus the header `exception.h`. You build and link the `.cc`; including the header alone won't give you the out-of-line constructor, `get_message()`, or `get_context()`.

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

### fmt is optional

The build probes for {fmt} with `find_package(fmt QUIET)`. If it is found, the target links `fmt::fmt` and format strings are interpolated. If it is not found, the build defines `WITHOUT_FMT` instead, which drops the dependency and passes the format string through literally (no interpolation, args ignored). See CMakeLists.txt:12-17.

That toggle is propagated `PUBLIC`. It has to be: `WITHOUT_FMT` changes the declared types in `exception.h` (`format_args` becomes `void*`, see exception.h:31-46), so the library and every consumer must agree on it or you get ABI mismatches. Let CMake set it; do not flip it per-consumer.

## Usage

```cpp
#include "exception.h"

void f() {
    // Plain message.
    THROW(Error, "boom");

    // With {fmt} interpolation (when fmt is available).
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

- `THROW(type, ...)` — throws `type`, constructed with `__func__`, `__FILE__`, `__LINE__`, the stringized type name (`#type`), and the format string plus args. See exception.h:144.
- `RETHROW(type, ...)` — same, but passes the in-scope `exc` first so the new exception inherits the *original* throw location instead of the rethrow site. See exception.h:145. The variable in scope must be named `exc`.

## Build & test

```sh
# Configure and build, running the smoke test when this is the top-level project.
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

You can also build the smoke test directly (the commands are in the header comment of test/test.cc:2-3):

```sh
# Zero-dependency fallback, no fmt.
c++ -std=c++17 -DWITHOUT_FMT -I.. test/test.cc exception.cc -o test && ./test

# With fmt (header-only).
c++ -std=c++17 -I.. -I<fmt>/include -DFMT_HEADER_ONLY test/test.cc exception.cc -o test && ./test
```

The test asserts the captured filename/line/function, the message, the `" at "` context format, and that the type is also a real `std::runtime_error` (test/test.cc:10-35).

## Notes & caveats

- Not header-only. You must compile and link `exception.cc`.
- `WITHOUT_FMT` is part of the ABI. It changes the declared types in the header, so it must be identical across the library and all consumers. CMake propagates it `PUBLIC` for exactly this reason; don't override it per-target.
- Under `WITHOUT_FMT` the format string is passed through verbatim and the args are dropped. No interpolation happens, so `"{}"` stays literal.
- `RETHROW` reads a hardcoded `exc` from the enclosing scope. Name your caught exception `exc` or it won't compile.
- The captured pointers (`function`, `filename`) come from `__func__`/`__FILE__` string literals, which have static storage, so they stay valid for the lifetime of the exception.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand) and turned into a standalone, generic library. Compared to the original: the {fmt} include was retargeted from a vendored `"fmt/format.h"` to the standard `<fmt/format.h>`; the Xapiand-specific subclasses (`ClientError`, `MissingTypeError`, `QueryDslError`) were dropped to keep it generic; and the constructors were fixed to compile against modern {fmt} (fmt >= 10 needs lvalues in `make_format_args`, so they no longer `std::forward` into it). Verified against current header-only {fmt} and the `WITHOUT_FMT` fallback.

## License

MIT. Copyright (c) 2015-2019 Dubalu LLC.
