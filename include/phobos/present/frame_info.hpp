#ifndef PHOBOS_PRESENT_FRAME_INFO_HPP_
#define PHOBOS_PRESENT_FRAME_INFO_HPP_

#include <vulkan/vulkan.hpp>

namespace ph {

struct FrameInfo {
    vk::CommandBuffer command_buffer;

    vk::Fence fence;

    vk::Semaphore image_available;
    vk::Semaphore render_finished;
};

}

#endif