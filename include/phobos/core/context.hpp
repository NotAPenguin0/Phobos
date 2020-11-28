#pragma once

#include <phobos/version.hpp>
#include <phobos/core/window_interface.hpp>

#include <vulkan/vulkan.h>
#include <string_view>
#include <vector>

namespace ph {

// This struct describes capabilities the user wants the GPU to have.
// If no GPU matching the requirements is found, the first GPU in the list of available devices is selected.
struct GPURequirements {
	bool dedicated = false;
	// Minimum amount of dedicated video memory (in bytes). This only counts memory heaps with the DEVICE_LOCAL bit set.
	uint64_t min_video_memory = 0;
	// TODO: Add queues and other settings here
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
	// Minimum GPU capabilities requested by the user. This will decide which GPU will be used.
	GPURequirements gpu_requirements{};
};

struct PhysicalDevice {
	VkPhysicalDevice handle = nullptr;
	VkPhysicalDeviceProperties properties{};
	VkPhysicalDeviceMemoryProperties memory_properties{};
};

struct PerThreadContext {

};

class Context {
public:
	Context(AppSettings const& settings);
	~Context();

	bool is_headless() const;
	bool validation_enabled() const;

	VkInstance instance = nullptr;
	VkDevice device = nullptr;
	PhysicalDevice phys_device{};

private:
	VkDebugUtilsMessengerEXT debug_messenger = nullptr;
	bool has_validation = false;
	uint32_t num_threads = 0;
	std::vector<PerThreadContext> ptcs{};
	WindowInterface* wsi = nullptr;
};

}