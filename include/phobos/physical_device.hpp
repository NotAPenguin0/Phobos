#ifndef PHOBOS_PHYSICAL_DEVICE_HPP_
#define PHOBOS_PHYSICAL_DEVICE_HPP_

#include <vulkan/vulkan.hpp>

#include <optional>

namespace ph {

struct QueueFamilyIndices {
    std::optional<size_t> graphics_family = std::nullopt;
    std::optional<size_t> present_family = std::nullopt;
};

struct PhysicalDeviceDetails {
    vk::PhysicalDevice handle;

    vk::PhysicalDeviceProperties properties;
    vk::PhysicalDeviceFeatures features;

    std::vector<vk::QueueFamilyProperties> queue_family_properties;
    QueueFamilyIndices queue_families;

    std::vector<vk::ExtensionProperties> supported_extensions;
};

struct PhysicalDeviceRequirements {
    bool graphics_required = true;
    bool present_required = true;  
    // This will always prefer a dedicated gpu over an integrated one
    bool prefer_dedicated_gpu = true;

    std::vector<const char*> required_extensions;
};

PhysicalDeviceDetails get_physical_device(vk::Instance instance, vk::SurfaceKHR surface, 
                                          PhysicalDeviceRequirements const& requirements);

}

#endif