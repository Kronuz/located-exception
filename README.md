# located-exception

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/located-exception/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/located-exception/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

Exceptions for C++20 that record their throw site: function, file and line, with std::format messages.
`THROW` passes `__func__`, `__FILE__` and `__LINE__` to an exception that derives from both
`BaseException` and the matching standard exception, so one throw is catchable either way, and
`RETHROW` adds a message while keeping the original site. Reach for it in servers and tools
whose errors need to say where they happened.

## ✨ Highlights

- **The throw site, free.** `THROW(Error, "...")` records the function, file and line.
- **Formatted messages.** `THROW(InvalidArgument, "expected {} args, got {}", 2, n)`.
- **Two identities.** `Error` is also a `std::runtime_error`, `OutOfRange` a
  `std::out_of_range`, `InvalidArgument` a `std::invalid_argument`.
- **Context that survives rethrows.** `RETHROW` adds a message and keeps the original site.
- **No dependencies.** `std::format`, or a passthrough that keeps the raw format string
  when `<format>` is missing or `WITHOUT_FORMAT` is defined.

## 🚀 Quick start

```cpp
#include "exception.h"

#include <cstdio>

void load(int n) {
	if (n > 5) { THROW(OutOfRange, "value {} out of range [0, 5]", n); }
}

int main() {
	try {
		try {
			load(7);
		} catch (const BaseException& exc) {  // RETHROW reads a variable named exc
			RETHROW(Error, "while loading config");
		}
	} catch (const Error& e) {
		std::puts(e.get_context());  // main.cc:6 at load: while loading config
	}
}
```

[`examples/demo.cc`](examples/demo.cc) compares this with a bare `std::runtime_error` and
shows both catch identities.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; it compiles one source file. CI builds it
on Linux (x86_64, arm64), macOS (arm64, x86_64), and FreeBSD, plus ASan, UBSan, and TSan
builds.

```cmake
include(FetchContent)
FetchContent_Declare(located_exception
  GIT_REPOSITORY https://github.com/Kronuz/located-exception.git
  GIT_TAG        4dcb2b4a0a4a3d05249d9ce3d2deee1ea81ee6df)  # or main, to track the tip
FetchContent_MakeAvailable(located_exception)

target_link_libraries(my_app PRIVATE located_exception::located_exception)
```

```cpp
#include "exception.h"
```

## 🎯 When to use it

Use it for errors that should report where they came from in logs and responses, while
callers that know only the standard exceptions can still catch them.

Not for:

- **Full stack traces.** It records one site; [traceback](https://github.com/Kronuz/traceback)
  captures the whole stack.
- **Mixed formatting backends.** The backend changes the declared types in `exception.h`,
  so `WITHOUT_FORMAT` and the C++ standard must match in the library and every consumer;
  the CMake target keeps C++20 public for that.
- **`RETHROW` outside a catch of `exc`.** The macro reads a variable named `exc`.

## 📚 Documentation

- [docs/API.md](docs/API.md): the types, the macros, the formatting backends and caveats.
- [ARCHITECTURE.md](ARCHITECTURE.md): how the constructors capture and chain the site.
- [CHANGELOG.md](CHANGELOG.md): dated changes.
- [AGENTS.md](AGENTS.md): repository map, invariants and traps.
- [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries): the catalog.

## 🛠️ Building and testing

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

`./build/located_exception_demo` runs the tour in `examples/demo.cc`.

## 🤝 Contributing

Read [AGENTS.md](AGENTS.md) and the [catalog's rules](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md)
first.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo and (c) 2015-2020 Dubalu LLC. See
[LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).
