#include <phobos/core/physical_device.hpp>

#include <vector>
#include <stl/vector.hpp>
#include <stl/enumerate.hpp>

namespace ph {

static std::optional<size_t> get_family_prefer_dedicated(std::vector<vk::QueueFamilyProperties> const& properties, 
        vk::QueueFlagBits required, vk::QueueFlags avoid) {

    std::optional<size_t> best_match = std::nullopt;
    for (size_t i = 0; i < properties.size(); ++i) {
        vk::QueueFlags flags = properties[i].queueFlags;
        if (!(flags & required)) { continue; }
        if (!(flags & avoid)) { return i; }
        best_match = i;
    }
    return best_match;
}

static QueueFamilyIndices get_queue_families(PhysicalDeviceDetails& physical_device, vk::SurfaceKHR surface) {
    QueueFamilyIndices indices;
    physical_device.queue_family_properties = physical_device.handle.getQueueFamilyProperties();

    indices.graphics_family = get_family_prefer_dedicated(physical_device.queue_family_properties, vk::QueueFlagBits::eGraphics, 
        vk::QueueFlagBits::eCompute);
    indices.transfer_family = get_family_prefer_dedicated(physical_device.queue_family_properties, vk::QueueFlagBits::eTransfer,
        vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
    indices.compute_family = get_family_prefer_dedicated(physical_device.queue_family_properties, vk::QueueFlagBits::eCompute,
        vk::QueueFlagBits::eGraphics);

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
    stl::vector<PhysicalDeviceDetails> devices;
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
    for (auto[index, device] : stl::enumerate(devices.begin(), devices.end())) {

        bool discard_device = false;
        size_t device_score = 0;

        device.features = device.handle.getFeatures();
        device.properties = device.handle.getProperties();
        device.memory_properties = device.handle.getMemoryProperties();
        

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
                best_device_index  = index;
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