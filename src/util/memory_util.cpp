#include <phobos/util/memory_util.hpp>

namespace ph::memory_util {

uint32_t find_memory_type(PhysicalDeviceDetails const& phys_device, uint32_t type_filter, vk::MemoryPropertyFlags properties) {
     // Get available memory types
    vk::PhysicalDeviceMemoryProperties const& device_properties = phys_device.memory_properties;
    // Find a matching one
    for (uint32_t i = 0; i < device_properties.memoryTypeCount; ++i) {
        // If the filter matches the memory type, return the index of the memory type
        if (type_filter & (1 << i) && 
            (device_properties.memoryTypes[i].propertyFlags & properties)) { 
            return i;
        }
    }

    assert(false && "Failed to find suitable memory type\n");
    return -1;
}

}