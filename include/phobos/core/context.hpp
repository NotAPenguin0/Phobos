#pragma once

#include <phobos/version.hpp>
#include <phobos/core/window_interface.hpp>
#include <phobos/core/log_interface.hpp>
#include <phobos/core/queue.hpp>

#include <vulkan/vulkan.h>
#include <string_view>
#include <vector>
#include <optional>

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

	std::vector<VkImage> images{};
	std::vector<VkImageView> view{};
};

struct PerThreadContext {

};

class Context {
public:
	Context(AppSettings settings);
	~Context();

	bool is_headless() const;
	bool validation_enabled() const;
	std::optional<Queue*> get_queue(QueueType type);

	VkInstance instance = nullptr;
	VkDevice device = nullptr;
	PhysicalDevice phys_device{};
	// This is std::nullopt for a headless context
	std::optional<Swapchain> swapchain = std::nullopt;
private:
	VkDebugUtilsMessengerEXT debug_messenger = nullptr;
	bool has_validation = false;
	uint32_t num_threads = 0;
	std::vector<PerThreadContext> ptcs{};
	std::vector<Queue> queues{};
	WindowInterface* wsi = nullptr;
	LogInterface* log = nullptr;
};

}