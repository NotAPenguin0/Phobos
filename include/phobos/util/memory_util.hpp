#ifndef PHOBOS_MEMORY_UTIL_HPP_
#define PHOBOS_MEMORY_UTIL_HPP_

#include <phobos/core/vulkan_context.hpp>

namespace ph {

namespace memory_util {

uint32_t find_memory_type(PhysicalDeviceDetails const& phys_device, uint32_t type_filter, vk::MemoryPropertyFlags properties);

}

}

#endif