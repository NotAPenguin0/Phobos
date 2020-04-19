#include "test_app_framework.hpp"

#include <iostream>

#include <mimas/mimas.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_mimas.h>
#include <imgui/imgui_impl_phobos.h>
#include "imgui_style.hpp"

#include <numeric>
#include <phobos/renderer/mesh.hpp>
#include <phobos/renderer/texture.hpp>

#include <stb/stb_image.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#undef near
#undef far

namespace ph {

void DefaultLogger::write(ph::log::Severity severity, std::string_view str) {
    if (timestamp_enabled) {
        std::cout << get_timestamp_string() << ": ";
    }
    std::cout << str << std::endl;
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

TestApplication::TestApplication(size_t width, size_t height, const char* title) : width(width), height(height) {
    logger.set_timestamp(true);
    window = create_window_context(title, width, height);
    ph::AppSettings settings;
    settings.enable_validation_layers = true;
    settings.version = ph::Version{ 0, 0, 2 };
    ctx = create_vulkan_context(*window, &logger, settings);

    present = stl::make_unique<PresentManager>(*ctx);
    renderer = stl::make_unique<Renderer>(*ctx);

    // Next up is initializing ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    style_theme_grey();

    ImGui_ImplMimas_InitForVulkan(ctx->window_ctx->handle);
    ImGui_ImplPhobos_InitInfo init_info;
    init_info.context = ctx;
    init_info.max_frames_in_flight = 2; // TODO: better
    ImGui_ImplPhobos_Init(&init_info);
    io.Fonts->AddFontDefault();
    vk::CommandPoolCreateInfo command_pool_info;
    command_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    vk::CommandPool command_pool = ctx->device.createCommandPool(command_pool_info);
    load_imgui_fonts(*ctx, command_pool);
    ctx->device.destroyCommandPool(command_pool);
}

TestApplication::~TestApplication() {
    renderer->destroy();
    present->destroy();
    ImGui_ImplPhobos_Shutdown();
    ctx->destroy();
    mimas_destroy_window(window->handle);
    mimas_terminate();
    delete ctx;
}

void TestApplication::run() {
    double last_time = 0;
    while (window->is_open()) {
        window->poll_events();

        time = mimas_get_time();
        frame_time = time - last_time;
        last_time = time;

        present->wait_for_available_frame();
        ImGui_ImplMimas_NewFrame();
        ImGui::NewFrame();

        FrameInfo& frame = present->get_frame_info();
        RenderGraph render_graph { ctx };

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Dockspace", nullptr, flags);
        ImGui::PopStyleVar(3);

        // DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                ImGuiDockNodeFlags_None);
        }

        update(frame, render_graph);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplPhobos_RenderDrawData(ImGui::GetDrawData(), &frame, &render_graph, renderer.get());
        // Build the rendergraph. This creates resources like VkFramebuffers and a VkRenderPass for each ph::RenderPass.
        // Note that these resources are cached, so you don't need to worry about them being recreated every frame.
        render_graph.build();
        frame.render_graph = &render_graph;
        renderer->render_frame(frame);
        present->present_frame(frame);
    }
    ctx->device.waitIdle();
}

Mesh TestApplication::generate_cube_geometry() {
    static constexpr float vertices[] = {
        -1, -1, -1, 0, 0, -1, -1, 0, 0,  1, 0,  1, 1, -1, 0, 0, -1, -1, 0, 0,  0, 1,
        1, -1, -1, 0, 0, -1, -1, 0, 0,  0, 0,   1, 1, -1, 0, 0, -1, -1, 0, 0,  0, 1,
        -1, -1, -1, 0, 0, -1, -1, 0, 0,  1, 0,  -1, 1, -1, 0, 0, -1, -1, 0, 0,  1, 1,
        -1, -1, 1, 0, 0, 1, 1, 0.0, 0,  0, 0,   1, -1, 1, 0, 0, 1,1, 0.0, 0,  1, 0,
        1, 1, 1, 0, 0, 1, 1, 0.0, 0,  1, 1,     1, 1, 1, 0, 0, 1, 1, 0.0, 0,  1, 1,
        -1, 1, 1, 0, 0, 1, 1, 0.0, 0,  0, 1,    -1, -1, 1, 0, 0, 1, 1, 0.0, 0,  0, 0,
        -1, 1, -1, -1, 0, 0, 0, 0, 1, 0, 1,   -1, -1, -1, -1, 0, 0, 0, 0, 1, 0, 0,
        -1, 1, 1, -1, 0, 0, 0, 0, 1,  1, 1,    -1, -1, -1, -1, 0, 0, 0, 0, 1,  0, 0,
        -1, -1, 1, -1, 0, 0, 0, 0, 1,  1, 0,   -1, 1, 1, -1, 0, 0, 0, 0, 1,  1, 1,
        1, 1, 1, 1, 0, 0, 0, 0, -1, 0, 1,     1, -1, -1, 1, 0, 0, 0, 0, -1, 1, 0,
        1, 1, -1, 1, 0, 0, 0, 0, -1,  1, 1,    1, -1, -1, 1, 0, 0, 0, 0, -1,  1, 0,
        1, 1, 1, 1, 0, 0, 0, 0, -1,  0, 1,     1, -1, 1, 1, 0, 0, 0, 0, -1,  0, 0,
        -1, -1, -1, 0, -1, 0, 1, 0, 0,  0, 0, 1, -1, -1, 0, -1, 0, 1, 0, 0,  1, 0,
        1, -1, 1, 0, -1, 0, 1, 0, 0,  1, 1,   1, -1, 1, 0, -1, 0, 1, 0, 0,  1, 1,
        -1, -1, 1, 0, -1, 0, 1, 0, 0,  0, 1,  -1, -1, -1, 0, -1, 0, 1, 0, 0,  0, 0,
        -1, 1, -1, 0, 1, 0, 1, 0, 0,  0, 1,    1, 1, 1, 0, 1, 0, 1, 0, 0,  1, 0,
        1, 1, -1, 0, 1, 0, 1, 0, 0,  1, 1,     1, 1, 1, 0, 1, 0, 1, 0, 0,  1, 0,
        -1, 1, -1, 0, 1, 0, 1, 0, 0,  0, 1,    -1, 1, 1, 0, 1, 0, 1, 0, 0,  0, 0
    };
    uint32_t indices[36];
    std::iota(indices, indices + 36, 0);
    ph::Mesh::CreateInfo cube_info;
    cube_info.ctx = ctx;
    cube_info.vertices = vertices;
    cube_info.vertex_count = 36;
    cube_info.vertex_size = 11;
    cube_info.indices = indices;
    cube_info.index_count = 36;
    return ph::Mesh(cube_info);
}

Mesh TestApplication::generate_quad_geometry() {
    static constexpr float vertices[] = {
        -1, 1, 0, 0, 0, 1, 1, 0, 0,  0, 1, -1, -1, 0, 0, 0, 1, 1, 0, 0,  0, 0,
        1, -1, 0, 0, 0, 1, 1, 0, 0,  1, 0, -1, 1, 0, 0, 0, 1, 1, 0, 0,  0, 1,
        1, -1, 0, 0, 0, 1, 1, 0, 0,  1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0,  1, 1
    };
    uint32_t indices[6];
    std::iota(indices, indices + 6, 0);
    ph::Mesh::CreateInfo quad_info;
    quad_info.ctx = ctx;
    quad_info.vertices = vertices;
    quad_info.vertex_count = 6;
    quad_info.vertex_size = 11;
    quad_info.indices = indices;
    quad_info.index_count = 6;
    return ph::Mesh(quad_info);
}

Mesh TestApplication::generate_plane_geometry() {
    static constexpr float vertices[] = {
        -1, 1, 0, 0, 0, 1, 1, 0, 0,  0, 1, -1, -1, 0, 0, 0, 1, 1, 0, 0,  0, 0,
        1, -1, 0, 0, 0, 1, 1, 0, 0,  1, 0, -1, 1, 0, 0, 0, 1, 1, 0, 0,  0, 1,
        1, -1, 0, 0, 0, 1, 1, 0, 0,  1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0,  1, 1
    };
    uint32_t indices[6];
    std::iota(indices, indices + 6, 0);
    ph::Mesh::CreateInfo plane_info;
    plane_info.ctx = ctx;
    plane_info.vertices = vertices;
    plane_info.vertex_count = 6;
    plane_info.vertex_size = 11;
    plane_info.indices = indices;
    plane_info.index_count = 6;
    return ph::Mesh(plane_info);
}

static vk::Format get_image_format(int channels, bool srgb) {
    if (srgb) {
        switch (channels) {
        case 1: return vk::Format::eR8Srgb;
        case 2: return vk::Format::eR8G8Srgb;
        case 3: return vk::Format::eR8G8B8Srgb;
        case 4: return vk::Format::eR8G8B8A8Srgb;
        default: return vk::Format::eUndefined;
        }
    }
    else {
        switch (channels) {
        case 1: return vk::Format::eR8Unorm;
        case 2: return vk::Format::eR8G8Unorm;
        case 3: return vk::Format::eR8G8B8Unorm;
        case 4: return vk::Format::eR8G8B8A8Unorm;
        default: return vk::Format::eUndefined;
        }
    }
}

Texture TestApplication::load_texture(std::string_view path) {
    int w, h, channels = 4;
    uint8_t* img = stbi_load(path.data(), &w, &h, &channels, 4);
    if (!img) { throw std::runtime_error("failed to load image"); }
    ph::Texture::CreateInfo tex_info;
    tex_info.ctx = ctx;
    tex_info.channels = 4;
    tex_info.format = get_image_format(4, true);
    tex_info.width = w;
    tex_info.height = h;
    tex_info.data = img;
    ph::Texture tex(tex_info);
    stbi_image_free(img);

    return stl::move(tex);
}

Texture TestApplication::load_texture_map(std::string_view path) {
    int w, h, channels = 4;
    uint8_t* img = stbi_load(path.data(), &w, &h, &channels, 4);
    if (!img) { throw std::runtime_error("failed to load image"); }
    ph::Texture::CreateInfo tex_info;
    tex_info.ctx = ctx;
    tex_info.channels = 4;
    tex_info.format = get_image_format(4, false);
    tex_info.width = w;
    tex_info.height = h;
    tex_info.data = img;
    ph::Texture tex(tex_info);
    stbi_image_free(img);

    return stl::move(tex);
}

Cubemap TestApplication::load_cubemap(std::array<std::string_view, 6> faces) {
    int w, h, channels = 4;
    std::array<uint8_t*, 6> data;
    for (size_t i = 0; i < 6; ++i) {
        uint8_t* img = stbi_load(faces[i].data(), &w, &h, &channels, 4);
        if (!img) { throw std::runtime_error("failed to load image"); }
        data[i] = img;
    }

    ph::Cubemap::CreateInfo info;
    info.ctx = ctx;
    info.channels = 4;
    for (size_t i = 0; i < 6; ++i) { info.faces[i] = data[i]; }
    info.format = get_image_format(4, true);
    info.width = w;
    info.height = h;
    ph::Cubemap cubemap(info);
    for (auto img : data) { stbi_image_free(img); }

    return stl::move(cubemap);
}

ImVec2 TestApplication::match_attachment_to_window_size(RenderAttachment& attachment) {
    ImVec2 size = ImGui::GetWindowSize();
    attachment.resize(size.x, size.y);
    return size;
}

glm::mat4 TestApplication::projection(float fov, float near, float far, RenderAttachment const& attachment) {
    float const aspect = (float)attachment.get_width() / (float)attachment.get_height();
    return projection(fov, near, far, aspect);
}

glm::mat4 TestApplication::projection(float fov, float near, float far, float aspect) {
    glm::mat4 mat = glm::perspective(fov, aspect, near, far);
    // Invert y axis because vulkan
    mat[1][1] *= -1;
    return mat;
}

static Mesh load_mesh(VulkanContext& ctx, aiMesh* mesh) {
    Mesh::CreateInfo info;

    info.ctx = &ctx;
    info.vertex_size = 3 + 3 + 3 + 2;
    info.vertex_count = mesh->mNumVertices;
    std::vector<float> verts(info.vertex_count * info.vertex_size);
    for (size_t i = 0; i < mesh->mNumVertices; ++i) {
        size_t const index = i * info.vertex_size;
        // Position
        verts[index] = mesh->mVertices[i].x;
        verts[index + 1] = mesh->mVertices[i].y;
        verts[index + 2] = mesh->mVertices[i].z;
        // Normal
        verts[index + 3] = mesh->mNormals[i].x;
        verts[index + 4] = mesh->mNormals[i].y;
        verts[index + 5] = mesh->mNormals[i].z;
        // Tangent
        verts[index + 6] = mesh->mTangents[i].x;
        verts[index + 7] = mesh->mTangents[i].y;
        verts[index + 8] = mesh->mTangents[i].z;
        // TexCoord
        verts[index + 9] = mesh->mTextureCoords[0][i].x;
        verts[index + 10] = mesh->mTextureCoords[0][i].y;
    }
    info.vertices = verts.data();

    std::vector<uint32_t> indices;
    indices.reserve(mesh->mNumFaces * 3);
    for (stl::size_t i = 0; i < mesh->mNumFaces; ++i) {
        aiFace const& face = mesh->mFaces[i];
        for (stl::size_t j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }
    info.index_count = indices.size();
    info.indices = indices.data();

    return Mesh(info);
}

Mesh TestApplication::load_obj(std::string_view path) {
    Assimp::Importer importer;
    constexpr int postprocess = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_CalcTangentSpace;
    aiScene const* scene = importer.ReadFile(std::string(path), postprocess);
   
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode) {
        throw std::runtime_error("Failed to load model");
    }

    for (size_t i = 0; i < scene->mNumMeshes; ++i) {
        return load_mesh(*ctx, scene->mMeshes[i]);
    }

    throw std::runtime_error("model had no meshes to load!");
}

uint32_t TestApplication::add_material(Material const& material) {
    materials.push_back(material);
    return materials.size() - 1;
}

}