#include <phobos/window_context.hpp>
#include <phobos/vulkan_context.hpp>

int main() {
    ph::WindowContext window_context = ph::create_window_context("Phobos Test App", 1280, 720);
    
    while(window_context.is_open()) {
        window_context.poll_events();
        window_context.swap_buffers();
    }
}