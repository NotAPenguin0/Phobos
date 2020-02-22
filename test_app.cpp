#include <phobos/core/window_context.hpp>
#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/present_manager.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/renderer/imgui_renderer.hpp>
#include <phobos/renderer/texture.hpp>
#include <phobos/util/log_interface.hpp>

#include <stb/stb_image.h>

#include <mimas/mimas.h>
#include <mimas/mimas_vk.h>
#include <iostream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui/imgui.h>

#include "imgui_style.hpp"


void make_ui(int draw_calls) {
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PopStyleVar(3);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                         ImGuiDockNodeFlags_None);
    }

    static bool show_stats = true;
    if (ImGui::Begin("Renderer stats", &show_stats)) {
        int fps = ImGui::GetIO().Framerate;
        if (fps > 120) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        } else if (fps > 60) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
        }
        ImGui::Text("FPS: %i", (int)ImGui::GetIO().Framerate);
        ImGui::PopStyleColor();
        ImGui::Text("Draw calls (ImGui excluded): %i", draw_calls);
    }

    ImGui::End();

    // End dockspace
}

class DefaultLogger : public ph::log::LogInterface {
public:
    void write(ph::log::Severity severity, std::string_view str) override {
        if (timestamp_enabled) {
            std::cout << get_timestamp_string() << ": ";
        }
        std::cout << str << "\n";
    }
};


int main() {
    DefaultLogger logger;
    logger.set_timestamp(true);
    // Create window context
    ph::WindowContext window_context = ph::create_window_context("Phobos Test App", 1280, 720);

    // Create Vulkan context
    ph::AppSettings settings;
    settings.enable_validation_layers = true;
    settings.version = ph::Version{0, 0, 1};
    ph::VulkanContext* vulkan_context = ph::create_vulkan_context(window_context, &logger, settings);
  
    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    style_theme_grey();

    // Create present manager (required for presenting to the screen).
    ph::PresentManager present_manager(*vulkan_context);
    ph::Renderer renderer(*vulkan_context);
    ph::ImGuiRenderer imgui_renderer(window_context, *vulkan_context);

    logger.write_fmt(ph::log::Severity::Info, "Created renderers");

    size_t draw_calls = 0;

    float vertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, 0.0, 0.0f, 0.0f
    };

    uint32_t indices[] = {
        0, 1, 2, 2, 3, 0
    };

    ph::Mesh::CreateInfo triangle_info;
    triangle_info.ctx = vulkan_context;
    triangle_info.vertices = vertices;
    triangle_info.vertex_count = 4;
    triangle_info.vertex_size = 5;
    triangle_info.indices = indices;
    triangle_info.index_count = 6;
    ph::Mesh triangle(triangle_info);

    int w, h, channels;
    uint8_t* img = stbi_load("data/textures/pengu.png", &w, &h, &channels, STBI_rgb_alpha);
    ph::Texture::CreateInfo tex_info;
    tex_info.ctx = vulkan_context;
    tex_info.channels = channels;
    tex_info.format = vk::Format::eR8G8B8A8Srgb;
    tex_info.width = w;
    tex_info.height = h;
    tex_info.data = img;
    ph::Texture pengu(tex_info);
    stbi_image_free(img);

    ph::Material default_material;
    default_material.texture = &pengu;

    logger.write_fmt(ph::log::Severity::Info, "Loaded assets");

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)window_context.width / (float)window_context.height, 
        0.1f, 100.0f);
    projection[1][1] *= -1;
    glm::mat4 view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    while(window_context.is_open()) {
        window_context.poll_events();
        present_manager.wait_for_available_frame();

        ///// FRAME START
        imgui_renderer.begin_frame();

        ph::FrameInfo& frame_info = present_manager.get_frame_info();

        // Imgui
        make_ui(draw_calls);

        ph::RenderGraph render_graph;
        render_graph.meshes.push_back(&triangle);
        render_graph.materials.push_back(&default_material);
        ph::RenderGraph::DrawCommand draw;
        draw.mesh_index = 0;
        draw.material_index = 0;
        draw.instances = { 
            { glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 0.0f)) }, 
            { glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) }
        };
        render_graph.draw_commands.push_back(draw);

        render_graph.projection = projection;
        render_graph.view = view;

        renderer.render_frame(frame_info, render_graph);
        imgui_renderer.render_frame(frame_info);

        present_manager.present_frame(frame_info);

        draw_calls = frame_info.draw_calls;

        ///// FRAME END
    }

    logger.write_fmt(ph::log::Severity::Info, "Exiting");

    // Wait until everything is done before deallocating
    vulkan_context->device.waitIdle();

    triangle.destroy();
    pengu.destroy();
    // Deallocate resources
    renderer.destroy();
    imgui_renderer.destroy();
    present_manager.destroy();
    vulkan_context->destroy();
    delete vulkan_context;
    mimas_destroy_window(window_context.handle);
    mimas_terminate();
}