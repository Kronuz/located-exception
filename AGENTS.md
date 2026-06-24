# AGENTS.md

Notes for agents and contributors working in this repo. For internal design see ARCHITECTURE.md; for usage see README.md.

## Repo map

- `exception.h` — the public API: `BaseException`, the derived types (`Exception`, `Error`, `InvalidArgument`, `OutOfRange`, `SystemExit`), the `WITHOUT_FMT` shim, and the `THROW` / `RETHROW` macros.
- `exception.cc` — out-of-line implementations: the constructors (including the private formatting constructor), `get_message()`, and `get_context()`. This file must be compiled and linked; the library is not header-only.
- `test/test.cc` — a single self-contained smoke test (asserts only, no framework).
- `CMakeLists.txt` — builds the static library, probes for {fmt}, and wires the test when this repo is top-level.

## Build & run the test

With CMake:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The test target is only added when this repo is the top-level project (CMakeLists.txt:20), so consumers vendoring it via `FetchContent` won't build it.

Direct compilation, both fmt modes. These are the exact commands from the header comment of `test/test.cc` (test/test.cc:2-3):

```sh
# Zero-dependency fallback (WITHOUT_FMT).
c++ -std=c++17 -DWITHOUT_FMT -I.. test/test.cc exception.cc -o test && ./test

# With fmt (header-only).
c++ -std=c++17 -I.. -I<fmt>/include -DFMT_HEADER_ONLY test/test.cc exception.cc -o test && ./test
```

Run both before declaring a change good. They exercise different code paths (real {fmt} vs. the stub) and a change can pass one and break the other.

A passing run prints `located-exception OK: <context>` and exits 0; failure prints `FAIL` and exits 1.

## Conventions

- C++17. Double quotes in code. No em dashes in docs.
- MIT-licensed; keep the copyright header (Copyright (c) 2015-2019 Dubalu LLC) on source files.
- Keep the library generic. Do not add domain-specific exception subclasses here; that is the kind of thing that was deliberately stripped when this was extracted from Xapiand.
- Public API surface is `exception.h`. Implementation belongs in `exception.cc`.

## Load-bearing invariants

- **`WITHOUT_FMT` must be consistent across the library and every consumer.** It changes the *declared types* in `exception.h` (`format_args` becomes `void*`, exception.h:31-46), so a library built one way and a consumer built the other is an ABI mismatch. CMake propagates the toggle `PUBLIC` precisely so this can't drift (CMakeLists.txt:16). Never set or unset `WITHOUT_FMT` on a single target by hand.
- **`make_format_args` needs lvalues under modern {fmt} (>= 10).** The constructors pass `args...` into `make_format_args(args...)` *without* `std::forward` (exception.h:79, 82, 85). This is intentional and is the fix that makes the library compile against current {fmt}. Do not "tidy" it into `std::forward<Args>(args)...`; that reintroduces the rvalue temporaries that fmt >= 10 rejects.
- **`RETHROW` reads a variable named `exc`** (exception.h:145). The macro hardcodes `&exc`, so the enclosing `catch` must bind to that name.
- **Base init order matters.** Derived types build `BaseException` before the `std::` base and feed the latter from `BaseException::message` (exception.h:107). `BaseException` must stay the first-listed base.

## How to extend: adding a new exception subclass

Most additions look like the existing wrapped-standard types (exception.h:127-138). To add one that is also a `std::` exception:

```cpp
class LengthError : public BaseException, public std::length_error {
public:
    template<typename... Args>
    LengthError(Args&&... args)
        : BaseException(std::forward<Args>(args)...), std::length_error(message) { }
};
```

Keep `BaseException` first in the base list, forward the variadic args into it, and construct the `std::` base from `message`. If you don't need a standard-exception twin, derive from `Exception` instead (like `Error`, exception.h:118-122). No macro changes are needed: `THROW(LengthError, ...)` and `RETHROW(LengthError, ...)` work as soon as the type exists, because they stringize the type token and call its constructor.

If the new type is reusable and generic, it belongs here. If it is application-specific, define it in the consuming project, not in this library.

## Traps

- Editing the constructor templates: it's easy to break the `THROW` / `RETHROW` dispatch. The three overloads (format / exc-aware / `void*` fallback) differ only by their first parameter and the SFINAE guard (exception.h:77-93). Touching the parameter lists can silently re-route `RETHROW` through the format path and lose location inheritance.
- Under `WITHOUT_FMT`, format placeholders are *not* interpolated; the format string is returned verbatim (exception.h:38-40). A test that asserts on an interpolated message will pass with fmt and fail (or assert) without it. Write assertions that hold in both modes, or guard them.
- `function` and `filename` are stored as raw `const char*`. They're fine because `__func__`/`__FILE__` have static storage, but don't start pointing them at temporaries.

## Provenance note

This library was extracted from [Xapiand](https://github.com/Kronuz/Xapiand). When porting more from there, keep the generalizations that were made on extraction: standard `<fmt/format.h>` include (not the vendored path), no Xapiand-domain subclasses, and the modern-{fmt} `make_format_args` fix. Don't regress those.
