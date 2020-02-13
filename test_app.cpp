#include <phobos/core/window_context.hpp>
#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/present_manager.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/renderer/imgui_renderer.hpp>

#include <GLFW/glfw3.h>
#include <iostream>

#include <imgui/imgui.h>

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

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    // Create present manager (required for presenting to the screen).
    ph::PresentManager present_manager(vulkan_context);
    ph::Renderer renderer(vulkan_context);
    ph::ImGuiRenderer imgui_renderer(window_context, vulkan_context);
    
    while(window_context.is_open()) {
        window_context.poll_events();
        present_manager.wait_for_available_frame();

        ///// FRAME START
        imgui_renderer.begin_frame();

        ph::FrameInfo frame_info = present_manager.get_frame_info();
        static bool show = true;
        ImGui::ShowDemoWindow(&show);

        renderer.render_frame(frame_info);
        imgui_renderer.render_frame(frame_info);

        present_manager.present_frame(frame_info);

        ///// FRAME END
    }

    // Wait until everything is done before deallocating
    vulkan_context.device.waitIdle();

    // Deallocate resources
    imgui_renderer.destroy();
    present_manager.destroy();
    vulkan_context.destroy();
    glfwDestroyWindow(window_context.handle);
    glfwTerminate();
}