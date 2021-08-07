#pragma once

#include <string_view>
#include <functional>
#include <string>

#include <fmt/format.h>

namespace ph {
    
enum class LogSeverity {
    Debug = 0x00000001, // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
    Info = 0x00000010, // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
    Warning = 0x00000100, // VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
    Error = 0x00001000, // VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
    Fatal = 0x00010000 // No matching VK_DEBUG_UTILS_MESSAGE_SEVERITY bit
};

class LogInterface {
public:
    template<typename... Args>
    void write_fmt(LogSeverity severity, std::string_view fmt, Args&&... args) {
        write(severity, fmt::format(fmt, std::forward<Args>(args)...));
    }

    virtual void write(LogSeverity severity, std::string_view str) = 0;

    void set_timestamp(bool timestamp);
protected:
    bool timestamp_enabled = false;

    virtual std::string get_timestamp_string() const;
};


} // namespace ph
