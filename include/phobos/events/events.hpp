#ifndef PHOBOS_EVENTS_HPP_
#define PHOBOS_EVENTS_HPP_

#include <vulkan/vulkan.hpp>
#include <phobos/core/window_context.hpp>

namespace ph {

struct DynamicGpuBufferResizeEvent {
    vk::Buffer buffer_handle;
    vk::DescriptorSet descriptor_set;
    size_t new_size;
};

struct WindowResizeEvent {
    WindowContext* window_ctx;
    int32_t new_width;
    int32_t new_height;
};

struct SwapchainRecreateEvent {
    WindowContext* window_ctx;
};

}

#endif