#ifndef PHOBOS_PRESENT_MANAGER_HPP_
#define PHOBOS_PRESENT_MANAGER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>

#include <vector>

// Proposed API:
/*
ph::PresentManager present_mngr;
while(app_loop) {
  ph::RenderGraph render_graph = build_render_graph(scene);
  ph::FrameInfo frame_info = present_mngr.render_frame(render_graph);
  present_mng.present_frame(frame_info);
  // Might return instantly if there is a next frame available already.
  present_mngr.wait_until_next_frame();
}
*/

namespace ph {

class PresentManager {
public:
    PresentManager(VulkanContext& ctx, size_t max_frames_in_flight = 2);

    // Builds command buffers from rendergraph and returns them as a FrameInfo struct
    FrameInfo& render_frame(/*RenderGraph*/);

    // Presents the frame 
    void present_frame(FrameInfo& frame);

    // Waits until a new frame is available for submitting
    void wait_for_available_frame();

    void destroy();

private:
    VulkanContext& context;

    size_t max_frames_in_flight;
    size_t frame_index = 0;
    uint32_t image_index = 0;
    std::vector<vk::Fence> image_in_flight_fences;
    std::vector<FrameInfo> frames;

    vk::CommandPool command_pool;
    std::vector<vk::CommandBuffer> command_buffers;
};

}

#endif