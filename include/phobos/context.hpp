#pragma once

#include <phobos/version.hpp>
#include <phobos/window_interface.hpp>
#include <phobos/log_interface.hpp>
#include <phobos/queue.hpp>
#include <phobos/ring_buffer.hpp>
#include <phobos/image.hpp>
#include <phobos/attachment.hpp>
#include <phobos/cache.hpp>
#include <phobos/pipeline.hpp>
#include <phobos/shader.hpp>
#include <phobos/buffer.hpp>

#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

#include <string_view>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>
#include <cstddef>

namespace ph {

// This struct describes capabilities the user wants the GPU to have.
struct GPURequirements {
	bool dedicated = false;
	// Minimum amount of dedicated video memory (in bytes). This only counts memory heaps with the DEVICE_LOCAL bit set.
	uint64_t min_video_memory = 0;
	// Request queues and add them to this list
	std::vector<QueueRequest> requested_queues;
	// Device extensions and features
	std::vector<const char*> device_extensions;
	VkPhysicalDeviceFeatures features{};
	// To add a pNext chain to the VkDeviceCreateInfo
	void* pNext = nullptr;
};

struct AppSettings {
	// Application name and version. These are passed to vulkan directly, and could show up in tools like NSight for example.
	std::string_view app_name;
	Version app_version = Version{ 0, 0, 1 };
	// Whether to use the default vulkan validation layers.
	// Defaults to false.
	bool enable_validation = false;
	// Amount of threads phobos can be used from. Using any more threads than this amount is 
	// undefined behaviour and can lead to race conditions.
	uint32_t num_threads = 1;
	// Whether to use phobos without a window. This disables all swapchain functionality.
	// Useful for compute-only applications.
	// Settings this flag to true will ignore the WindowInterface specified.
	bool create_headless = false;
	// Pointer to the windowing system interface.
	// Note that this pointer must be valid for the entire lifetime of the created ph::Context
	WindowInterface* wsi = nullptr;
	// Pointer to the logging interface.
	// Like the wsi pointer, this must be valid for the entire lifetime of the ph::Context
	LogInterface* log = nullptr;
	// Minimum GPU capabilities requested by the user. This will decide which GPU will be used.
	GPURequirements gpu_requirements{};
	// Surface format. If this is not available the best available fallback will be picked.
	// When create_headless is true, this value is ignored.
	// The default value for this, and also the fallback value, is B8G8R8A8_SRGB with COLORSPACE_SRGB_NONLINEAR
	VkSurfaceFormatKHR surface_format{ .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	// Preferred present mode. The default value for this is FIFO, as this is always supported.
	// If your requested present mode is not supported, this will fallback to FIFO
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	// Minimum amount of swapchain images to present to.
	// The final value chosen is min(surface.maxImageCount, max(surface.minImageCount + 1, min_swapchain_image_count))
	uint32_t min_swapchain_image_count = 1;
	// Maximum amount of in-flight frames. Default value is 2. This value is ignored for a headless context.
	uint32_t max_frames_in_flight = 2;
	// Maximum size of an unbounded sampler array
	uint32_t max_unbounded_array_size = 4096;
};

struct SurfaceInfo {
	VkSurfaceKHR handle = nullptr;
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats{};
	std::vector<VkPresentModeKHR> present_modes{};
};

struct PhysicalDevice {
	VkPhysicalDevice handle = nullptr;
	VkPhysicalDeviceProperties properties{};
	VkPhysicalDeviceMemoryProperties memory_properties{};
	std::vector<VkQueueFamilyProperties> queue_families{};
	// Contains details of all used queues.
	std::vector<QueueInfo> found_queues{};
	// This is std::nullopt for a headless context
	std::optional<SurfaceInfo> surface = std::nullopt;
};

struct Swapchain {
	VkSwapchainKHR handle = nullptr;
	VkSurfaceFormatKHR format{};
	VkPresentModeKHR present_mode{};
	VkExtent2D extent{};

	struct PerImage {
		RawImage image{};
		ImageView view{};
		VkFence fence = nullptr;
	};

	std::vector<PerImage> per_image{};
	uint32_t image_index = 0;
};

struct PerThreadContext {

};

namespace impl {
	// Responsible for initialization and destruction and core context management.
	class ContextImpl;
	// Responsible for creating and managing renderpasses, framebuffers and attachments
	class AttachmentImpl;
	// Responsible for creating and managing pipelines and shaders
	class PipelineImpl;
	// Responsible for managing frames and presenting.
	class FrameImpl;
	// Responsible for managing caches
	class CacheImpl;
	// Responsible for creating and operating on images
	class ImageImpl;
	// Responsible for creating and operating on buffers
	class BufferImpl;
}

class Context {
public:
	Context(AppSettings settings);
	~Context();

	bool is_headless() const;
	bool validation_enabled() const;
	uint32_t thread_count() const;

	Queue* get_queue(QueueType type);
	// Gets the pointer to the first present-capable queue.
	Queue* get_present_queue();

	size_t max_frames_in_flight() const;
	void wait_for_frame();
	void submit_frame_commands(Queue& queue, CommandBuffer& cmd_buf);
	void present(Queue& queue);

	Attachment* get_attachment(std::string_view name);
	void create_attachment(std::string_view name, VkExtent2D size, VkFormat format);
	bool is_swapchain_attachment(std::string const& name);
	std::string get_swapchain_attachment_name() const;

	ShaderHandle create_shader(std::string_view path, std::string_view entry_point, PipelineStage stage);
	void reflect_shaders(ph::PipelineCreateInfo& pci);
	void create_named_pipeline(ph::PipelineCreateInfo pci);
	ShaderMeta const& get_shader_meta(std::string_view pipeline_name);
	VkSampler basic_sampler();

	RawImage create_image(ImageType type, VkExtent2D size, VkFormat format);
	void destroy_image(RawImage& image);
	ImageView create_image_view(RawImage const& target, ImageAspect aspect = ImageAspect::Color);
	void destroy_image_view(ImageView& view);

	RawBuffer create_buffer(BufferType type, VkDeviceSize size);
	void destroy_buffer(RawBuffer& buffer);
	bool is_valid_buffer(RawBuffer const& buffer);
	bool has_persistent_mapping(RawBuffer const& buffer);
	// For a persistently mapped buffer, this does not remap memory, and instead returns the pointer immediately.
	// Persistently mapped buffers are buffers with buffer type MappedUniformBuffer and StorageBufferDynamic
	std::byte* map_memory(RawBuffer& buffer);
	// Flushes memory owned by the buffer passed in. For memory that is not host-coherent, this is required to make
	// changes visible to the gpu after writing to mapped memory. If you want to flush the whole mapped range, size can be VK_WHOLE_SIZE
	void flush_memory(BufferSlice slice);
	void unmap_memory(RawBuffer& buffer);
	// Resizes the raw buffer to be able to hold at least requested_size bytes. Returns whether a reallocation occured. 
	// Note: If the buffer needs a resize, the old contents will be lost.
	bool ensure_buffer_size(RawBuffer& buf, VkDeviceSize requested_size);
private:
	// Pointers to implementation classes. We declare these in a very specific order so reverse destruction order can take care of the proper destruction order.
	// The reason image_impl is above context_impl is because we need to destroy image views inside the context destructor. Luckily, the image implementation does not need to do anything upon destruction, 
	// so we can safely destruct it last.

	// The reasoning for using multiple pimpl's here is to split up the Context class into logical modules, mostly to avoid creating one huge class with a massive implementation file. 
	// This way it's much easier to navigate the implementation code.

	std::unique_ptr<impl::ImageImpl> image_impl;
	std::unique_ptr<impl::BufferImpl> buffer_impl;
	std::unique_ptr<impl::ContextImpl> context_impl;
	// Will not be created for a headless context.
	std::unique_ptr<impl::FrameImpl> frame_impl;
	std::unique_ptr<impl::AttachmentImpl> attachment_impl;
	std::unique_ptr<impl::PipelineImpl> pipeline_impl;
	std::unique_ptr<impl::CacheImpl> cache_impl;

	// These classes need access to the following functions part of the private phobos API.
	friend class Queue;
	friend class CommandBuffer;
	friend class RenderGraph;
	friend class DescriptorBuilder;

	VkDevice device();

	VkFramebuffer get_or_create_framebuffer(VkFramebufferCreateInfo const& info);
	VkRenderPass get_or_create_renderpass(VkRenderPassCreateInfo const& info);
	VkDescriptorSetLayout get_or_create_descriptor_set_layout(DescriptorSetLayoutCreateInfo const& dslci);
	PipelineLayout get_or_create_pipeline_layout(PipelineLayoutCreateInfo const& plci, VkDescriptorSetLayout set_layout);
	Pipeline get_or_create_pipeline(std::string_view name, VkRenderPass render_pass);
	VkDescriptorSet get_or_create_descriptor_set(DescriptorSetBinding set_binding, Pipeline const& pipeline, void* pNext = nullptr);
};

}