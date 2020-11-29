#include <phobos/core/context.hpp>
#include <cassert>

namespace ph {

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
	VkDebugUtilsMessengerCallbackDataEXT const* callback_data, void* arg) {

	// Only log messages with severity 'warning' or above
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		ph::LogInterface* log = reinterpret_cast<ph::LogInterface*>(arg);
		if (log) {
			log->write_fmt(ph::LogSeverity::Warning, "{}", callback_data->pMessage);
		}
	}

	return VK_FALSE;
}

static std::optional<size_t> get_queue_family_prefer_dedicated(std::vector<VkQueueFamilyProperties> const& properties, VkQueueFlagBits required, VkQueueFlags avoid) {

	std::optional<size_t> best_match = std::nullopt;
	for (size_t i = 0; i < properties.size(); ++i) {
		VkQueueFlags flags = properties[i].queueFlags;
		if (!(flags & required)) { continue; }
		if (!(flags & avoid)) { return i; }
		best_match = i;
	}
	return best_match;
}

static PhysicalDevice select_physical_device(VkInstance instance, GPURequirements const& requirements) {
	uint32_t device_count;
	VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	assert(result == VK_SUCCESS && device_count > 0 && "Could not find any vulkan-capable devices");
	std::vector<VkPhysicalDevice> devices(device_count);
	result = vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
	for (VkPhysicalDevice device : devices) {
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(device, &properties);
		VkPhysicalDeviceMemoryProperties mem_properties{};
		vkGetPhysicalDeviceMemoryProperties(device, &mem_properties);
		// Check if the properties match the required settings
		if (requirements.dedicated && properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) continue;
		// Only do this check if there is a minimum video memory limit
		if (requirements.min_video_memory != 0) {
			uint64_t dedicated_video_memory = 0;
			for (uint32_t heap_idx = 0; heap_idx < mem_properties.memoryHeapCount; ++heap_idx) {
				VkMemoryHeap heap = mem_properties.memoryHeaps[heap_idx];
				if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
					dedicated_video_memory += heap.size;
				}
			}
			if (dedicated_video_memory < requirements.min_video_memory) continue;
		}

		// Check queues
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
		bool must_skip = false; // If we find a requested queue that isn't supported, skip this device.
		std::vector<QueueInfo> found_queues;
		for (QueueRequest queue : requirements.requested_queues) {
			// Find a queue with the desired capabilities
			VkQueueFlags avoid = {};
			if (queue.dedicated) {
				if (queue.type == QueueType::Graphics) avoid = VK_QUEUE_COMPUTE_BIT;
				if (queue.type == QueueType::Compute) avoid = VK_QUEUE_GRAPHICS_BIT;
				if (queue.type == QueueType::Transfer) avoid = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;
			}
			auto index = get_queue_family_prefer_dedicated(queue_families, static_cast<VkQueueFlagBits>(queue.type), avoid);
			// No queue was found
			if (index == std::nullopt) {
				must_skip = true;
				break;
			}

			// A queue was found.
			found_queues.push_back(QueueInfo{ .dedicated = !(queue_families[*index].queueFlags & avoid), .type = queue.type, .family_index = *index });
		}
		if (must_skip) continue;

		return { .handle = device, .properties = properties, .memory_properties = mem_properties, .found_queues = std::move(found_queues) };
	}

	// No matching device was found.
	return {};
}

static void fill_surface_details(PhysicalDevice& device) {
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.handle, device.surface->handle, &device.surface->capabilities);
	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device.handle, device.surface->handle, &format_count, nullptr);
	device.surface->formats.resize(format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device.handle, device.surface->handle, &format_count, device.surface->formats.data());
	uint32_t mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device.handle, device.surface->handle, &mode_count, nullptr);
	device.surface->present_modes.resize(mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device.handle, device.surface->handle, &mode_count, device.surface->present_modes.data());
}

Context::Context(AppSettings settings) {
	num_threads = settings.num_threads;
	has_validation = settings.enable_validation;
	if (!settings.create_headless) {
		wsi = settings.wsi;
	}
	log = settings.log;

	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.apiVersion = VK_API_VERSION_1_2;
		app_info.pEngineName = "Phobos";
		app_info.engineVersion = VK_MAKE_VERSION(VERSION.major, VERSION.minor, VERSION.patch);
		app_info.pApplicationName = settings.app_name.data();
		app_info.applicationVersion = VK_MAKE_VERSION(settings.app_version.major, settings.app_version.minor, settings.app_version.patch);

		VkInstanceCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		info.pApplicationInfo = &app_info;
		
		if (validation_enabled()) {
			const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
			info.ppEnabledLayerNames = layers;
			info.enabledLayerCount = sizeof(layers) / sizeof(const char*);
		}

		std::vector<const char*> extensions;
		if (!is_headless()) {
			extensions = wsi->window_extension_names();
			if (validation_enabled()) {
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}
			info.ppEnabledExtensionNames = extensions.data();
			info.enabledExtensionCount = extensions.size();
		}

		VkResult result = vkCreateInstance(&info, nullptr, &instance);
		assert(result == VK_SUCCESS && "Failed to create VkInstance");
	}

	// Create debug messenger
	if (validation_enabled()) {
		auto create_func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		assert(create_func && "VK_EXT_debug_utils not present");

		// Create debug messenger
		VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
		messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		messenger_info.pUserData = log;
		messenger_info.pfnUserCallback = vk_debug_callback;
		VkResult result = create_func(instance, &messenger_info, nullptr, &debug_messenger);
		assert(result == VK_SUCCESS && "Failed to create debug messenger");
	}

	phys_device = select_physical_device(instance, settings.gpu_requirements);
	if (!phys_device.handle) {
		log->write(LogSeverity::Fatal, "Could not find a suitable physical device");
		return;
	}

	phys_device.surface = SurfaceInfo{};
	phys_device.surface->handle = wsi->create_surface(instance);
	fill_surface_details(phys_device);

	// Get the logical device using the queues we found.
	{
		std::vector<VkDeviceQueueCreateInfo> queue_infos;
		float const priority = 1.0f;
		for (QueueInfo queue : phys_device.found_queues) {
			VkDeviceQueueCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			info.queueCount = 1;
			info.queueFamilyIndex = queue.family_index;
			info.pQueuePriorities = &priority;
			queue_infos.push_back(info);
		}

		VkDeviceCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		info.pQueueCreateInfos = queue_infos.data();
		info.queueCreateInfoCount = queue_infos.size();
		
		if (!is_headless()) {
			settings.gpu_requirements.device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}
		info.enabledExtensionCount = settings.gpu_requirements.device_extensions.size();
		info.ppEnabledExtensionNames = settings.gpu_requirements.device_extensions.data();
		info.pNext = settings.gpu_requirements.pNext;
		info.pEnabledFeatures = &settings.gpu_requirements.features;
		
		VkResult result = vkCreateDevice(phys_device.handle, &info, nullptr, &device);
		assert(result == VK_SUCCESS && "Failed to create logical device");
 	}
}

Context::~Context() {
	if (validation_enabled()) {
		auto destroy_func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		destroy_func(instance, debug_messenger, nullptr);
	}
	if (!is_headless()) {
		vkDestroySurfaceKHR(instance, phys_device.surface->handle, nullptr);
	}
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
}

bool Context::is_headless() const {
	return wsi == nullptr;
}

bool Context::validation_enabled() const {
	return has_validation;
}

}