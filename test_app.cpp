#include <phobos/core/window_context.hpp>
#include <phobos/core/vulkan_context.hpp>

#include <phobos/present/present_manager.hpp>

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

    // Create present manager (required for presenting to the screen).
    ph::PresentManager present_manager(vulkan_context);

    while(window_context.is_open()) {
        present_manager.wait_for_available_frame();

        ph::FrameInfo frame_info = present_manager.render_frame();
        present_manager.present_frame(frame_info);

        window_context.poll_events();
    }

    vulkan_context.device.waitIdle();
    present_manager.destroy();
    vulkan_context.destroy();
    glfwDestroyWindow(window_context.handle);
    glfwTerminate();
}