#ifndef PHOBOS_MAPPED_UBO_HPP_
#define PHOBOS_MAPPED_UBO_HPP_

#include <vulkan/vulkan.hpp>

namespace ph {

struct MappedUBO {
    vk::Buffer buffer =  nullptr;
    vk::DeviceMemory memory = nullptr;
    void* ptr = nullptr;
    size_t size = 0;
};

}

#endif