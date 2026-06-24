# Architecture

This describes the internals of `located-exception`: how the throw site is captured, how the constructor overload set routes `THROW` and `RETHROW`, how the message is formatted, and why the types multiply-inherit from the standard library.

## Capturing the throw site

The capture happens entirely in the macros, expanded at the call site (exception.h:148-149):

```cpp
#define THROW(exception, ...)   throw exception(__func__, __FILE__, __LINE__, #exception, ##__VA_ARGS__)
#define RETHROW(exception, ...) throw exception(&exc, __func__, __FILE__, __LINE__, #exception, ##__VA_ARGS__)
```

`__func__`, `__FILE__`, and `__LINE__` are resolved by the preprocessor and compiler where the macro is used, so they name the function, file, and line of the throw, not anything inside the library. `#exception` stringizes the type token, giving the constructor a human-readable type name (`"Error"`, `"InvalidArgument"`, ...) that doubles as the message fallback. The `##__VA_ARGS__` lets you call `THROW(Error)` with no message and `THROW(Error, "fmt {}", x)` with one, both expanding cleanly.

`RETHROW` is identical except it prepends `&exc`. That extra leading argument is what selects the exception-aware constructor overloads below, and it is why the caught exception in scope must be named `exc`.

## The constructor overload set

`BaseException` exposes several public constructor templates. They all funnel into one private constructor that does the real work (exception.h:61, exception.cc:60-67):

```cpp
BaseException(private_ctor, const BaseException& exc,
              const char *function_, const char *filename_, int line_,
              const char* type, std::string_view format, format_args args);
```

The `private_ctor` tag keeps this overload from colliding with the public ones. The public templates differ only in their first parameter, which is how they get dispatched:

1. **Format constructors (no leading exc).** The variadic template at exception.h:81-83 takes `(function, filename, line, type, format, args...)`. This is what `THROW` hits. It forwards `default_exc()` (a static empty sentinel, exception.h:54-57) as the `exc` argument, so the private constructor uses the freshly captured `function_`/`filename_`/`line_`.

2. **Exception-aware constructors (leading `const T*`).** The template at exception.h:84-86 takes `(const T* exc, function, filename, line, type, format, args...)` and is SFINAE-gated with `std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>`. This is what `RETHROW(type, ...)` hits, because `&exc` is a pointer to something derived from `BaseException`. It dereferences `exc` and passes it through.

3. **`void*` fallback (leading `const void*`).** exception.h:87-89 catches the case where the leading pointer is *not* a `BaseException` subclass (the SFINAE on overload 2 fails). It ignores the pointer and behaves like the format constructor, falling back to `default_exc()`. This keeps `RETHROW` from being a hard compile error if `exc` happens to be some unrelated pointer type.

Each of these three has a non-template twin (exception.h:91-97) for the plain `std::string_view msg = ""` case with no format args, so a bare `THROW(Error, "boom")` doesn't need to instantiate the variadic path.

### How RETHROW inherits the original location

The private constructor decides whose location to keep (exception.cc:60-67):

```cpp
function(exc.type.empty() ? function_ : exc.function),
filename(exc.type.empty() ? filename_ : exc.filename),
line(exc.type.empty() ? line_ : exc.line)
```

If the incoming `exc` is the empty sentinel (`type` is empty, as it is for `THROW`), it takes the just-captured site. If `exc` is a real prior exception (as with `RETHROW`), it takes *that* exception's `function`/`filename`/`line`. So a rethrow points at where the problem first occurred, and you can wrap an error in higher-level context without losing the origin. The smoke test relies on this distinction.

## Formatting the message

The private constructor formats up front: `message(vformat(format, args))` (exception.cc:62). `format_args`, `make_format_args`, and `vformat` are aliased to one of two backends, chosen automatically at compile time by a preprocessor cascade in the header (exception.h:36-50):

```cpp
#if !defined(WITHOUT_FORMAT) && defined(__cpp_lib_format) && __has_include(<format>)
    #include <format>
    using std::format_args; using std::make_format_args; using std::vformat;
#else
    using format_args = const void*;
    template <typename... Args> const void* make_format_args(Args&&...) { return nullptr; }
    inline std::string vformat(std::string_view format_str, const void*) { return std::string(format_str); }
#endif
```

The first arm is the default on C++20: if `<format>` is present and `__cpp_lib_format` is defined, the standard library does the formatting and there is no third-party dependency. Failing that, or when `WITHOUT_FORMAT` is defined explicitly, the passthrough arm `vformat` ignores the args and returns the format string verbatim, with `format_args` reduced to `const void*`.

That two-way choice has to be consistent across translation units. Each backend declares a *different* `format_args` type, and `format_args` appears in the signature of the private constructor (exception.h:61). If the library is compiled with one backend and a consumer with another (for example, library on C++20 with `std::format`, consumer with `WITHOUT_FORMAT` or a pre-C++20 standard), the private constructor's mangled signature differs between the two and you get an ODR/ABI mismatch. This is why CMake propagates the C++ standard `PUBLIC`: keeping the standard the same on both sides keeps the same arm selected, and therefore the same declared types.

`get_message()` returns the stored message, substituting the type name if the message came out empty (exception.cc:69-76). `get_context()` builds the location-prefixed string lazily and caches it in the `mutable context` member (exception.cc:79-92):

```cpp
context.append(filename);
context.push_back(':');
context.append(std::to_string(line));
context.append(" at ");
context.append(function);
context.append(": ");
context.append(get_message());
```

Both `message` and `context` are `mutable` (exception.h:66-67) so the lazy fill works on a `const` exception, which is how you almost always hold one in a `catch`.

## The multiple-inheritance trick

Each concrete type inherits both `BaseException` and a standard exception (exception.h:108-142):

```cpp
class Exception        : public BaseException, public std::runtime_error { ... };
class InvalidArgument  : public BaseException, public std::invalid_argument { ... };
class OutOfRange       : public BaseException, public std::out_of_range { ... };
```

The forwarding constructor builds the `BaseException` base first, then constructs the `std::` base from the already-formatted `message`:

```cpp
template<typename... Args>
Exception(Args&&... args)
    : BaseException(std::forward<Args>(args)...), std::runtime_error(message) { }
```

`message` is the `BaseException` member, populated by the time the `std::runtime_error` subobject is initialized (base subobjects initialize in declaration order, `BaseException` first). The result is that one `throw Error(...)` is catchable as `Error`, as `Exception`, as `BaseException`, *and* as `std::runtime_error` / `std::exception`. `what()` returns the same text as `get_message()`. `Error` is a thin alias over `Exception`, and `SystemExit` is the one type that doesn't take a format message: it is constructed as `SystemExit(code)` and carries an `int code` (exception.h:115-119).

## Design decisions

- **Capture in the macro, not the constructor.** `__func__`/`__FILE__`/`__LINE__` only mean anything where they are written. Putting them in `THROW` keeps the call site honest and the library code location-agnostic.
- **One private constructor.** Every public overload normalizes to the same private signature, so the location-inheritance logic and the format call live in exactly one place.
- **Sentinel instead of a null check.** Using a static empty `default_exc()` lets the `THROW` path share the same private constructor as `RETHROW`; the `type.empty()` test does the branching.
- **`std::format` with a passthrough fallback.** Aliasing the format primitives behind a preprocessor cascade means the same code compiles against `std::format` or, when it isn't available, a no-op passthrough, picking the better option with no configuration. The default is zero-dependency on C++20 with no third-party dependency; the passthrough covers pre-C++20 toolchains and the explicit `WITHOUT_FORMAT` opt-out. The cost is that the choice is ABI-visible, because each backend declares a different `format_args` type.
- **Capture by raw `const char*`.** `__func__`/`__FILE__` are static-storage string literals, so storing the pointer is safe and cheap; no string copy at throw time for the location.

## Limitations

- Single site, not a stack trace. A `RETHROW` chain preserves the *original* site, but you don't get the intermediate frames.
- `RETHROW` is name-coupled to a variable called `exc`.
- The passthrough backend (no `std::format` available, or `WITHOUT_FORMAT` defined) silently drops interpolation; a format string with `{}` placeholders survives as literal text. There's no compile-time warning that args were ignored.
- The `void*` fallback overload makes a misused leading pointer compile (it just loses the chaining) rather than erroring loudly.
