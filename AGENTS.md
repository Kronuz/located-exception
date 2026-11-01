# AGENTS.md

Notes for agents and contributors working in this repo. For internal design see ARCHITECTURE.md; for usage see README.md.

## Repo map

- `exception.h` — the public API: `BaseException`, the derived types (`Exception`, `Error`, `InvalidArgument`, `OutOfRange`, `SystemExit`), the formatting-backend cascade (`std::format` / passthrough), and the `THROW` / `RETHROW` macros.
- `exception.cc` — out-of-line implementations: the constructors (including the private formatting constructor), `get_message()`, and `get_context()`. This file must be compiled and linked; the library is not header-only.
- `test/test.cc` — a single self-contained smoke test (asserts only, no framework).
- `docs/API.md` — the API reference, the formatting backends, and caveats.
- `LICENSE` — MIT, Dubalu LLC.
- `CMakeLists.txt` — builds the static library (alias `located_exception::located_exception`), requests C++20 `PUBLIC` (so `std::format` is the backend), and wires the test when this repo is top-level.

## Verification and benchmarks

CI (`.github/workflows/ci.yml`) calls the catalog's shared
[`cmake.yml`](https://github.com/Kronuz/cpp-libraries/blob/main/.github/workflows/cmake.yml) on every push and nightly:
CTest on Linux (x86-64 GCC and Clang, arm64), macOS (arm64, x86-64) and FreeBSD, then again under
ASan+UBSan and TSan. The shared method is the catalog's
[PERFORMANCE.md](https://github.com/Kronuz/cpp-libraries/blob/main/docs/PERFORMANCE.md).

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
# ASan+UBSan as CI's linux-asan-ubsan job runs it (Linux, Clang)
CC=clang CXX=clang++ cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS" \
  "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
cmake --build build-asan && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
# Both formatting backends by hand; CI builds only the std::format one
c++ -std=c++20 -I. test/test.cc exception.cc -o build/test_format && build/test_format
c++ -std=c++17 -DWITHOUT_FORMAT -I. test/test.cc exception.cc -o build/test_passthrough && build/test_passthrough
```

Run both backends before declaring a change good: they take different code paths (`std::format`
and the passthrough stub), and a change can pass one and break the other. A passing run prints
`located-exception OK: <context>` and exits 0. The test uses `assert`, which a Release build compiles
out; the default (empty) build type keeps it. The test target is added only when this repo
is the top-level project, so consumers that use `FetchContent` do not build it. There is no
bench, fuzz target or mutation script: the library makes no performance claim.

## Conventions

- C++20 (for the default `std::format` backend; the passthrough still compiles on C++17 with `WITHOUT_FORMAT`). Double quotes in code. No em dashes in docs.
- MIT-licensed; keep the copyright header (Copyright (c) 2015-2019 Dubalu LLC) on source files.
- Keep the library generic. Do not add domain-specific exception subclasses here; that is the kind of thing that was deliberately stripped when this was extracted from Xapiand.
- Public API surface is `exception.h`. Implementation belongs in `exception.cc`.

## Load-bearing invariants

- **The formatting backend must be consistent across the library and every consumer.** The backend is chosen automatically by a preprocessor cascade: `std::format` when `<format>` and `__cpp_lib_format` are available, else a passthrough (also forced by `WITHOUT_FORMAT`). The two backends declare a *different* `format_args` type (`std::format_args` vs `const void*`), which appears in the private constructor signature, so a library built with one backend and a consumer with another is an ODR/ABI mismatch. The choice is keyed on the C++ standard and header availability, so CMake propagates `cxx_std_20` `PUBLIC` precisely so the standard (and thus the backend) can't drift between the two sides. Don't compile the library and a consumer with different standards, and don't set or unset `WITHOUT_FORMAT` on a single target by hand.
- **`make_format_args` needs lvalues.** The constructors pass `args...` into `make_format_args(args...)` *without* `std::forward`. This is required by `std::format`'s `make_format_args`, which takes lvalue references. Do not "tidy" it into `std::forward<Args>(args)...`; that reintroduces the rvalue temporaries it rejects.
- **`RETHROW` reads a variable named `exc`**. The macro hardcodes `&exc`, so the enclosing `catch` must bind to that name.
- **Base init order matters.** Derived types build `BaseException` before the `std::` base and feed the latter from `BaseException::message`. `BaseException` must stay the first-listed base.

## How to extend: adding a new exception subclass

Most additions look like the existing wrapped-standard types. To add one that is also a `std::` exception:

```cpp
class LengthError : public BaseException, public std::length_error {
public:
    template<typename... Args>
    LengthError(Args&&... args)
        : BaseException(std::forward<Args>(args)...), std::length_error(message) { }
};
```

Keep `BaseException` first in the base list, forward the variadic args into it, and construct the `std::` base from `message`. If you don't need a standard-exception twin, derive from `Exception` instead (like `Error`). No macro changes are needed: `THROW(LengthError, ...)` and `RETHROW(LengthError, ...)` work as soon as the type exists, because they stringize the type token and call its constructor.

If the new type is reusable and generic, it belongs here. If it is application-specific, define it in the consuming project, not in this library.

## Traps

- Editing the constructor templates: it's easy to break the `THROW` / `RETHROW` dispatch. The three overloads (format / exc-aware / `void*` fallback) differ only by their first parameter and the SFINAE guard. Touching the parameter lists can silently re-route `RETHROW` through the format path and lose location inheritance.
- Under the passthrough backend (`WITHOUT_FORMAT`, or no `std::format` available), format placeholders are *not* interpolated; the format string is returned verbatim. A test that asserts on an interpolated message will pass with `std::format` and fail (or assert) under the passthrough. Write assertions that hold in both modes, or guard them.
- `function` and `filename` are stored as raw `const char*`. They're fine because `__func__`/`__FILE__` have static storage, but don't start pointing them at temporaries.

## Provenance note

This library was extracted from [Xapiand](https://github.com/Kronuz/Xapiand). When porting more from there, keep the generalizations that were made on extraction: the automatic backend cascade defaulting to `std::format` with a passthrough fallback and no third-party dependency (the original {fmt} fallback was dropped entirely), no Xapiand-domain subclasses, and the lvalue `make_format_args` fix. Don't regress those.
