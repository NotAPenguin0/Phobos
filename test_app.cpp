#include <phobos/core/window_context.hpp>
#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/present_manager.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/renderer/texture.hpp>
#include <phobos/util/log_interface.hpp>

#include <phobos/renderer/render_pass.hpp>

#include <stb/stb_image.h>

#include <mimas/mimas.h>
#include <mimas/mimas_vk.h>

#include <numeric>
#include <iostream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui/imgui.h>

#include <imgui/imgui_impl_mimas.h>
#include <imgui/imgui_impl_phobos.h>

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
//        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
//        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
//                         ImGuiDockNodeFlags_None);
    }

    static bool show_stats = true;
    if (ImGui::Begin("Renderer stats", &show_stats)) {
//        ImGui::PushFont(io.FontDefault);
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
    ImVec2 size;
    if (ImGui::Begin("Scene", &show_scene)) {
        auto& img = info.present_manager->get_attachment("color1");
        auto& depth = info.present_manager->get_attachment("depth1");
        size = ImGui::GetWindowSize();
        img.resize(size.x, size.y);
        depth.resize(size.x, size.y);
        ImGui::Image(img.get_imgui_tex_id(), ImVec2(img.get_width(), img.get_height()));
    }

    ImGui::End();

    if (ImGui::Begin("Color2", &show_scene)) {
        auto& img = info.present_manager->get_attachment("color2");
        img.resize(size.x, size.y);
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

static void load_imgui_fonts(ph::VulkanContext& ctx, vk::CommandPool command_pool) {
    vk::CommandBufferAllocateInfo buf_info;
    buf_info.commandBufferCount = 1;
    buf_info.commandPool = command_pool;
    buf_info.level = vk::CommandBufferLevel::ePrimary;
    vk::CommandBuffer command_buffer = ctx.device.allocateCommandBuffers(buf_info)[0];

    vk::CommandBufferBeginInfo begin_info = {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    command_buffer.begin(begin_info);
    
    ImGui_ImplPhobos_CreateFontsTexture(command_buffer);

    vk::SubmitInfo end_info = {};
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    command_buffer.end();

    ctx.graphics_queue.submit(end_info, nullptr);

    ctx.device.waitIdle();
    ImGui_ImplPhobos_DestroyFontUploadObjects();
    ctx.device.freeCommandBuffers(command_pool, command_buffer);
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
//    ph::ImGuiRenderer imgui_renderer(*window_context, *vulkan_context);
    ImGui_ImplMimas_InitForVulkan(window_context->handle);
    ImGui_ImplPhobos_InitInfo init_info;
    init_info.context = vulkan_context;
    init_info.max_frames_in_flight = 2; // TODO: better
    ImGui_ImplPhobos_Init(&init_info);
    io.Fonts->AddFontDefault();
    vk::CommandPoolCreateInfo command_pool_info;
    command_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    vk::CommandPool command_pool = vulkan_context->device.createCommandPool(command_pool_info);
    load_imgui_fonts(*vulkan_context, command_pool);
    vulkan_context->device.destroyCommandPool(command_pool);
    ImGui_ImplPhobos_DestroyFontUploadObjects();


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

    ph::Mesh::CreateInfo cube_info;
    cube_info.ctx = vulkan_context;
    cube_info.vertices = vertices;
    cube_info.vertex_count = 36;
    cube_info.vertex_size = 8;
    cube_info.indices = indices;
    cube_info.index_count = 36;
    ph::Mesh cube(cube_info);

    int w, h, channels;
    uint8_t* img = stbi_load("data/textures/blank.png", &w, &h, &channels, STBI_rgb_alpha);
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

    Scene scene;
    // Default values
    scene.light.position = glm::vec3(0, 2, 4);
    scene.light.ambient = glm::vec3(34.0f/255.0f, 34.0f/255.0f, 34.0f/255.0f);
    scene.light.diffuse = glm::vec3(185.0f/255.0f, 194.0f/255.0f, 32.0f/255.0f);
    scene.light.specular = glm::vec3(1, 1, 1);

    float rotation = 0;

    present_manager.add_color_attachment("color1");
    present_manager.add_color_attachment("color2");
    present_manager.add_depth_attachment("depth1");

    while(window_context->is_open()) {
        window_context->poll_events();

        float rotation_speed = 20.0f;
        static float time = 0.0f;
        time += ImGui::GetIO().DeltaTime;
        rotation += ImGui::GetIO().DeltaTime * rotation_speed;

        present_manager.wait_for_available_frame();

        ///// FRAME START
//        imgui_renderer.begin_frame();
        ImGui_ImplPhobos_NewFrame();
        ImGui_ImplMimas_NewFrame();
        ImGui::NewFrame();

        ph::FrameInfo& frame_info = present_manager.get_frame_info();

        // Imgui
        make_ui(draw_calls, scene, frame_info);

        scene.light.position.x = std::sin(time) * 2.0f;
        scene.light.position.y = 1.0f;
        scene.light.position.z = std::cos(time) * 2.0f;

        // Create RenderGraph
        ph::RenderGraph render_graph(vulkan_context);

        auto& offscreen_attachment = present_manager.get_attachment("color1");
        auto& color_attachment2 = present_manager.get_attachment("color2");
        auto& depth_attachment = present_manager.get_attachment("depth1");

        // Add materials
        render_graph.materials.push_back(default_material);
        // Add lights
        render_graph.point_lights.push_back(scene.light);

        // Setup camera data
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
            (float)offscreen_attachment.get_width() / (float)offscreen_attachment.get_height(), 
            0.1f, 100.0f);
        projection[1][1] *= -1;
        glm::vec3 cam_pos = glm::vec3(2, 2, 2);
        glm::mat4 view = glm::lookAt(cam_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        render_graph.projection = projection;
        render_graph.view = view;
        render_graph.camera_pos = cam_pos;

        vk::ClearColorValue clear_color = vk::ClearColorValue { std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0F}} };
        vk::ClearDepthStencilValue clear_depth = vk::ClearDepthStencilValue { 1.0f, 0 };

        // Main render pass. This render pass renders the scene to an offscreen texture
        ph::RenderPass main_pass;
        main_pass.sampled_attachments = {};
        main_pass.outputs.push_back(offscreen_attachment);
        main_pass.outputs.push_back(depth_attachment);
        main_pass.clear_values.push_back(clear_color);
        main_pass.clear_values.push_back(clear_depth);
    
        // Add a draw command for the cube
        ph::RenderPass::DrawCommand draw;
        draw.mesh = &cube;
        draw.material_index = 0;
        glm::mat4 cube_transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        main_pass.transforms.push_back(cube_transform);
        main_pass.draw_commands.push_back(draw);

        // This is the most basic callback to get something rendered.
        main_pass.callback = [&frame_info, &renderer](ph::CommandBuffer& cmd_buf) {
            renderer.execute_draw_commands(frame_info, cmd_buf);
        };

        // Send the renderpass to the graph
        render_graph.add_pass(stl::move(main_pass));

        ph::RenderPass second_pass;
        second_pass.sampled_attachments = {};
        second_pass.outputs.push_back(color_attachment2);
        second_pass.outputs.push_back(depth_attachment);
        second_pass.clear_values.push_back(clear_color);
        second_pass.clear_values.push_back(clear_depth);

        ph::RenderPass::DrawCommand draw2;
        draw2.mesh = &cube;
        draw2.material_index = 0;
        glm::mat4 transform_2 = glm::scale(glm::mat4(1.0), glm::vec3(1, 1, 1));
        second_pass.transforms.push_back(transform_2);
        second_pass.transforms.push_back(glm::translate(transform_2, glm::vec3(0, 1, 0)));
        second_pass.draw_commands.push_back(draw2);
        second_pass.draw_commands.push_back(draw2);
        second_pass.callback = [&frame_info, &renderer](ph::CommandBuffer& cmd_buf) {
            renderer.execute_draw_commands(frame_info, cmd_buf);
        };

        render_graph.add_pass(stl::move(second_pass));

        ImGui::Render();
        ImGui_ImplPhobos_RenderDrawData(ImGui::GetDrawData(), &frame_info, &render_graph, &renderer);

        // Build the rendergraph. This creates resources like vkFramebuffers and a vkRenderPass for each ph::RenderPass
        render_graph.build();

        // Render frame
        frame_info.render_graph = &render_graph;
        renderer.render_frame(frame_info);

        present_manager.present_frame(frame_info);

        draw_calls = frame_info.draw_calls;

        ///// FRAME END
    }

    logger.write_fmt(ph::log::Severity::Info, "Exiting");

    // Wait until everything is done before deallocating
    vulkan_context->device.waitIdle();

    // Deallocate resources
    pengu.destroy();
    cube.destroy();
    renderer.destroy();
    present_manager.destroy();
    ImGui_ImplPhobos_Shutdown();
//    imgui_renderer.destroy();
    vulkan_context->destroy();
    mimas_destroy_window(window_context->handle);
    mimas_terminate();
    delete vulkan_context;
}