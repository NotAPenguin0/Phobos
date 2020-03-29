#ifndef PHOBOS_PRESENT_MANAGER_HPP_
#define PHOBOS_PRESENT_MANAGER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/frame_info.hpp>
#include <phobos/events/event_dispatcher.hpp>
#include <phobos/renderer/render_target.hpp>
#include <phobos/renderer/render_attachment.hpp>

#include <unordered_map>
#include <stl/vector.hpp>

namespace ph {

class PresentManager : public EventListener<WindowResizeEvent> {
public:
    PresentManager(VulkanContext& ctx, size_t max_frames_in_flight = 2);

    // Gets data for the current frame, like the framebuffer to use, command buffers, etc
    FrameInfo& get_frame_info();

    // Presents the frame 
    void present_frame(FrameInfo& frame);

    // Waits until a new frame is available for submitting
    void wait_for_available_frame();

    void destroy();

    RenderAttachment add_color_attachment(std::string const& name);
    RenderAttachment add_depth_attachment(std::string const& name);
    RenderAttachment& get_attachment(std::string const& name);
    RenderAttachment get_swapchain_attachment(FrameInfo& frame);

protected:
    void on_event(WindowResizeEvent const& evt) override;
private:
    VulkanContext& context;

    size_t max_frames_in_flight;
    size_t frame_index = 0;
    uint32_t image_index = 0;
    stl::vector<vk::Fence> image_in_flight_fences;
    stl::vector<FrameInfo> frames;

    vk::CommandPool command_pool;
    std::vector<vk::CommandBuffer> command_buffers;

    // Pool for all "fixed" descriptors
    vk::DescriptorPool fixed_descriptor_pool;

    // Samplers
    vk::Sampler default_sampler;

    std::unordered_map<std::string, RenderAttachment> attachments;
};

}

#endif