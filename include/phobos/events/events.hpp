#ifndef PHOBOS_EVENTS_HPP_
#define PHOBOS_EVENTS_HPP_

#include <vulkan/vulkan.hpp>

namespace ph {

struct InstancingBufferResizeEvent {
    vk::Buffer buffer_handle;
    vk::DescriptorSet descriptor_set;
    size_t new_size;
};

}

#endif