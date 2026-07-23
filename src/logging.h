#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fmt/base.h>
#include <fmt/format.h>
#include <mutex>
#include <string>
#include <utility>

namespace chunguslog {

enum class Level { Debug, Info, Warn, Error };

inline const char* level_name(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "INFO";
}

inline Level configured_level() {
    static const Level level = [] {
        const char* raw = std::getenv("LOG_LEVEL");
        if (raw == nullptr) return Level::Info;

        std::string value(raw);
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (value == "DEBUG" || value == "TRACE") return Level::Debug;
        if (value == "WARN" || value == "WARNING") return Level::Warn;
        if (value == "ERROR") return Level::Error;
        return Level::Info;
    }();
    return level;
}

inline std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    char seconds[21];
    std::strftime(seconds, sizeof(seconds), "%Y-%m-%dT%H:%M:%S", &utc);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    return fmt::format("{}.{:03}Z", seconds, millis.count());
}

inline std::mutex& output_mutex() {
    static std::mutex mutex;
    return mutex;
}

template <typename... T>
inline void write(Level level, fmt::format_string<T...> message, T&&... args) {
    if (static_cast<int>(level) < static_cast<int>(configured_level())) return;

    const auto rendered = fmt::format(message, std::forward<T>(args)...);
    std::lock_guard<std::mutex> lock(output_mutex());
    fmt::println(stderr, "[{}][chungusway][{}] {}", timestamp(), level_name(level), rendered);
}

template <typename... T>
inline void debug(fmt::format_string<T...> message, T&&... args) {
    write(Level::Debug, message, std::forward<T>(args)...);
}

template <typename... T>
inline void info(fmt::format_string<T...> message, T&&... args) {
    write(Level::Info, message, std::forward<T>(args)...);
}

template <typename... T>
inline void warn(fmt::format_string<T...> message, T&&... args) {
    write(Level::Warn, message, std::forward<T>(args)...);
}

template <typename... T>
inline void error(fmt::format_string<T...> message, T&&... args) {
    write(Level::Error, message, std::forward<T>(args)...);
}

} // namespace chunguslog
