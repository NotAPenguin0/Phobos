#include <phobos/core/context.hpp>
#include <cassert>

#include <iostream>

namespace ph {

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
	VkDebugUtilsMessengerCallbackDataEXT const* callback_data, void*) {

	// Only log messages with severity 'warning' or above
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		// TODO: Add proper logging
		std::cout << callback_data->pMessage << "\n";
	}

	return VK_FALSE;
}

static PhysicalDevice select_physical_device(VkInstance instance, GPURequirements requirements) {
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

		return { device, properties, mem_properties };
	}

	// No matching device was found, return the first in the list
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(devices[0], &properties);
	VkPhysicalDeviceMemoryProperties mem_properties{};
	vkGetPhysicalDeviceMemoryProperties(devices[0], &mem_properties);
	return { devices[0], properties, mem_properties };
}

Context::Context(AppSettings const& settings) {
	num_threads = settings.num_threads;
	has_validation = settings.enable_validation;
	if (!settings.create_headless) {
		wsi = settings.wsi;
	}

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
		messenger_info.pfnUserCallback = vk_debug_callback;
		VkResult result = create_func(instance, &messenger_info, nullptr, &debug_messenger);
		assert(result == VK_SUCCESS && "Failed to create debug messenger");
	}

	phys_device = select_physical_device(instance, settings.gpu_requirements);
}

Context::~Context() {

}

bool Context::is_headless() const {
	return wsi == nullptr;
}

bool Context::validation_enabled() const {
	return has_validation;
}

}