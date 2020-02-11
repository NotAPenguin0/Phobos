#include <phobos/window_context.hpp>
#include <phobos/vulkan_context.hpp>

#include <GLFW/glfw3.h>

#include <iostream>

void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
    fflush(stderr);
}


int main() {
    glfwSetErrorCallback(glfw_error_callback);

    // Create window context
    ph::WindowContext window_context = ph::create_window_context("Phobos Test App", 1280, 720);

    // Create Vulkan context
    ph::AppSettings settings;
    settings.enable_validation_layers = true;
    settings.version = ph::Version{0, 0, 1};
    ph::VulkanContext vulkan_context = ph::create_vulkan_context(window_context, settings);

    std::cout << "Created Phobos Vulkan context with physical device " << vulkan_context.physical_device.properties.deviceName
              << std::endl;

    while(window_context.is_open()) {
        window_context.poll_events();
    }

    vulkan_context.destroy();
    glfwDestroyWindow(window_context.handle);
    glfwTerminate();
}