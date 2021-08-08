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

	[[nodiscard]] InThreadContext begin_thread(uint32_t thread_index);
	void end_thread(uint32_t thread_index);

	void name_object(ph::Pipeline const& pipeline, std::string const& name);
	void name_object(VkRenderPass pass, std::string const& name);
	void name_object(VkFramebuffer framebuf, std::string const& name);
	void name_object(VkBuffer buffer, std::string const& name);
	void name_object(VkImage image, std::string const& name);
	void name_object(ph::RawImage const& image, std::string const& name);
	void name_object(ph::ImageView const& view, std::string const& name);
	void name_object(VkFence fence, std::string const& name);
	void name_object(VkSemaphore semaphore, std::string const& name);
	void name_object(VkQueue queue, std::string const& name);
	void name_object(VkCommandPool pool, std::string const& name);
	void name_object(ph::CommandBuffer const& cmd_buf, std::string const& name);

	VkFence create_fence();
	VkResult wait_for_fence(VkFence fence, uint64_t timeout);
	void reset_fence(VkFence fence);
	void destroy_fence(VkFence fence);

	VkSemaphore create_semaphore();
	void destroy_semaphore(VkSemaphore semaphore);

	VkSampler create_sampler(VkSamplerCreateInfo info);
	void destroy_sampler(VkSampler sampler);

	VkQueryPool create_query_pool(VkQueryType type, uint32_t count);
	void destroy_query_pool(VkQueryPool pool);

	// Only used on initialization
	void post_init(Context& ctx, ImageImpl& image_impl, AppSettings const& settings);

	template<typename... Args>
	void log(LogSeverity sev, std::string_view format, Args&&... args) const {
		logger->write_fmt(sev, format, std::forward<Args>(args)...);
	}

	LogInterface* get_logger();

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

	PFN_vkSetDebugUtilsObjectNameEXT set_debug_utils_name_fun;
	VkDebugUtilsMessengerEXT debug_messenger = nullptr;
	bool const has_validation = false;

	uint32_t const num_threads = 0;
	std::vector<PerThreadContext> ptcs{};

	WindowInterface* wsi = nullptr;
	LogInterface* logger = nullptr;

	std::vector<Queue> queues{};
};

} // namespace impl
} // namespace ph