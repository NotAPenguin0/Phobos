#ifndef PHOBOS_MEMORY_UTIL_HPP_
#define PHOBOS_MEMORY_UTIL_HPP_

#include <phobos/core/vulkan_context.hpp>

namespace ph {

namespace memory_util {

uint32_t find_memory_type(PhysicalDeviceDetails const& phys_device, uint32_t type_filter, vk::MemoryPropertyFlags properties);

template<typename T>
auto to_vk_type(T obj) { 
    return static_cast<typename T::CType>(obj);
}

template<typename T>
uint64_t vk_to_u64(T obj) {
    return reinterpret_cast<uint64_t>(to_vk_type(obj));
}

}

}

#endif