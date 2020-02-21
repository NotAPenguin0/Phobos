#ifndef PHOBOS_LOG_INTERFACE_HPP_
#define PHOBOS_LOG_INTERFACE_HPP_

#include <string_view>
#include <functional>

#include <fmt/format.h>

namespace ph {

namespace log {

enum class Severity {
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

class LogInterface {
public:
    template<typename... Args>
    void write_fmt(Severity severity, std::string_view fmt, Args&&... args) {
        write(severity, fmt::format(fmt, std::forward<Args>(args)...));
    }

    virtual void write(Severity severity, std::string_view str) = 0;

    void set_timestamp(bool timestamp);
protected:
    bool timestamp_enabled = false;

    virtual std::string get_timestamp_string() const;
};

} // namespace log

} // namespace ph

#endif