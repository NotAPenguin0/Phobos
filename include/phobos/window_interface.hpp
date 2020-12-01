#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace ph {

class WindowInterface {
public:
	virtual std::vector<const char*> window_extension_names() const = 0;
	virtual VkSurfaceKHR create_surface(VkInstance instance) const = 0;
	virtual uint32_t width() const = 0;
	virtual uint32_t height() const = 0;
private:

};

}