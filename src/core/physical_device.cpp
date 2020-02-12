#include <phobos/core/physical_device.hpp>

#include <vector>

namespace ph {

static QueueFamilyIndices get_queue_families(PhysicalDeviceDetails& physical_device, vk::SurfaceKHR surface) {
    QueueFamilyIndices indices;
    physical_device.queue_family_properties = physical_device.handle.getQueueFamilyProperties();

    for (size_t i = 0; i < physical_device.queue_family_properties.size(); ++i) {
        auto const& family = physical_device.queue_family_properties[i];

        // Check if the queue family has graphics support
        if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = i;
        }
    }

    return indices;
}

bool check_required_extensions(PhysicalDeviceDetails& details, PhysicalDeviceRequirements const& requirements) {
    details.supported_extensions = details.handle.enumerateDeviceExtensionProperties();
    for (auto const& name : requirements.required_extensions) {
        bool found = false;
        
        for (auto const& ext : details.supported_extensions) {
            if (std::strcmp(ext.extensionName, name)) {
                found = true;
            }
        }

        // These extensions are required
        if (!found) {
            return false;
        }
    }

    return true;
}

SurfaceDetails get_surface_details(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    SurfaceDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.present_modes = device.getSurfacePresentModesKHR(surface);
    details.handle = surface;

    return details;
}

PhysicalDeviceDetails get_physical_device(vk::Instance instance, vk::SurfaceKHR surface, 
                                          PhysicalDeviceRequirements const& requirements) {
    std::vector<PhysicalDeviceDetails> devices;
    // Find all available devices
    std::vector<vk::PhysicalDevice> available_devices = instance.enumeratePhysicalDevices();
    devices.reserve(available_devices.size());
    for (auto const& device : available_devices) {
        devices.emplace_back();
        devices.back().handle = device;
    }

    if (devices.empty()) {
        throw std::runtime_error("No physical devices available");
    }

    int best_device_index = -1;
    size_t max_device_score = 0;
    for (size_t i = 0; i < devices.size(); ++i) {

        auto& device = devices[i];
        bool discard_device = false;
        size_t device_score = 0;

        device.features = device.handle.getFeatures();
        device.properties = device.handle.getProperties();

        device.queue_families = get_queue_families(device, surface);

        // Check if this device has all required queue families
        if (requirements.graphics_required && !device.queue_families.graphics_family.has_value()) {
            discard_device = true;
        } 

        if (requirements.prefer_dedicated_gpu) {
            if (device.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                device_score += 1000;
            } else if (device.properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
                device_score += 500;
            } 
        }

        if (!check_required_extensions(device, requirements)) {
            discard_device = true;
        }

        device.surface_details = get_surface_details(device.handle, surface);
        // We need at least one surface format and one present mode
        if (device.surface_details.present_modes.empty() || device.surface_details.formats.empty()) {
            discard_device = true;
        }

        // Check if the device supports presenting on this surface
        if (!device.handle.getSurfaceSupportKHR(device.queue_families.graphics_family.value(), surface)) {
            discard_device = true;
        }

        // If the device wasn't discarded, check if it's better than the previous device
        if (!discard_device) {
            // Add one to all device scores (temp)
            if (device_score > max_device_score) {
                best_device_index  = i;
                max_device_score = device_score;
            }
        }
    }

    if (best_device_index < 0) {
        throw std::runtime_error("No suitable physical devices found.");
    }

    return devices[best_device_index];
}

}