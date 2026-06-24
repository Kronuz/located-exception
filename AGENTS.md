# AGENTS.md

Notes for agents and contributors working in this repo. For internal design see ARCHITECTURE.md; for usage see README.md.

## Repo map

- `exception.h` — the public API: `BaseException`, the derived types (`Exception`, `Error`, `InvalidArgument`, `OutOfRange`, `SystemExit`), the formatting-backend cascade (`std::format` / {fmt} / passthrough), and the `THROW` / `RETHROW` macros.
- `exception.cc` — out-of-line implementations: the constructors (including the private formatting constructor), `get_message()`, and `get_context()`. This file must be compiled and linked; the library is not header-only.
- `test/test.cc` — a single self-contained smoke test (asserts only, no framework).
- `CMakeLists.txt` — builds the static library, requests C++20 `PUBLIC` (so `std::format` is the backend), and wires the test when this repo is top-level.

## Build & run the test

With CMake:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The test target is only added when this repo is the top-level project (CMakeLists.txt:20), so consumers vendoring it via `FetchContent` won't build it.

Direct compilation, all three backends. These build on the commands from the header comment of `test/test.cc` (test/test.cc:2-3):

```sh
# Default: std::format on C++20, no dependency.
c++ -std=c++20 -I.. test/test.cc exception.cc -o test && ./test

# {fmt} fallback (header-only), C++17 toolchains.
c++ -std=c++17 -I.. -I<fmt>/include -DFMT_HEADER_ONLY test/test.cc exception.cc -o test && ./test

# Zero-interpolation passthrough.
c++ -std=c++20 -DWITHOUT_FMT -I.. test/test.cc exception.cc -o test && ./test
```

Run all three before declaring a change good. They exercise different code paths (`std::format`, real {fmt}, and the passthrough stub) and a change can pass one and break another.

A passing run prints `located-exception OK: <context>` and exits 0; failure prints `FAIL` and exits 1.

## Conventions

- C++20 (for the default `std::format` backend; the {fmt} fallback still compiles on C++17). Double quotes in code. No em dashes in docs.
- MIT-licensed; keep the copyright header (Copyright (c) 2015-2019 Dubalu LLC) on source files.
- Keep the library generic. Do not add domain-specific exception subclasses here; that is the kind of thing that was deliberately stripped when this was extracted from Xapiand.
- Public API surface is `exception.h`. Implementation belongs in `exception.cc`.

## Load-bearing invariants

- **The formatting backend must be consistent across the library and every consumer.** The backend is chosen automatically by a preprocessor cascade (exception.h:37-56): `std::format` when `<format>` and `__cpp_lib_format` are available, else {fmt} if `<fmt/format.h>` is present, else a passthrough (also forced by `WITHOUT_FMT`). Each backend declares a *different* `format_args` type, which appears in the private constructor signature, so a library built with one backend and a consumer with another is an ODR/ABI mismatch. The choice is keyed on the C++ standard and header availability, so CMake propagates `cxx_std_20` `PUBLIC` precisely so the standard (and thus the backend) can't drift between the two sides (CMakeLists.txt:11). Don't compile the library and a consumer with different standards, and don't set or unset `WITHOUT_FMT` on a single target by hand.
- **`make_format_args` needs lvalues.** The constructors pass `args...` into `make_format_args(args...)` *without* `std::forward` (exception.h:89, 92, 95). This is required by both `std::format`'s and modern {fmt}'s (>= 10) `make_format_args`, which take lvalue references. Do not "tidy" it into `std::forward<Args>(args)...`; that reintroduces the rvalue temporaries both backends reject.
- **`RETHROW` reads a variable named `exc`** (exception.h:155). The macro hardcodes `&exc`, so the enclosing `catch` must bind to that name.
- **Base init order matters.** Derived types build `BaseException` before the `std::` base and feed the latter from `BaseException::message` (exception.h:117). `BaseException` must stay the first-listed base.

## How to extend: adding a new exception subclass

Most additions look like the existing wrapped-standard types (exception.h:137-148). To add one that is also a `std::` exception:

```cpp
class LengthError : public BaseException, public std::length_error {
public:
    template<typename... Args>
    LengthError(Args&&... args)
        : BaseException(std::forward<Args>(args)...), std::length_error(message) { }
};
```

Keep `BaseException` first in the base list, forward the variadic args into it, and construct the `std::` base from `message`. If you don't need a standard-exception twin, derive from `Exception` instead (like `Error`, exception.h:128-132). No macro changes are needed: `THROW(LengthError, ...)` and `RETHROW(LengthError, ...)` work as soon as the type exists, because they stringize the type token and call its constructor.

If the new type is reusable and generic, it belongs here. If it is application-specific, define it in the consuming project, not in this library.

## Traps

- Editing the constructor templates: it's easy to break the `THROW` / `RETHROW` dispatch. The three overloads (format / exc-aware / `void*` fallback) differ only by their first parameter and the SFINAE guard (exception.h:87-95). Touching the parameter lists can silently re-route `RETHROW` through the format path and lose location inheritance.
- Under the passthrough backend (`WITHOUT_FMT`, or no `std::format`/{fmt} available), format placeholders are *not* interpolated; the format string is returned verbatim (exception.h:47-56). A test that asserts on an interpolated message will pass with `std::format`/{fmt} and fail (or assert) under the passthrough. Write assertions that hold in both modes, or guard them.
- `function` and `filename` are stored as raw `const char*`. They're fine because `__func__`/`__FILE__` have static storage, but don't start pointing them at temporaries.

## Provenance note

This library was extracted from [Xapiand](https://github.com/Kronuz/Xapiand). When porting more from there, keep the generalizations that were made on extraction: the automatic backend cascade defaulting to `std::format` (with the standard `<fmt/format.h>` include, not the vendored path, as the {fmt} fallback), no Xapiand-domain subclasses, and the lvalue `make_format_args` fix. Don't regress those.
