#pragma once

#include <cstddef>

namespace ph {

struct Version {
    size_t major, minor, patch;
};

constexpr inline Version VERSION = Version{ 0, 1, 0 };

}
