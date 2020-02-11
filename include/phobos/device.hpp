#ifndef PHOBOS_DEVICE_HPP_
#define PHOBOS_DEVICE_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/physical_device.hpp>

namespace ph {

struct DeviceRequirements {
    std::vector<const char*> extensions;
    vk::PhysicalDeviceFeatures features;  
};

vk::Device create_device(PhysicalDeviceDetails const& physical_device, DeviceRequirements const& requirements = {});

}

#endif