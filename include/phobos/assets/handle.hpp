#ifndef PHOBOS_ASSET_HANDLE_HPP_
#define PHOBOS_ASSET_HANDLE_HPP_

#include <cstdint>

namespace ph {

template<typename T>
struct Handle {
    std::uint64_t id;
};

}

#endif