/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <stdexcept>          // for std::runtime_error
#include <string>             // for std::string
#include <string_view>        // for std::string_view
#include <type_traits>        // for std::forward
#include <version>            // for __cpp_lib_format (feature-test macros)
#include <vector>             // for std::vector

// Message formatting uses std::format (C++20). If <format> is unavailable (or
// WITHOUT_FORMAT is defined), it falls back to a no-op passthrough that returns
// the format string uninterpolated. The chosen backend changes the declared
// types below, so it must be consistent across this library and every consumer
// (CMake propagates the C++ standard PUBLIC to keep std::format on both sides).
// <version> above is what makes __cpp_lib_format visible before this check: the
// feature-test macro lives there, so without it libstdc++ (which, unlike libc++,
// doesn't leak it in through other headers) would silently pick the passthrough.
#if !defined(WITHOUT_FORMAT) && defined(__cpp_lib_format) && __has_include(<format>)
	#include <format>
	// std::format backend: interpolate the arguments into the format string.
	template <typename... Args>
	inline std::string interpolate(std::string_view format, Args&&... args) {
		// make_format_args requires lvalues (P2418); args are named, so no std::forward.
		// Fully qualified so an unqualified call can't collide via ADL with a
		// std::make_format_args pulled in by an argument type (e.g. std::string_view).
		return std::vformat(format, std::make_format_args(args...));
	}
#else
	// Passthrough backend (pre-C++20, WITHOUT_FORMAT, or a libc++ that ships <format>
	// without advertising __cpp_lib_format): return the format string verbatim.
	template <typename... Args>
	inline std::string interpolate(std::string_view format, Args&&...) {
		return std::string(format);
	}
#endif


class BaseException {
	static const BaseException& default_exc() {
		static const BaseException default_exc;
		return default_exc;
	}

	struct private_ctor {};

	BaseException(private_ctor, const BaseException& exc, const char *function_, const char *filename_, int line_, const char* type, std::string message_);

protected:
	std::string type;

	mutable std::string message;
	mutable std::string context;

	BaseException();

public:
	const char* function;
	const char* filename;
	const int line;

	BaseException(const BaseException& exc);
	BaseException(BaseException&& exc);

	BaseException(const BaseException* exc);

	template <typename... Args>
	BaseException(const char *function, const char *filename, int line, const char* type, std::string_view format, Args&&... args)
		: BaseException(private_ctor{}, default_exc(), function, filename, line, type, interpolate(format, args...)) { }
	template <typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
	BaseException(const T* exc, const char *function, const char *filename, int line, const char* type, std::string_view format, Args&&... args)
		: BaseException(private_ctor{}, *exc, function, filename, line, type, interpolate(format, args...)) { }
	template <typename... Args>
	BaseException(const void*, const char *function, const char *filename, int line, const char* type, std::string_view format, Args&&... args)
		: BaseException(private_ctor{}, default_exc(), function, filename, line, type, interpolate(format, args...)) { }

	BaseException(const char *function, const char *filename, int line, const char* type, std::string_view msg = "")
		: BaseException(private_ctor{}, default_exc(), function, filename, line, type, interpolate(msg)) { }
	template <typename T, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
	BaseException(const T* exc, const char *function, const char *filename, int line, const char* type, std::string_view msg = "")
		: BaseException(private_ctor{}, *exc, function, filename, line, type, interpolate(msg)) { }
	BaseException(const void*, const char *function, const char *filename, int line, const char* type, std::string_view msg = "")
		: BaseException(private_ctor{}, default_exc(), function, filename, line, type, interpolate(msg)) { }

	const char* get_message() const;
	const char* get_context() const;

	bool empty() const {
		return type.empty();
	}
};


class Exception : public BaseException, public std::runtime_error {
public:
	template<typename... Args>
	Exception(Args&&... args) : BaseException(std::forward<Args>(args)...), std::runtime_error(message) { }
};


class SystemExit : public BaseException, public std::runtime_error {
public:
	int code;
	explicit SystemExit(int code_) : BaseException(__func__, __FILE__, __LINE__, "SystemExit"), std::runtime_error(message), code(code_) { }
};


class Error : public Exception {
public:
	template<typename... Args>
	Error(Args&&... args) : Exception(std::forward<Args>(args)...) { }
};


// Wrapped standard exceptions:

class InvalidArgument : public BaseException, public std::invalid_argument {
public:
	template<typename... Args>
	InvalidArgument(Args&&... args) : BaseException(std::forward<Args>(args)...), std::invalid_argument(message) { }
};


class OutOfRange : public BaseException, public std::out_of_range {
public:
	template<typename... Args>
	OutOfRange(Args&&... args) : BaseException(std::forward<Args>(args)...), std::out_of_range(message) { }
};


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define THROW(exception, ...) throw exception(__func__, __FILE__, __LINE__, #exception, ##__VA_ARGS__)
#define RETHROW(exception, ...) throw exception(&exc, __func__, __FILE__, __LINE__, #exception, ##__VA_ARGS__)

#pragma clang diagnostic pop
