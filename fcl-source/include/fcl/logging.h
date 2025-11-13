#pragma once

#include <ostream>
#include <charconv>
#include <system_error>
#include <string>
#include <type_traits>

#ifndef FCL_ENABLE_STD_LOGGING
#define FCL_ENABLE_STD_LOGGING 1
#endif

namespace fcl {
namespace logging {

#if FCL_ENABLE_STD_LOGGING

inline std::ostream& GetCerr() noexcept {
    return std::cerr;
}

#else

class NullStream {
public:
    template <typename T>
    NullStream& operator<<(const T&) noexcept {
        return *this;
    }

    NullStream& operator<<(std::ostream& (*)(std::ostream&)) noexcept {
        return *this;
    }

    NullStream& operator<<(std::ios_base& (*)(std::ios_base&)) noexcept {
        return *this;
    }
};

inline NullStream& GetCerr() noexcept {
    static NullStream stream;
    return stream;
}

#endif

class StringBuilder {
public:
    StringBuilder& operator<<(const char* text) {
        if (text != nullptr) {
            buffer_.append(text);
        }
        return *this;
    }

    StringBuilder& operator<<(char value) {
        buffer_.push_back(value);
        return *this;
    }

    StringBuilder& operator<<(const std::string& text) {
        buffer_.append(text);
        return *this;
    }

    StringBuilder& operator<<(bool value) {
        buffer_.append(value ? "true" : "false");
        return *this;
    }

    template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    StringBuilder& operator<<(T value) {
        char buffer[32];
        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (ec == std::errc()) {
            buffer_.append(buffer, ptr);
        }
        return *this;
    }

    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    StringBuilder& operator<<(T value) {
        char buffer[64];
        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general);
        if (ec == std::errc()) {
            buffer_.append(buffer, ptr);
        }
        return *this;
    }

    std::string str() const {
        return buffer_;
    }

private:
    std::string buffer_;
};

}  // namespace logging
}  // namespace fcl

#define FCL_CERR (::fcl::logging::GetCerr())
