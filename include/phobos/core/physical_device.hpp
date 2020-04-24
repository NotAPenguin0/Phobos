#ifndef PHOBOS_PHYSICAL_DEVICE_HPP_
#define PHOBOS_PHYSICAL_DEVICE_HPP_

#include <vulkan/vulkan.hpp>

#include <optional>

namespace ph {

struct QueueFamilyIndices {
    std::optional<size_t> graphics_family = std::nullopt;
};

struct SurfaceDetails {
    vk::SurfaceKHR handle;
    
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
};

struct PhysicalDeviceDetails {
    PhysicalDeviceDetails() {};
    PhysicalDeviceDetails(PhysicalDeviceDetails const& rhs) {
        handle = rhs.handle;
        properties = rhs.properties;
        memory_properties = rhs.memory_properties;
        features = rhs.features;

        queue_family_properties = rhs.queue_family_properties;
        queue_families = rhs.queue_families;

        supported_extensions = rhs.supported_extensions;
        surface_details = rhs.surface_details;
    }

    PhysicalDeviceDetails& operator=(PhysicalDeviceDetails const& rhs) {
        handle = rhs.handle;
        properties = rhs.properties;
        memory_properties = rhs.memory_properties;
        features = rhs.features;

        queue_family_properties = rhs.queue_family_properties;
        queue_families = rhs.queue_families;

        supported_extensions = rhs.supported_extensions;
        surface_details = rhs.surface_details;
        return *this;
    }

    vk::PhysicalDevice handle;

    vk::PhysicalDeviceProperties properties;
    vk::PhysicalDeviceMemoryProperties memory_properties;
    vk::PhysicalDeviceFeatures features;

    std::vector<vk::QueueFamilyProperties> queue_family_properties;
    QueueFamilyIndices queue_families;

    std::vector<vk::ExtensionProperties> supported_extensions;

    SurfaceDetails surface_details;
};

struct PhysicalDeviceRequirements {
    bool graphics_required = true;
    // This will always prefer a dedicated gpu over an integrated one
    bool prefer_dedicated_gpu = true;

    std::vector<const char*> required_extensions;
};

PhysicalDeviceDetails get_physical_device(vk::Instance instance, vk::SurfaceKHR surface, 
                                          PhysicalDeviceRequirements const& requirements);

}

#endif