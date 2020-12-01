#include <phobos/log_interface.hpp>

#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>

namespace ph {

void LogInterface::set_timestamp(bool timestamp) {
    timestamp_enabled = timestamp;
}

std::string LogInterface::get_timestamp_string() const {
    std::ostringstream oss;
    std::time_t time = std::time(nullptr);
    std::tm tm = *std::localtime(&time);
    oss << std::put_time(&tm, "[%H:%M:%S]");
    return oss.str();
}

}