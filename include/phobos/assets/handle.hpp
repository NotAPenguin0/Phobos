#ifndef PHOBOS_ASSET_HANDLE_HPP_
#define PHOBOS_ASSET_HANDLE_HPP_

#include <cstddef>

namespace ph {

template<typename T>
struct Handle {
    uint64_t id;
};

}

#endif