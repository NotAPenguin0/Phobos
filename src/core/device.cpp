#include <phobos/core/device.hpp>

namespace ph {

vk::Device create_device(PhysicalDeviceDetails const& physical_device, DeviceRequirements const& requirements) {
    // We only need 1 graphics queue
    vk::DeviceQueueCreateInfo queue_info;
    queue_info.queueFamilyIndex = physical_device.queue_families.graphics_family.value();
    queue_info.queueCount = 1;
    float priority = 1.0f;
    queue_info.pQueuePriorities = &priority;

    vk::DeviceCreateInfo info;
    info.pQueueCreateInfos = &queue_info;
    info.queueCreateInfoCount = 1;
    info.enabledExtensionCount = requirements.extensions.size();
    info.ppEnabledExtensionNames = requirements.extensions.data();
    info.pEnabledFeatures = &requirements.features;

    return physical_device.handle.createDevice(info);
}

}