#pragma once

#include <phobos/context.hpp>

#include <phobos/pipeline.hpp>

namespace ph {
namespace impl {

class ImageImpl;

class ContextImpl {
public:
	ContextImpl(AppSettings settings);
	~ContextImpl();

	bool is_headless() const;
	bool validation_enabled() const;
	uint32_t thread_count() const;

	Queue* get_queue(QueueType type);
	// Gets the pointer to the first present-capable queue.
	Queue* get_present_queue();

	void next_frame();

	void name_object(ph::Pipeline const& pipeline, std::string const& name);
	void name_object(VkRenderPass pass, std::string const& name);
	void name_object(VkFramebuffer framebuf, std::string const& name);

	// Only used on initialization
	void post_init(Context& ctx, ImageImpl& image_impl, AppSettings const& settings);

	VkInstance instance = nullptr;
	VkDevice device = nullptr;
	VmaAllocator allocator = nullptr;
	PhysicalDevice phys_device;

	// This is std::nullopt for a headless context
	std::optional<Swapchain> swapchain = std::nullopt;

	uint32_t const max_unbounded_array_size = 0;
	uint32_t const max_frames_in_flight = 0;
private:
	ImageImpl* img = nullptr;

	VkDebugUtilsMessengerEXT debug_messenger = nullptr;
	bool const has_validation = false;

	uint32_t const num_threads = 0;
	std::vector<PerThreadContext> ptcs{};


	WindowInterface* wsi = nullptr;
	LogInterface* log = nullptr;

	std::vector<Queue> queues{};

	PFN_vkSetDebugUtilsObjectNameEXT set_debug_utils_name_fun;
};

} // namespace impl
} // namespace ph