#include <phobos/core/device.hpp>

namespace ph {

vk::Device create_device(PhysicalDeviceDetails const& physical_device, DeviceRequirements const& requirements) {
    vk::DeviceQueueCreateInfo queue_info;
    queue_info.queueFamilyIndex = physical_device.queue_families.graphics_family.value();
    queue_info.queueCount = 1;
    float priority = 1.0f;
    queue_info.pQueuePriorities = &priority;

    vk::DeviceQueueCreateInfo transfer_queue;
    transfer_queue.queueFamilyIndex = physical_device.queue_families.transfer_family.value();
    transfer_queue.queueCount = 1;
    transfer_queue.pQueuePriorities = &priority;

    vk::DeviceQueueCreateInfo compute_queue;
    compute_queue.queueFamilyIndex = physical_device.queue_families.compute_family.value();
    compute_queue.queueCount = 1;
    compute_queue.pQueuePriorities = &priority;

    vk::DeviceCreateInfo info;

    vk::DeviceQueueCreateInfo queues[]{ queue_info, transfer_queue, compute_queue };
    
    info.pQueueCreateInfos = queues;
    info.queueCreateInfoCount = 3;
    info.enabledExtensionCount = requirements.extensions.size();
    info.ppEnabledExtensionNames = requirements.extensions.data();
    info.pEnabledFeatures = &requirements.features;
    info.pNext = requirements.pNext;

    return physical_device.handle.createDevice(info);
}

}