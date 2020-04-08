#include "imgui_impl_phobos.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <phobos/core/vulkan_context.hpp>

#include <phobos/renderer/render_graph.hpp>
#include <phobos/renderer/renderer.hpp>

#include <phobos/present/frame_info.hpp>
#include <phobos/present/present_manager.hpp>

#include <phobos/util/buffer_util.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/util/shader_util.hpp>

#include <phobos/pipeline/shader_info.hpp>

/* Notes
 * In ImGui_ImplPhobos, an ImTextureID is simply a VkImageView handle.
 * To obtain a handle, you can call ImGui_ImplPhobos_AddTexture() or simply do
 * reinterpret_cast<ImTextureID>(my_vk_image_view);
 */

// TODO: Custom VkAllocationCallbacks

static vk::Sampler g_FontSampler;
static vk::Image g_FontImage;
static vk::ImageView g_FontView;
static vk::DeviceMemory g_FontMemory;
static vk::Buffer g_UploadBuffer;
static vk::DeviceMemory g_UploadBufferMemory;
static vk::DescriptorPool g_DescriptorPool;
static ph::VulkanContext* g_Context;

namespace {
struct ImGui_ImplPhobos_Buffer {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    vk::DeviceSize size = 0;
};
}

static stl::vector<ImGui_ImplPhobos_Buffer> g_VertexBuffers;
static stl::vector<ImGui_ImplPhobos_Buffer> g_IndexBuffers;


// glsl_shader.vert, compiled with:
// # glslangValidator -V -x -o glsl_shader.vert.u32 glsl_shader.vert
/*
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(push_constant) uniform uPushConstant { vec2 uScale; vec2 uTranslate; } pc;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;

void main()
{
    Out.Color = aColor;
    Out.UV = aUV;
    gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
}
*/
static uint32_t __glsl_shader_vert_spv[] =
{
    0x07230203,0x00010000,0x00080001,0x0000002e,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x000a000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000b,0x0000000f,0x00000015,
    0x0000001b,0x0000001c,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
    0x00000000,0x00030005,0x00000009,0x00000000,0x00050006,0x00000009,0x00000000,0x6f6c6f43,
    0x00000072,0x00040006,0x00000009,0x00000001,0x00005655,0x00030005,0x0000000b,0x0074754f,
    0x00040005,0x0000000f,0x6c6f4361,0x0000726f,0x00030005,0x00000015,0x00565561,0x00060005,
    0x00000019,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000019,0x00000000,
    0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x0000001b,0x00000000,0x00040005,0x0000001c,
    0x736f5061,0x00000000,0x00060005,0x0000001e,0x73755075,0x6e6f4368,0x6e617473,0x00000074,
    0x00050006,0x0000001e,0x00000000,0x61635375,0x0000656c,0x00060006,0x0000001e,0x00000001,
    0x61725475,0x616c736e,0x00006574,0x00030005,0x00000020,0x00006370,0x00040047,0x0000000b,
    0x0000001e,0x00000000,0x00040047,0x0000000f,0x0000001e,0x00000002,0x00040047,0x00000015,
    0x0000001e,0x00000001,0x00050048,0x00000019,0x00000000,0x0000000b,0x00000000,0x00030047,
    0x00000019,0x00000002,0x00040047,0x0000001c,0x0000001e,0x00000000,0x00050048,0x0000001e,
    0x00000000,0x00000023,0x00000000,0x00050048,0x0000001e,0x00000001,0x00000023,0x00000008,
    0x00030047,0x0000001e,0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
    0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040017,
    0x00000008,0x00000006,0x00000002,0x0004001e,0x00000009,0x00000007,0x00000008,0x00040020,
    0x0000000a,0x00000003,0x00000009,0x0004003b,0x0000000a,0x0000000b,0x00000003,0x00040015,
    0x0000000c,0x00000020,0x00000001,0x0004002b,0x0000000c,0x0000000d,0x00000000,0x00040020,
    0x0000000e,0x00000001,0x00000007,0x0004003b,0x0000000e,0x0000000f,0x00000001,0x00040020,
    0x00000011,0x00000003,0x00000007,0x0004002b,0x0000000c,0x00000013,0x00000001,0x00040020,
    0x00000014,0x00000001,0x00000008,0x0004003b,0x00000014,0x00000015,0x00000001,0x00040020,
    0x00000017,0x00000003,0x00000008,0x0003001e,0x00000019,0x00000007,0x00040020,0x0000001a,
    0x00000003,0x00000019,0x0004003b,0x0000001a,0x0000001b,0x00000003,0x0004003b,0x00000014,
    0x0000001c,0x00000001,0x0004001e,0x0000001e,0x00000008,0x00000008,0x00040020,0x0000001f,
    0x00000009,0x0000001e,0x0004003b,0x0000001f,0x00000020,0x00000009,0x00040020,0x00000021,
    0x00000009,0x00000008,0x0004002b,0x00000006,0x00000028,0x00000000,0x0004002b,0x00000006,
    0x00000029,0x3f800000,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
    0x00000005,0x0004003d,0x00000007,0x00000010,0x0000000f,0x00050041,0x00000011,0x00000012,
    0x0000000b,0x0000000d,0x0003003e,0x00000012,0x00000010,0x0004003d,0x00000008,0x00000016,
    0x00000015,0x00050041,0x00000017,0x00000018,0x0000000b,0x00000013,0x0003003e,0x00000018,
    0x00000016,0x0004003d,0x00000008,0x0000001d,0x0000001c,0x00050041,0x00000021,0x00000022,
    0x00000020,0x0000000d,0x0004003d,0x00000008,0x00000023,0x00000022,0x00050085,0x00000008,
    0x00000024,0x0000001d,0x00000023,0x00050041,0x00000021,0x00000025,0x00000020,0x00000013,
    0x0004003d,0x00000008,0x00000026,0x00000025,0x00050081,0x00000008,0x00000027,0x00000024,
    0x00000026,0x00050051,0x00000006,0x0000002a,0x00000027,0x00000000,0x00050051,0x00000006,
    0x0000002b,0x00000027,0x00000001,0x00070050,0x00000007,0x0000002c,0x0000002a,0x0000002b,
    0x00000028,0x00000029,0x00050041,0x00000011,0x0000002d,0x0000001b,0x0000000d,0x0003003e,
    0x0000002d,0x0000002c,0x000100fd,0x00010038
};

// glsl_shader.frag, compiled with:
// # glslangValidator -V -x -o glsl_shader.frag.u32 glsl_shader.frag
/*
#version 450 core
layout(location = 0) out vec4 fColor;
layout(set=0, binding=0) uniform sampler2D sTexture;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
void main()
{
    fColor = In.Color * texture(sTexture, In.UV.st);
}
*/
static uint32_t __glsl_shader_frag_spv[] =
{
    0x07230203,0x00010000,0x00080001,0x0000001e,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000d,0x00030010,
    0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
    0x00000000,0x00040005,0x00000009,0x6c6f4366,0x0000726f,0x00030005,0x0000000b,0x00000000,
    0x00050006,0x0000000b,0x00000000,0x6f6c6f43,0x00000072,0x00040006,0x0000000b,0x00000001,
    0x00005655,0x00030005,0x0000000d,0x00006e49,0x00050005,0x00000016,0x78655473,0x65727574,
    0x00000000,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000d,0x0000001e,
    0x00000000,0x00040047,0x00000016,0x00000022,0x00000000,0x00040047,0x00000016,0x00000021,
    0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
    0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,
    0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,0x0000000a,0x00000006,
    0x00000002,0x0004001e,0x0000000b,0x00000007,0x0000000a,0x00040020,0x0000000c,0x00000001,
    0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000001,0x00040015,0x0000000e,0x00000020,
    0x00000001,0x0004002b,0x0000000e,0x0000000f,0x00000000,0x00040020,0x00000010,0x00000001,
    0x00000007,0x00090019,0x00000013,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,
    0x00000001,0x00000000,0x0003001b,0x00000014,0x00000013,0x00040020,0x00000015,0x00000000,
    0x00000014,0x0004003b,0x00000015,0x00000016,0x00000000,0x0004002b,0x0000000e,0x00000018,
    0x00000001,0x00040020,0x00000019,0x00000001,0x0000000a,0x00050036,0x00000002,0x00000004,
    0x00000000,0x00000003,0x000200f8,0x00000005,0x00050041,0x00000010,0x00000011,0x0000000d,
    0x0000000f,0x0004003d,0x00000007,0x00000012,0x00000011,0x0004003d,0x00000014,0x00000017,
    0x00000016,0x00050041,0x00000019,0x0000001a,0x0000000d,0x00000018,0x0004003d,0x0000000a,
    0x0000001b,0x0000001a,0x00050057,0x00000007,0x0000001c,0x00000017,0x0000001b,0x00050085,
    0x00000007,0x0000001d,0x00000012,0x0000001c,0x0003003e,0x00000009,0x0000001d,0x000100fd,
    0x00010038
};


static void ImGui_ImplPhobos_CreateSampler(ph::VulkanContext* ctx) {
    vk::SamplerCreateInfo info;
    info.magFilter = vk::Filter::eLinear;
    info.minFilter = vk::Filter::eLinear;
    info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    info.addressModeU = vk::SamplerAddressMode::eRepeat;
    info.addressModeV = vk::SamplerAddressMode::eRepeat;
    info.addressModeW = vk::SamplerAddressMode::eRepeat;
    info.minLod = -1000;
    info.maxLod = 1000;
    info.maxAnisotropy = 1.0f;
    g_FontSampler = ctx->device.createSampler(info);
}


static void ImGui_ImplPhobos_CreatePipeline(ph::VulkanContext* ctx) {
    ph::PipelineCreateInfo pci;

    pci.shaders.emplace_back(std::vector<uint32_t>(&__glsl_shader_vert_spv[0], __glsl_shader_vert_spv + IM_ARRAYSIZE(__glsl_shader_vert_spv)),
        "main", vk::ShaderStageFlagBits::eVertex);
    pci.shaders.emplace_back(std::vector<uint32_t>(&__glsl_shader_frag_spv[0], __glsl_shader_frag_spv + IM_ARRAYSIZE(__glsl_shader_frag_spv)),
        "main", vk::ShaderStageFlagBits::eFragment);

    ph::reflect_shaders(pci);

    pci.vertex_attributes.resize(3);
    pci.vertex_attributes[0].location = 0;
    pci.vertex_attributes[0].binding = 0;
    pci.vertex_attributes[0].format = vk::Format::eR32G32Sfloat;
    pci.vertex_attributes[0].offset = IM_OFFSETOF(ImDrawVert, pos);
    pci.vertex_attributes[1].location = 1;
    pci.vertex_attributes[1].binding = 0;
    pci.vertex_attributes[1].format = vk::Format::eR32G32Sfloat;
    pci.vertex_attributes[1].offset = IM_OFFSETOF(ImDrawVert, uv);
    pci.vertex_attributes[2].location = 2;
    pci.vertex_attributes[2].binding = 0;
    pci.vertex_attributes[2].format = vk::Format::eR8G8B8A8Unorm;
    pci.vertex_attributes[2].offset = IM_OFFSETOF(ImDrawVert, col);
    pci.vertex_input_binding.inputRate = vk::VertexInputRate::eVertex;
    pci.vertex_input_binding.stride = sizeof(ImDrawVert);

    pci.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

    pci.viewports.emplace_back();
    pci.scissors.emplace_back();

    pci.blend_logic_op_enable = false;
    pci.blend_attachments.emplace_back();
    auto& blend = pci.blend_attachments.back();
    blend.blendEnable = true;
    blend.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blend.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blend.colorBlendOp = vk::BlendOp::eAdd;
    blend.srcAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    blend.alphaBlendOp = vk::BlendOp::eAdd;
    blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    pci.dynamic_states.push_back(vk::DynamicState::eViewport);
    pci.dynamic_states.push_back(vk::DynamicState::eScissor);

    ctx->pipelines.create_named_pipeline("ImGui_ImplPhobos_pipeline", stl::move(pci));
}

static void ImGui_ImplPhobos_CreateDescriptorPool(ImGui_ImplPhobos_InitInfo* init_info) {
    vk::DescriptorPoolSize pool_sizes[] = {
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
    };
    vk::DescriptorPoolCreateInfo pool_info = {};
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    g_DescriptorPool = init_info->context->device.createDescriptorPool(pool_info);
}

void ImGui_ImplPhobos_InitBuffers(ImGui_ImplPhobos_InitInfo* init_info) {
    g_VertexBuffers.resize(init_info->max_frames_in_flight);
    g_IndexBuffers.resize(init_info->max_frames_in_flight);
}


void ImGui_ImplPhobos_Init(ImGui_ImplPhobos_InitInfo* init_info) {
    g_Context = init_info->context;
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_phobos";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    ImGui_ImplPhobos_CreateSampler(init_info->context);
    ImGui_ImplPhobos_CreatePipeline(init_info->context);
    ImGui_ImplPhobos_InitBuffers(init_info);
    ImGui_ImplPhobos_CreateDescriptorPool(init_info);
}

void ImGui_ImplPhobos_EnsureBufferSize(ImGui_ImplPhobos_Buffer& buffer, vk::DeviceSize size, vk::BufferUsageFlags usage) {
    // Don't resize the buffer if it already has enough capacity
    if (buffer.size >= size) { return; }

    if (buffer.buffer) {
        g_Context->device.destroyBuffer(buffer.buffer);
        g_Context->device.freeMemory(buffer.memory);
    }

    // Use the available phobos utilites because we're implementing
    // a phobos backend anyway
    ph::create_buffer(*g_Context, size, usage, vk::MemoryPropertyFlagBits::eHostVisible, buffer.buffer, buffer.memory);
    buffer.size = size;
}


void ImGui_ImplPhobos_UpdateBuffers(ImGui_ImplPhobos_Buffer& vertex, ImGui_ImplPhobos_Buffer& index, ImDrawData* draw_data) {
    // Map the buffer memory
    ImDrawVert* vtx_mem = (ImDrawVert*)g_Context->device.mapMemory(vertex.memory, 0, VK_WHOLE_SIZE);
    ImDrawIdx* idx_mem = (ImDrawIdx*)g_Context->device.mapMemory(index.memory, 0, VK_WHOLE_SIZE);

    for (size_t i = 0; i < (size_t)draw_data->CmdListsCount; ++i) {
        ImDrawList const* cmd_list = draw_data->CmdLists[i];
        // Copy data
        memcpy(vtx_mem, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_mem, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        // Advance pointer
        vtx_mem += cmd_list->VtxBuffer.Size;
        idx_mem += cmd_list->IdxBuffer.Size;
    }

    // Flush the mapped memory range
    vk::MappedMemoryRange ranges[2];
    ranges[0].memory = vertex.memory;
    ranges[0].size = VK_WHOLE_SIZE;
    ranges[1].memory = index.memory;
    ranges[1].size = VK_WHOLE_SIZE;
    g_Context->device.flushMappedMemoryRanges(2, ranges);

    // Unmap memory
    g_Context->device.unmapMemory(vertex.memory);
    g_Context->device.unmapMemory(index.memory);
}

void ImGui_ImplPhobos_RenderDrawData(ImDrawData* draw_data, ph::FrameInfo* frame, ph::RenderGraph* render_graph, ph::Renderer* renderer) {
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0 || draw_data->TotalVtxCount == 0) {
        return;
    }

    ImGui_ImplPhobos_Buffer& vertex_buffer = g_VertexBuffers[frame->frame_index];
    ImGui_ImplPhobos_Buffer& index_buffer = g_IndexBuffers[frame->frame_index];

    size_t const vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    size_t const index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    ImGui_ImplPhobos_EnsureBufferSize(vertex_buffer, vertex_size, vk::BufferUsageFlagBits::eVertexBuffer);
    ImGui_ImplPhobos_EnsureBufferSize(index_buffer, index_size, vk::BufferUsageFlagBits::eIndexBuffer);

    ImGui_ImplPhobos_UpdateBuffers(vertex_buffer, index_buffer, draw_data);

    ph::RenderPass render_pass;
    render_pass.debug_name = "Imgui - Renderpass";

    ph::RenderAttachment swapchain = frame->present_manager->get_swapchain_attachment(*frame);
    // TODO: Make this more customizable? => Probably just pass it as a parameter to ImGui_ImplPhobos_RenderDrawData
    render_pass.outputs.push_back(swapchain);
    vk::ClearColorValue clear_color = vk::ClearColorValue { std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0F}} };
    render_pass.clear_values.push_back(clear_color);
    
    // Loop through all attachments and check if this frame samples from them.
    // We need to do this to fill out the render_pass.sampled_attachments field to ensure these images are properly transitioned
    for (auto const&[name, attachment] : frame->present_manager->get_all_attachments()) {
        for (size_t i = 0; i < (size_t)draw_data->CmdListsCount; ++i) {
            bool found = false;
            ImDrawList const* draw_list = draw_data->CmdLists[i];
            for (size_t cmd_i = 0; cmd_i < (size_t)draw_list->CmdBuffer.Size; ++cmd_i) {
                ImDrawCmd const* cmd = &draw_list->CmdBuffer[cmd_i];
                if (cmd->TextureId == attachment.get_imgui_tex_id()) {
                    render_pass.sampled_attachments.push_back(attachment);
                    found = true;
                    break;
                }
            }
            if (found) { break; }
        }
    }

    render_pass.callback = [draw_data, frame, renderer, fb_width, fb_height](ph::CommandBuffer& cmd_buf) {
        ph::RenderPass& pass = *cmd_buf.get_active_renderpass();
        ph::Pipeline pipeline = renderer->get_pipeline("ImGui_ImplPhobos_pipeline", pass);
        ph::ShaderInfo const& shader_info = g_Context->pipelines.get_named_pipeline("ImGui_ImplPhobos_pipeline")->shader_info;

        cmd_buf.bind_pipeline(pipeline);
        ImGui_ImplPhobos_Buffer& vtx_buffer = g_VertexBuffers[frame->frame_index];
        ImGui_ImplPhobos_Buffer& idx_buffer = g_IndexBuffers[frame->frame_index];

        cmd_buf.bind_vertex_buffer(0, vtx_buffer.buffer);
        cmd_buf.bind_index_buffer(idx_buffer.buffer, 0, sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
        vk::Viewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = (float)fb_width;
        viewport.height = (float)fb_height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmd_buf.set_viewport(viewport);

        float scale[2];
        scale[0] = 2.0f / draw_data->DisplaySize.x;
        scale[1] = 2.0f / draw_data->DisplaySize.y;
        float translate[2];
        translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
        translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];
        cmd_buf.push_constants(vk::ShaderStageFlagBits::eVertex, 0, sizeof(float) * 2, scale);
        cmd_buf.push_constants(vk::ShaderStageFlagBits::eVertex, sizeof(float) * 2, sizeof(float) * 2, translate);

        ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewportManager
        ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

        // Render command lists
        size_t global_vtx_offset = 0;
        size_t global_idx_offset = 0;
        for (size_t i = 0; i < (size_t)draw_data->CmdListsCount; ++i) {
            ImDrawList const* cmd_list = draw_data->CmdLists[i];
            for (size_t cmd_i = 0; cmd_i < (size_t)cmd_list->CmdBuffer.Size; ++cmd_i) {
                ImDrawCmd const* cmd = &cmd_list->CmdBuffer[cmd_i];
                // We don't support user callbacks yet
                ImVec4 clip_rect;
                clip_rect.x = (cmd->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (cmd->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (cmd->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (cmd->ClipRect.w - clip_off.y) * clip_scale.y;

                if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                    // Negative offsets are illegal for vkCmdSetScissor
                    if (clip_rect.x < 0.0f)
                        clip_rect.x = 0.0f;
                    if (clip_rect.y < 0.0f)
                        clip_rect.y = 0.0f;

                    vk::Rect2D scissor;
                    scissor.offset.x = (int32_t)(clip_rect.x);
                    scissor.offset.y = (int32_t)(clip_rect.y);
                    scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
                    scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
                    
                    cmd_buf.set_scissor(scissor);
                    // Bind descriptorset with font or user texture
                    vk::ImageView img_view = static_cast<vk::ImageView>(reinterpret_cast<VkImageView>(cmd->TextureId));
                    // Get descriptor set from the renderer
                    ph::DescriptorSetBinding descriptor_set;
                    descriptor_set.pool = g_DescriptorPool;
                    descriptor_set.bindings.push_back(ph::make_descriptor(shader_info["sTexture"], img_view, g_FontSampler));
                    vk::DescriptorSet descr_set = renderer->get_descriptor(*frame, pass, descriptor_set);

                    cmd_buf.bind_descriptor_set(0, descr_set)
                           .draw_indexed(cmd->ElemCount, 1, cmd->IdxOffset + global_idx_offset, cmd->VtxOffset + global_vtx_offset, 0);
                }
            }       
            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }
    };

    render_graph->add_pass(stl::move(render_pass));
}

bool ImGui_ImplPhobos_CreateFontsTexture(vk::CommandBuffer cmd_buf) {
    ImGuiIO& io = ImGui::GetIO();

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t upload_size = width * height * 4 * sizeof(char);

    ph::create_image(*g_Context, width, height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal,
        g_FontImage, g_FontMemory);
    g_FontView = ph::create_image_view(g_Context->device, g_FontImage, vk::Format::eR8G8B8A8Unorm);

    // Store our identifier
    io.Fonts->TexID = reinterpret_cast<void*>(static_cast<VkImageView>(g_FontView));

    // Create the Upload Buffer:
    ph::create_buffer(*g_Context, upload_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible, g_UploadBuffer, g_UploadBufferMemory);
    // Upload to buffer
    void* data = g_Context->device.mapMemory(g_UploadBufferMemory, 0, upload_size);
    memcpy(data, pixels, upload_size);
    vk::MappedMemoryRange range;
    range.memory = g_UploadBufferMemory;
    range.size = upload_size;
    g_Context->device.flushMappedMemoryRanges(1, &range);
    g_Context->device.unmapMemory(g_UploadBufferMemory);

    // Copy to image
    ph::transition_image_layout(cmd_buf, g_FontImage, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    ph::copy_buffer_to_image(cmd_buf, g_UploadBuffer, g_FontImage, vk::ImageLayout::eTransferDstOptimal, width, height);
    ph::transition_image_layout(cmd_buf, g_FontImage, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    return true;
}

void ImGui_ImplPhobos_DestroyFontUploadObjects() {
    if (g_UploadBuffer) {
        g_Context->device.destroyBuffer(g_UploadBuffer);
        g_UploadBuffer = nullptr;
    }
    if (g_UploadBufferMemory) {
        g_Context->device.freeMemory(g_UploadBufferMemory);
        g_UploadBufferMemory = nullptr;
    }
}

ImTextureID ImGui_ImplPhobos_AddTexture(vk::ImageView image_view, vk::ImageLayout image_layout) {
    return reinterpret_cast<void*>(static_cast<VkImageView>(image_view));
}

void ImGui_ImplPhobos_RemoveTexture(ImTextureID tex_id) {
    // TODO: Queue for deletion?
}

void ImGui_ImplPhobos_Shutdown() {
    ImGui_ImplPhobos_DestroyFontUploadObjects();
    g_Context->device.destroySampler(g_FontSampler);
    g_Context->device.destroyImage(g_FontImage);
    g_Context->device.destroyImageView(g_FontView);
    g_Context->device.freeMemory(g_FontMemory);
    g_Context->device.destroyDescriptorPool(g_DescriptorPool);

    for (auto& buf : g_VertexBuffers) {
        g_Context->device.destroyBuffer(buf.buffer);
        g_Context->device.freeMemory(buf.memory);
    }
    g_VertexBuffers.clear();

    for (auto& buf : g_IndexBuffers) {
        g_Context->device.destroyBuffer(buf.buffer);
        g_Context->device.freeMemory(buf.memory);
    }
    g_IndexBuffers.clear();
}
