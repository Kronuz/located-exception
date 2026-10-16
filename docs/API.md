# located-exception API reference

## `BaseException`

The root of the hierarchy. Public members:

- `const char* function` — capturing function (`__func__` at the throw site).
- `const char* filename` — capturing file (`__FILE__`).
- `const int line` — capturing line (`__LINE__`).
- `const char* get_message() const` — the formatted message, or the type name if empty.
- `const char* get_context() const` — `"file:line at func: message"`, built lazily and cached.
- `bool empty() const` — true when the exception carries no type (a default-constructed sentinel).

## Derived types

- `Exception` — also a `std::runtime_error`.
- `Error` — derives from `Exception` (so also a `std::runtime_error`).
- `InvalidArgument` — also a `std::invalid_argument`.
- `OutOfRange` — also a `std::out_of_range`.
- `SystemExit` — also a `std::runtime_error`; carries an `int code` exit code. Constructed directly as `SystemExit(code)`, not via `THROW`.

Because each type multiply-inherits both `BaseException` and a `std::` exception, a single throw is catchable as your custom type *or* as the matching standard exception (or `std::exception`).

## Macros

- `THROW(type, ...)` — throws `type`, constructed with `__func__`, `__FILE__`, `__LINE__`, the stringized type name (`#type`), and the format string plus args.
- `RETHROW(type, ...)` — same, but passes the in-scope `exc` first so the new exception inherits the *original* throw location instead of the rethrow site. The variable in scope must be named `exc`.

## Notes & caveats

- Not header-only. You must compile and link `exception.cc`.
- The formatting backend is part of the ABI. It changes the declared types in the header, so it must be identical across the library and all consumers. The choice is keyed on the C++ standard and header availability (`std::format` on C++20, else passthrough), and CMake keeps the standard `PUBLIC` for exactly this reason; don't compile the library and a consumer with different standards or `WITHOUT_FORMAT` settings.
- Under the passthrough backend (`WITHOUT_FORMAT`, or no `std::format` available) the format string is passed through verbatim and the args are dropped. No interpolation happens, so `"{}"` stays literal.
- `RETHROW` reads a hardcoded `exc` from the enclosing scope. Name your caught exception `exc` or it won't compile.
- The captured pointers (`function`, `filename`) come from `__func__`/`__FILE__` string literals, which have static storage, so they stay valid for the lifetime of the exception.

## Formatting backends

The formatting backend is picked automatically in `exception.h`: `std::format` when `<format>` and `__cpp_lib_format` are available (the C++20 default), else a no-op passthrough that returns the format string uninterpolated. There is no third-party dependency.

Define `WITHOUT_FORMAT` `PUBLIC` to force the passthrough and drop interpolation entirely (the format string is returned literally, args ignored). The passthrough is also selected automatically on a pre-C++20 toolchain that lacks `<format>`/`__cpp_lib_format`.

The chosen backend changes the declared types in `exception.h` (`format_args` is `std::format_args` or `const void*`), so the library and every consumer must agree on it or you get ABI mismatches. Keeping the C++ standard `PUBLIC` (and the `WITHOUT_FORMAT` setting consistent) is what keeps the choice consistent; let CMake set it, do not flip it per-consumer.
