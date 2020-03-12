#include <phobos/core/window_context.hpp>
#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/present_manager.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/renderer/imgui_renderer.hpp>
#include <phobos/renderer/texture.hpp>
#include <phobos/util/log_interface.hpp>

#include <phobos/assets/asset_manager.hpp>

#include <stb/stb_image.h>

#include <mimas/mimas.h>
#include <mimas/mimas_vk.h>

#include <numeric>
#include <iostream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui/imgui.h>

#include "imgui_style.hpp"

struct Scene {
    ph::PointLight light;
};

void make_ui(int draw_calls, Scene& scene, ph::FrameInfo& info) {
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
        ImGui::Text("Frametime: %f ms", ImGui::GetIO().DeltaTime * 1000);
        ImGui::Text("FPS: %i", (int)ImGui::GetIO().Framerate);
        ImGui::Text("Draw calls (ImGui excluded): %i", draw_calls);
    }

    ImGui::End();

    static bool show_scene_info = true;
    if (ImGui::Begin("Scene Info", &show_scene_info)) {
        ImGui::DragFloat3("light_pos", &scene.light.position.x);
        ImGui::ColorEdit3("light_ambient", &scene.light.ambient.x);
        ImGui::ColorEdit3("light_diffuse", &scene.light.diffuse.x);
        ImGui::ColorEdit3("light_specular", &scene.light.specular.x);
    }

    ImGui::End();

    static bool show_scene;
    if (ImGui::Begin("Scene", &show_scene)) {
        auto img = info.present_manager->get_attachment(info, "color1");
        ImGui::Image(img.get_imgui_tex_id(), ImVec2(img.get_width(), img.get_height()));
    }

    ImGui::End();
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

void mouse_button_callback(Mimas_Window* win, Mimas_Key button, Mimas_Mouse_Button_Action action, void* user_data) {
    ph::VulkanContext* ctx = reinterpret_cast<ph::VulkanContext*>(user_data);
    if (action == MIMAS_MOUSE_BUTTON_PRESS) {

    }
}

int main() {
    DefaultLogger logger;
    logger.set_timestamp(true);
    // Create window context
    ph::WindowContext* window_context = ph::create_window_context("Phobos Test App", 1280, 720);

    // Create Vulkan context
    ph::AppSettings settings;
    settings.enable_validation_layers = true;
    settings.version = ph::Version{0, 0, 1};
    ph::VulkanContext* vulkan_context = ph::create_vulkan_context(*window_context, &logger, settings);
    
    mimas_set_window_mouse_button_callback(window_context->handle, mouse_button_callback, vulkan_context);
    
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
    ph::ImGuiRenderer imgui_renderer(*window_context, *vulkan_context);

    logger.write_fmt(ph::log::Severity::Info, "Created renderers");

    size_t draw_calls = 0;

    float vertices[] = {
        -1, -1, -1, 0, 0, -1, 1, 1,    1, 1, -1, 0, 0, -1, 0, 0,     1,  -1, -1, 0, 0, -1, 0, 1,
        1,  1, -1, 0, 0, -1, 0, 0,    -1, -1, -1, 0, 0, -1, 1, 1,   -1, 1, -1, 0, 0, -1, 1, 0,

        -1, -1, 1, 0, 0, 1, 0, 1,    1, -1, 1, 0, 0, 1, 1, 1,     1, 1, 1, 0, 0, 1, 1, 0,
        1,  1,  1, 0, 0, 1, 1, 0,    -1, 1, 1, 0, 0, 1, 0, 0,     -1, -1, 1, 0, 0, 1, 0, 1,

        -1, 1, -1, -1, 0, 0, 0, 0,    -1, -1, -1, -1, 0, 0, 0, 1,   -1, 1, 1, -1, 0, 0, 1, 0,
        -1, -1, -1, -1, 0, 0, 0, 1,   -1, -1, 1, -1, 0, 0, 1, 1,    -1, 1, 1, -1, 0, 0, 1, 0,

        1, 1, 1, 1, 0, 0, 0, 0,      1, -1, -1, 1, 0, 0, 1, 1,    1, 1, -1, 1, 0, 0, 1, 0,
        1, -1, -1, 1, 0, 0, 1, 1,    1,  1, 1, 1, 0, 0, 0, 0,     1, -1, 1, 1, 0, 0, 0, 1,

        -1, -1, -1, 0, -1, 0, 0, 1,   1, -1, -1, 0, -1, 0, 1, 1,    1, -1, 1, 0, -1, 0, 1, 0,
        1, -1, 1, 0, -1, 0, 1, 0,     -1, -1, 1, 0, -1, 0, 0, 0,    -1, -1, -1, 0, -1, 0, 0, 1,

        -1, 1, -1, 0, 1, 0, 0, 0,    1, 1, 1, 0, 1, 0, 1, 1,      1, 1, -1, 0, 1, 0, 1, 0,
        1,  1, 1, 0, 1, 0, 1, 1,    -1, 1, -1, 0, 1, 0, 0, 0,    -1, 1, 1, 0, 1, 0, 0, 1
    };
  
    uint32_t indices[36];
    std::iota(indices, indices + 36, 0);

    ph::AssetManager asset_manager;

    ph::Mesh::CreateInfo triangle_info;
    triangle_info.ctx = vulkan_context;
    triangle_info.vertices = vertices;
    triangle_info.vertex_count = 36;
    triangle_info.vertex_size = 8;
    triangle_info.indices = indices;
    triangle_info.index_count = 36;
    ph::Handle<ph::Mesh> cube = asset_manager.add_mesh(triangle_info);

    int w, h, channels;
    uint8_t* img = stbi_load("data/textures/blank.png", &w, &h, &channels, STBI_rgb_alpha);
    ph::Texture::CreateInfo tex_info;
    tex_info.ctx = vulkan_context;
    tex_info.channels = channels;
    tex_info.format = vk::Format::eR8G8B8A8Srgb;
    tex_info.width = w;
    tex_info.height = h;
    tex_info.data = img;
    ph::Handle<ph::Texture> pengu = asset_manager.add_texture(tex_info);
    stbi_image_free(img);

    ph::Material default_material;
    default_material.texture = pengu;

    logger.write_fmt(ph::log::Severity::Info, "Loaded assets");

    Scene scene;
    // Default values
    scene.light.position = glm::vec3(0, 2, 4);
    scene.light.ambient = glm::vec3(34.0f/255.0f, 34.0f/255.0f, 34.0f/255.0f);
    scene.light.diffuse = glm::vec3(185.0f/255.0f, 194.0f/255.0f, 32.0f/255.0f);
    scene.light.specular = glm::vec3(1, 1, 1);

    float rotation = 0;

    present_manager.add_color_attachment("color1");

    while(window_context->is_open()) {
        window_context->poll_events();

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)window_context->width / (float)window_context->height, 
        0.1f, 100.0f);
        projection[1][1] *= -1;
        glm::vec3 cam_pos = glm::vec3(2, 2, 2);
        glm::mat4 view = glm::lookAt(cam_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        float rotation_speed = 20.0f;
        static float time = 0.0f;
        time += ImGui::GetIO().DeltaTime;
        rotation += ImGui::GetIO().DeltaTime * rotation_speed;

        present_manager.wait_for_available_frame();

        ///// FRAME START
        imgui_renderer.begin_frame();

        ph::FrameInfo& frame_info = present_manager.get_frame_info();
        auto offscreen_attachment = present_manager.get_attachment(frame_info, "color1");
        frame_info.offscreen_target = 
            ph::RenderTarget(vulkan_context, vulkan_context->default_render_pass, {offscreen_attachment, frame_info.depth_attachment});

        // Imgui
        make_ui(draw_calls, scene, frame_info);

        ph::RenderGraph render_graph;

        // Set materials
        render_graph.asset_manager = &asset_manager;
        render_graph.materials.push_back(&default_material);

        // Add lights
        scene.light.position.x = std::sin(time) * 2.0f;
        scene.light.position.y = 1.0f;
        scene.light.position.z = std::cos(time) * 2.0f;
        render_graph.point_lights.push_back(scene.light);

        // Add a draw command for our cube
        ph::RenderGraph::DrawCommand draw;
        draw.mesh = cube;
        draw.material_index = 0;
        glm::mat4 cube_scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        draw.instances = { 
            { cube_scale }, 
//            { glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) }
        };
        render_graph.draw_commands.push_back(draw);

        // Setup camera data
        render_graph.projection = projection;
        render_graph.view = view;
        render_graph.camera_pos = cam_pos;

        // Render frame
        renderer.render_frame(frame_info, render_graph);
        imgui_renderer.render_frame(frame_info);

        present_manager.present_frame(frame_info);

        draw_calls = frame_info.draw_calls;

        ///// FRAME END
    }

    logger.write_fmt(ph::log::Severity::Info, "Exiting");

    // Wait until everything is done before deallocating
    vulkan_context->device.waitIdle();

    // Deallocate resources
    asset_manager.destroy_all();
    renderer.destroy();
    present_manager.destroy();
    imgui_renderer.destroy();
    vulkan_context->destroy();
    mimas_destroy_window(window_context->handle);
    mimas_terminate();
    delete vulkan_context;
}